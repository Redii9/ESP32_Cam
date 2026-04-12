#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-CAM Sterowanie</title>
    <style>
        body { font-family: Arial; text-align: center; background: #222; color: white; }
        img { width: 100%; max-width: 600px; border: 2px solid #555; }
        .button {
            padding: 15px 30px; font-size: 18px; margin: 10px;
            cursor: pointer; background: #007bff; color: white; border: none; border-radius: 5px;
        }
        .button:active { background: #0056b3; }
    </style>
</head>
<body>
    <h2>ESP32-CAM Monitoring</h2>
    <img src="/stream" id="stream">
    <br><br>
    <button class="button" onclick="move('/left')">LEWO</button>
    <button class="button" onclick="move('/right')">PRAWO</button>

    <script>
        function move(url) {
            fetch(url).catch(error => console.error('Błąd:', error));
        }
        // Automatyczne ładowanie strumienia wideo
        window.onload = function() {
            document.getElementById('stream').src = window.location.origin + '/stream';
        }
    </script>
</body>
</html>
)=====";

#endif