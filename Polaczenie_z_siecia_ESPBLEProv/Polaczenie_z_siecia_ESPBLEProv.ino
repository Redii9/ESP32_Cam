#include "WiFi.h"
#include "WiFiProv.h"
#include <nvs_flash.h>

const char *service_name = "ESP32_CAM"; 
const char *pop = "1234567"; 

const int RESET_PIN = 0; 

void setup() {
  Serial.begin(115200);
  pinMode(RESET_PIN, INPUT_PULLUP);
  delay(1000);
  Serial.println("\nStart ESP32");

  WiFi.begin();
  int attempts = 0;
  Serial.print("Laczenie z siecia");
  
  // 5 sekund na polaczenie
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("ESP32 jest juz polaczone z Wi-Fi");
  } else {
    Serial.println("Brak zapisanej sieci lub zasiegu. Uruchamiam BLE Provisioning");
    uruchomProvisioning();
  }
}

void uruchomProvisioning() {
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
  Serial.println("Otworz aplikacje ESP BLE Prov na telefonie.");
  Serial.print("Nazwa urzadzenia: "); Serial.println(service_name);
}

void loop() {
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("\nPrzytrzymaj 3 sekundy przycisk by zresetowac siec");
    delay(3000);
    
    // Przycisk wcisniety po 3 sekundach
    if (digitalRead(RESET_PIN) == LOW) {
      Serial.println("Resetowanie ustawien sieciowych");
      nvs_flash_erase(); // czyszczenie pamieci z zapisanymi sieciami
      nvs_flash_init();
      WiFi.disconnect(true, true);
      delay(500);
      ESP.restart();
    } else {
      Serial.println("Reset anulowany");
    }
  }

  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 5000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Polaczono z siecia, adres IP to: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("ESP32 w trybie konfiguracji");
    }
    lastPrintTime = millis();
  }
}