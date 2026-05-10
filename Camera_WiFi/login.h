#ifndef LOGIN_HTML_H
#define LOGIN_HTML_H

const char login_html[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Logowanie ESP32-CAM</title>
    <style>
        body { font-family: Arial; text-align: center; background: #222; color: white; padding-top: 80px; }
        .login-box { display: inline-block; padding: 40px; border: 2px solid #555; border-radius: 10px; background: #333; }
        input[type="password"] { padding: 12px; font-size: 16px; border-radius: 5px; border: none; outline: none; text-align: center; margin-bottom: 15px; }
        button { padding: 12px 30px; font-size: 16px; cursor: pointer; background: #28a745; color: white; border: none; border-radius: 5px; }
        button:active { background: #218838; }
    </style>
</head>
<body>
    <div class="login-box">
        <h2>Wprowadz haslo</h2>
        <form action="/login" method="GET">
            <input type="password" name="pass" placeholder="Haslo Haslo" required><br>
            <button type="submit">Zaloguj</button>
        </form>
    </div>
</body>
</html>
)=====";

#endif