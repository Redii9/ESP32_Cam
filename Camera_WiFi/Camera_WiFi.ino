#include "WiFi.h"
#include "WiFiProv.h"
#include <ESPmDNS.h>
#include <nvs_flash.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "img_converters.h"
#include "Camera_pins.h"
#include <ESP32Servo.h>
#include "esp_bt.h"
#include "index.h"
#include "login.h"
#include "config.h"

#define PART_BOUNDARY "123456789000000000000987654321"
const char *service_name = "ESP32_CAM"; 
const char *pop = "1234567";
const int RESET_PIN = 0;
 
bool camera_ok = false;

// Servo
const int servoPin = 33;
#define SERVOMIN 900
#define SERVOMAX 2600
#define SERVO_FREQ 50
volatile int target_position = 90;
int current_position = 90;
Servo servo;

const char * _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
const char * _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
const char * _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

//Globalna deklaracja serwera
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// Sprawdzenie Ciasteczek 
bool is_authenticated(httpd_req_t *req) {
    size_t buf_len = httpd_req_get_hdr_value_len(req, "Cookie") + 1;
    if (buf_len > 1) {
        char* buf = (char*)malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Cookie", buf, buf_len) == ESP_OK) {
            // Szukanie ciastka o nazwie "auth=1"
            if (strstr(buf, "auth=1") != NULL) {
                free(buf);
                return true;
            }
        }
        free(buf);
    }
    return false;
}

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    if (is_authenticated(req)) {
        // Zalogowany
        return httpd_resp_send(req, index_html, strlen(index_html));
    } else {
        // Niezalogowany
        return httpd_resp_send(req, login_html, strlen(login_html));
    }
}

// Handler weryfikacji hasła (odbiera dane z formularza logowania)
static esp_err_t login_handler(httpd_req_t *req) {
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char pass[50];
        if (httpd_query_key_value(buf, "pass", pass, sizeof(pass)) == ESP_OK) {
            
            if (strcmp(pass, WEB_PASSWORD) == 0) { 
                httpd_resp_set_hdr(req, "Set-Cookie", "auth=1; Path=/; Max-Age=3600");
                httpd_resp_set_status(req, "303 See Other"); // Przekierowanie po sukcesie
                httpd_resp_set_hdr(req, "Location", "/");
                return httpd_resp_send(req, NULL, 0);
            }
        }
    }
    // Bledne haslo odswiezenie strony
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

// Handler ruchu w lewo
static esp_err_t left_handler(httpd_req_t *req) {
  // Zabezpieczenie przyciskow
  if (!is_authenticated(req)) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Brak dostepu");
  target_position += 10;
  if (target_position > 180) target_position = 180;
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, "OK", 2);
}

// Handler ruchu w prawo
static esp_err_t right_handler(httpd_req_t *req) {
  if (!is_authenticated(req)) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Brak dostepu");
  target_position -= 10;
  if (target_position < 0) target_position = 0;
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, "OK", 2);
}

//Aktualny ruch kamery zadanie dla 2 rdzenia
void servoTask(void * parameter) {
  for(;;) { // Nieskończona pętla Taska
    if (current_position != target_position) {
      current_position = target_position;
      servo.write(current_position);
      Serial.printf("Serwo rusza na: %d\n", current_position);
    }
    // Usypia wątek na 20ms, zdejmując obciążenie z procesora
    vTaskDelay(pdMS_TO_TICKS(20)); 
  }
}

static esp_err_t stream_handler(httpd_req_t * req) {
  // Zabezpieczenie strumienia kamery
  if (!is_authenticated(req)) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Brak dostepu");
  if (!camera_ok) {
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Camera not available", 20);
  }
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Uchwycenie obrazu się nie powiodło");
      continue;
    } else {
      if (fb -> width > 400) {
        if (fb -> format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, & _jpg_buf, & _jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb -> len;
          _jpg_buf = fb -> buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char * ) part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char * ) part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char * ) _jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    //Usypianie na chwile bo juz nie wyrabia
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.core_id = 0; // Rdzeń 0 dla WiFi/HTTP

    // Główne endpointy (Strona i przyciski)
    httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
    httpd_uri_t left_uri = { .uri = "/left", .method = HTTP_GET, .handler = left_handler, .user_ctx = NULL };
    httpd_uri_t right_uri = { .uri = "/right", .method = HTTP_GET, .handler = right_handler, .user_ctx = NULL };
    httpd_uri_t login_uri = { .uri = "/login", .method = HTTP_GET, .handler = login_handler, .user_ctx = NULL };

    Serial.printf("Uruchamianie serwera kontroli na porcie: %d\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &left_uri);
        httpd_register_uri_handler(camera_httpd, &right_uri);
        httpd_register_uri_handler(camera_httpd, &login_uri);
    }

    //Konfiguracja DRUGIEGO serwera dla strumienia ---
    config.server_port += 1; // Ustawia port na 81
    config.ctrl_port += 1;   // Wymagane, by instancje się nie gryzły
    
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

    Serial.printf("Uruchamianie serwera wideo na porcie: %d\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}


void initCamera(){
  
  // Cała konfiguracja i inicjalizacja kamery
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 15;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }

  //Inicjalizacja kamery i obsługa błędu przy jej braku
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera NIE działa, kod: 0x%x\n", err);
    camera_ok = false;
  } else {
    Serial.println("Kamera OK");
    camera_ok = true;
  }
}

void startProvisioning() {
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
  Serial.println("Otworz aplikacje ESP BLE Prov na telefonie.");
  Serial.print("Nazwa urzadzenia: "); Serial.println(service_name);
}



void setup() {
  Serial.begin(115200);
  pinMode(RESET_PIN, INPUT_PULLUP);
  delay(1000);
  Serial.println("\nStart ESP32");

  //Sekcja nawiązania połączenia
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin();
  int attempts = 0;
  Serial.print("Laczenie z siecia");

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Brak zapisanej sieci lub zasiegu. Uruchamiam BLE Provisioning");
    startProvisioning();

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print("*");
    }
    Serial.println("\nPołączono");
    
    //Wyłączenie Bloutooth po połączeniu
    btStop(); 
    delay(100);
    esp_bt_controller_deinit();
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    Serial.println("BLE Provisioning zatrzymany. Zwolniono zasoby.");
  }

  Serial.print("ESP32 jest połączone z Wi-Fi. IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32cam")) {
    MDNS.addService("http", "tcp", 80); 
    Serial.println("ESP32 działa pod: http://esp32cam.local");
  } else {
    Serial.println("Błąd inicjalizacji mDNS!");
  }
  
  initCamera();

  if (WiFi.status() == WL_CONNECTED) {
    startCameraServer();
    Serial.println("Serwer kamery uruchomiony.");
  }

  // Servo
  servo.setPeriodHertz(SERVO_FREQ);
  servo.attach(servoPin, SERVOMIN, SERVOMAX);
  Serial.println("Restart pozycji serwa");
  servo.write(current_position);

  // Przypięcie Taska serwa do Rdzenia 1
  xTaskCreatePinnedToCore(
    servoTask,      /* Funkcja taska */
    "ServoTask",    /* Nazwa */
    2048,           /* Rozmiar stosu w bajtach */
    NULL,           /* Parametry */
    1,              /* Priorytet (1 to niski, wystarczający) */
    NULL,           /* Uchwyt (niepotrzebny) */
    1               /* Przypnij do Rdzenia 1 */
  );
}

void loop() {
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("\nPrzytrzymaj 3 sekundy przycisk by zresetowac siec");
    delay(3000);
    if (digitalRead(RESET_PIN) == LOW) {
      Serial.println("Resetowanie ustawien sieciowych");
      nvs_flash_erase(); 
      nvs_flash_init();
      WiFi.disconnect(true, true);
      delay(500);
      ESP.restart();
    } else {
      Serial.println("Reset anulowany");
    }
  }

}
