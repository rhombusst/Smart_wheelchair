
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <string.h>

// =============== NORMAL WIFI ===============
const char* ssid = "H-417";
const char* password = "YOUR_WIFI_PASSWORD";

// =============== MOTOR PINS ===============
#define IN1 D5
#define IN2 D6
#define IN3 D7
#define IN4 D8

// =============== SENSOR PINS ===============
#define EMERGENCY_BUTTON D0
#define OBSTACLE_IR D1
#define LAND_IR D2

const int OBSTACLE_DETECTED_STATE = LOW;
const int LAND_PRESENT_STATE = LOW;

// =============== EMERGENCY BUTTON ===============
bool lastEmergencyButtonState = HIGH;
bool stableEmergencyButtonState = HIGH;
unsigned long lastEmergencyButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// =============== MOTOR SPEED ===============
int motorSpeed = 255;
const int MIN_SPEED = 100;
const int MAX_SPEED = 255;

// =============== EMAILJS SETTINGS ===============
const char* emailApiUrl =
  "https://api.emailjs.com/api/v1.0/email/send";

const char* serviceID = "service_9bixi9i";
const char* templateID = "template_xcxb0vn";
const char* publicKey = "BuzCBYQvWq_nEKdc2";
const char* recipient = "samarthtiwari153@gmail.com";

const char* emergencyLocation =
  "Rabindranath Tagore University, Bhojpur, Chiklod Road, "
  "near Bangrasiya Chouraha, Bhopal, Madhya Pradesh 464993";

// =============== SERVER ===============
WiFiServer server(80);

// =============== CONTROLLER WEBSITE ===============
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#101827">
<title>Smart Wheelchair</title>
<style>
*{box-sizing:border-box}
body{margin:0;background:#0b1220;color:#f8fafc;
font-family:Arial,sans-serif;text-align:center}
header{padding:24px 12px;background:#111c30}
h1{font-size:27px;margin:0}
p{color:#b5c3d8;line-height:1.5}
.container{max-width:480px;margin:auto;padding:14px}
.card{background:#172338;padding:18px;margin:14px 0;
border-radius:18px;border:1px solid #293a54}
.status{display:inline-block;padding:10px;margin:5px;
border-radius:10px;background:#334155;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);
gap:8px;align-items:center}
button{border:0;border-radius:14px;padding:16px 5px;
font-size:15px;font-weight:bold;background:#2563eb;color:white;
width:100%;min-height:62px;touch-action:manipulation}
.stop{background:#dc2626}
.speed{background:#475569}
.emergency{background:#b91c1c}
small{color:#94a3b8}
</style>
</head>
<body>
<header>
<h1>♿ Smart Wheelchair</h1>
<p>Wireless Control & Safety Monitor</p>
</header>
<div class="container">

<div class="card">
<h3>System Status</h3>
<div class="status" id="wifi">Wi-Fi: Checking</div>
<div class="status" id="obstacle">Obstacle: Checking</div>
<div class="status" id="land">Ground: Checking</div>
<p id="message">Controller ready</p>
</div>

<div class="card">
<h3>Movement Control</h3>
<div class="grid">
<div></div>
<button onclick="move('forward')">▲<br>Forward</button>
<div></div>
<button onclick="move('left')">◀<br>Left</button>
<button class="stop" onclick="move('stop')">■ STOP</button>
<button onclick="move('right')">▶<br>Right</button>
<div></div>
<button onclick="move('back')">▼<br>Backward</button>
<div></div>
</div>
<p><small>Movement is blocked if a safety sensor detects danger.</small></p>
</div>

<div class="card">
<h3>Motor Speed</h3>
<div class="grid" style="grid-template-columns:1fr 1fr">
<button class="speed" onclick="move('speed_down')">− Slower</button>
<button class="speed" onclick="move('speed_up')">+ Faster</button>
</div>
<p id="speed">Speed: 255</p>
</div>

<div class="card">
<h3>Emergency Assistance</h3>
<p>Stop the motors and send an emergency email.</p>
<button class="emergency" onclick="emergencyAlert()">
🚨 EMERGENCY ALERT
</button>
</div>

<div class="card">
<h3>Controller Connection</h3>
<p id="connection">Connecting...</p>
<button class="speed" onclick="refreshStatus()">Refresh Status</button>
</div>

</div>

<script>
let busy=false;

async function api(path){
const controller=new AbortController();
const timeout=setTimeout(()=>controller.abort(),5000);
try{
const r=await fetch(path,{cache:'no-store',signal:controller.signal});
return await r.text();
}finally{
clearTimeout(timeout);
}
}

async function move(command){
if(busy)return;
busy=true;
document.getElementById('message').textContent='Sending command...';
try{
document.getElementById('message').textContent=await api('/'+command);
if(command==='speed_up'||command==='speed_down')await refreshStatus();
}catch(e){
document.getElementById('message').textContent='Connection failed';
}finally{
busy=false;
}
}

async function emergencyAlert(){
if(confirm('Send emergency alert now?'))await move('emergency');
}

async function refreshStatus(){
try{
const data=JSON.parse(await api('/status'));

document.getElementById('wifi').textContent='Wi-Fi: Connected';

document.getElementById('obstacle').textContent=
data.obstacle?'Obstacle: DETECTED':'Obstacle: Clear';

document.getElementById('land').textContent=
data.land?'Ground: Detected':'Ground: MISSING';

document.getElementById('obstacle').style.background=
data.obstacle?'#991b1b':'#166534';

document.getElementById('land').style.background=
data.land?'#166534':'#991b1b';

document.getElementById('speed').textContent='Speed: '+data.speed;
document.getElementById('connection').textContent='Controller online';

if(data.obstacle||!data.land){
document.getElementById('message').textContent=
'SAFETY STOP: Check sensors';
}
}catch(e){
document.getElementById('wifi').textContent='Wi-Fi: Check connection';
document.getElementById('connection').textContent='Controller not responding';
}
}

refreshStatus();
setInterval(refreshStatus,1500);
</script>
</body>
</html>
)rawliteral";

// =============== FUNCTION DECLARATIONS ===============
void stopMotor();
void forward();
void backward();
void left();
void right();
void emergency();
void sendEmergencyEmail();
bool obstacleDetected();
bool landDetected();
bool safeToMove();
void checkSafety();
void sendText(WiFiClient &client, String body);
void sendJson(WiFiClient &client, String body);
void sendWebsite(WiFiClient &client);
String escapeJSON(const String &input);

// =============== JSON ESCAPE ===============
String escapeJSON(const String &input) {
  String output;
  output.reserve(input.length() + 16);

  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input[i];

    switch (c) {
      case '"':  output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default:
        if ((unsigned char)c >= 0x20) {
          output += c;
        }
        break;
    }
  }

  return output;
}

// =============== SENSORS ===============
bool obstacleDetected() {
  return digitalRead(OBSTACLE_IR) == OBSTACLE_DETECTED_STATE;
}

bool landDetected() {
  return digitalRead(LAND_IR) == LAND_PRESENT_STATE;
}

bool safeToMove() {
  return !obstacleDetected() && landDetected();
}

void checkSafety() {
  if (!safeToMove()) {
    stopMotor();
  }
}

// =============== HTTP RESPONSES ===============
void sendText(WiFiClient &client, String body) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/plain; charset=utf-8\r\n");
  client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(body.length());
  client.print("\r\n\r\n");
  client.print(body);
}

void sendJson(WiFiClient &client, String body) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: application/json; charset=utf-8\r\n");
  client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(body.length());
  client.print("\r\n\r\n");
  client.print(body);
}

void sendWebsite(WiFiClient &client) {
  size_t len = strlen_P(webpage);

  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html; charset=utf-8\r\n");
  client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(len);
  client.print("\r\n\r\n");

  client.write_P(webpage, len);
}

// =============== SETUP ===============
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
  pinMode(OBSTACLE_IR, INPUT);
  pinMode(LAND_IR, INPUT);

  stopMotor();

  lastEmergencyButtonState = digitalRead(EMERGENCY_BUTTON);
  stableEmergencyButtonState = lastEmergencyButtonState;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  // Keep motors stopped while connecting.
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 30000) {
    stopMotor();
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected!");
    Serial.print("Controller IP: http://");
    Serial.println(WiFi.localIP());

    server.begin();
    Serial.println("Web server started on port 80.");
  } else {
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Check SSID and password, then reset.");
  }
}

// =============== MAIN LOOP ===============
void loop() {
  // Reconnect to the configured router if disconnected.
  if (WiFi.status() != WL_CONNECTED) {
    stopMotor();

    static unsigned long lastReconnectAttempt = 0;

    if (millis() - lastReconnectAttempt >= 5000) {
      lastReconnectAttempt = millis();

      Serial.println("Wi-Fi disconnected. Reconnecting...");
      WiFi.begin(ssid, password);
    }

    delay(10);
    return;
  }

  // Physical emergency button with debounce.
  bool reading = digitalRead(EMERGENCY_BUTTON);

  if (reading != lastEmergencyButtonState) {
    lastEmergencyButtonTime = millis();
    lastEmergencyButtonState = reading;
  }

  if (millis() - lastEmergencyButtonTime >= DEBOUNCE_DELAY) {
    if (reading != stableEmergencyButtonState) {
      stableEmergencyButtonState = reading;

      if (stableEmergencyButtonState == LOW) {
        emergency();
      }
    }
  }

  // Enforce sensor safety continuously.
  checkSafety();

  WiFiClient client = server.available();
  if (!client) return;

  client.setTimeout(1500);

  unsigned long timeout = millis() + 1500;

  while (!client.available() && millis() < timeout) {
    checkSafety();
    delay(1);
  }

  if (!client.available()) {
    client.stop();
    return;
  }

  String request = client.readStringUntil('\r');
  client.readStringUntil('\n');

  Serial.println(request);

  // =============== ROUTES ===============
  if (request.startsWith("GET / ") ||
      request.startsWith("GET /index.html")) {
    sendWebsite(client);
  }

  else if (request.startsWith("GET /status")) {
    checkSafety();

    String body = "{\"obstacle\":";
    body += obstacleDetected() ? "true" : "false";
    body += ",\"land\":";
    body += landDetected() ? "true" : "false";
    body += ",\"safe\":";
    body += safeToMove() ? "true" : "false";
    body += ",\"speed\":";
    body += String(motorSpeed);
    body += "}";

    sendJson(client, body);
  }

  else if (request.startsWith("GET /forward")) {
    if (safeToMove()) {
      forward();
      sendText(client, "Moving Forward");
    } else {
      stopMotor();
      sendText(client, "SAFETY STOP: Check sensors");
    }
  }

  else if (request.startsWith("GET /back")) {
    if (safeToMove()) {
      backward();
      sendText(client, "Moving Backward");
    } else {
      stopMotor();
      sendText(client, "SAFETY STOP: Check sensors");
    }
  }

  else if (request.startsWith("GET /left")) {
    if (safeToMove()) {
      left();
      sendText(client, "Turning Left");
    } else {
      stopMotor();
      sendText(client, "SAFETY STOP: Check sensors");
    }
  }

  else if (request.startsWith("GET /right")) {
    if (safeToMove()) {
      right();
      sendText(client, "Turning Right");
    } else {
      stopMotor();
      sendText(client, "SAFETY STOP: Check sensors");
    }
  }

  else if (request.startsWith("GET /stop")) {
    stopMotor();
    sendText(client, "Stopped");
  }

  else if (request.startsWith("GET /speed_up")) {
    motorSpeed += 25;

    if (motorSpeed > MAX_SPEED) {
      motorSpeed = MAX_SPEED;
    }

    sendText(client, "Speed: " + String(motorSpeed));
  }

  else if (request.startsWith("GET /speed_down")) {
    motorSpeed -= 25;

    if (motorSpeed < MIN_SPEED) {
      motorSpeed = MIN_SPEED;
    }

    sendText(client, "Speed: " + String(motorSpeed));
  }

  else if (request.startsWith("GET /emergency")) {
    stopMotor();
    sendText(client, "Emergency request received. Sending email.");
    client.stop();

    sendEmergencyEmail();
    checkSafety();
    return;
  }

  else {
    sendText(client, "Unknown command");
  }

  client.stop();
  checkSafety();
}

// =============== MOTOR CONTROL ===============
void forward() {
  if (!safeToMove()) {
    stopMotor();
    return;
  }

  analogWrite(IN1, motorSpeed);
  digitalWrite(IN2, LOW);

  analogWrite(IN3, motorSpeed);
  digitalWrite(IN4, LOW);
}

void backward() {
  if (!safeToMove()) {
    stopMotor();
    return;
  }

  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);

  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void left() {
  if (!safeToMove()) {
    stopMotor();
    return;
  }

  analogWrite(IN1, motorSpeed);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void right() {
  if (!safeToMove()) {
    stopMotor();
    return;
  }

  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);

  analogWrite(IN3, motorSpeed);
  digitalWrite(IN4, LOW);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// =============== PHYSICAL EMERGENCY ===============
void emergency() {
  Serial.println("EMERGENCY ALERT!");

  stopMotor();
  sendEmergencyEmail();
}

// =============== EMAILJS EMERGENCY EMAIL ===============
void sendEmergencyEmail() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi disconnected. Email not sent.");
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println(" SMART WHEELCHAIR EMERGENCY");
  Serial.println("================================");

  unsigned long totalSeconds = millis() / 1000;
  unsigned long days = totalSeconds / 86400;
  unsigned long hours = (totalSeconds % 86400) / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  String uptime =
    String(days) + " days, " +
    String(hours) + " hours, " +
    String(minutes) + " minutes, " +
    String(seconds) + " seconds";

  String subject =
    "EMERGENCY ALERT - Smart Wheelchair Assistance Required";

  String message =
    "EMERGENCY ALERT!\n\n"
    "A Smart Wheelchair emergency has been triggered.\n"
    "Please check on the user and provide assistance immediately.\n\n"
    "REGISTERED LOCATION:\n";

  message += emergencyLocation;

  message +=
    "\n\nDEVICE: Smart Wheelchair Controller\n"
    "DEVICE UPTIME: ";

  message += uptime;

  message +=
    "\n\nPlease verify the situation and arrange emergency "
    "assistance if necessary.";

  String payload = "{";

  payload += "\"service_id\":\"";
  payload += escapeJSON(String(serviceID));
  payload += "\",";

  payload += "\"template_id\":\"";
  payload += escapeJSON(String(templateID));
  payload += "\",";

  payload += "\"user_id\":\"";
  payload += escapeJSON(String(publicKey));
  payload += "\",";

  payload += "\"template_params\":{";

  payload += "\"to_email\":\"";
  payload += escapeJSON(String(recipient));
  payload += "\",";

  payload += "\"subject\":\"";
  payload += escapeJSON(subject);
  payload += "\",";

  payload += "\"message\":\"";
  payload += escapeJSON(message);
  payload += "\",";

  payload += "\"device\":\"Smart Wheelchair Controller\",";

  payload += "\"uptime\":\"";
  payload += escapeJSON(uptime);
  payload += "\"";

  payload += "}}";

  WiFiClientSecure secureClient;

  // Testing only. Configure certificate verification for production.
  secureClient.setInsecure();

  HTTPClient emailHttp;
  emailHttp.setTimeout(15000);

  Serial.println("Connecting to EmailJS...");

  if (!emailHttp.begin(secureClient, emailApiUrl)) {
    Serial.println("ERROR: HTTPS initialization failed.");
    return;
  }

  emailHttp.addHeader("Content-Type", "application/json");
  emailHttp.addHeader("origin", "http://localhost");

  Serial.println("Sending emergency email...");

  int httpCode = emailHttp.POST(payload);

  if (httpCode == 200) {
    Serial.println("SUCCESS: EmailJS accepted the request.");
    Serial.println("Check inbox and spam folder.");
  } else {
    Serial.print("EMAIL FAILED. HTTP code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      Serial.println("EmailJS response:");
      Serial.println(emailHttp.getString());
    } else {
      Serial.println(emailHttp.errorToString(httpCode));
    }
  }

  emailHttp.end();

  Serial.println("Emergency email request finished.");
  Serial.println("================================");
}
