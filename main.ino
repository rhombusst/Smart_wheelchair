#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <string.h>

// =============== SETUP WIFI ===============
const char* setupSSID = "SmartWheelchair_Setup";
const char* setupPassword = "wheel123";

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

// =============== EEPROM ===============
const int EEPROM_SIZE = 100;
bool setupMode = false;

// =============== EMERGENCY BUTTON ===============
bool lastEmergencyButtonState = HIGH;
unsigned long lastEmergencyButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// =============== MOTOR SPEED ===============
int motorSpeed = 255;
const int MIN_SPEED = 100;
const int MAX_SPEED = 255;

// =============== API SETTINGS ===============
const char* emailApiUrl =
  "http://192.168.1.7:3000/api/send-email";

const char* emergencyEmail =
  "aarin@kmrb.tech";

const char* locationApiUrl =
  "http://192.168.1.7:3000/api/location/latest";

// =============== SERVER ===============
WiFiServer server(80);
WiFiClient httpClient;
HTTPClient http;

// =============== SETUP PAGE ===============
const char setupPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Wheelchair Setup</title>
<style>
body{margin:0;padding:20px;background:#0b1220;color:white;
font-family:Arial;text-align:center}
.card{max-width:400px;margin:30px auto;padding:24px;
background:#172338;border-radius:18px}
input,button{box-sizing:border-box;width:100%;padding:14px;
margin:10px 0;border:0;border-radius:10px;font-size:16px}
button{background:#2563eb;color:white;font-weight:bold}
p{color:#bfdbfe;overflow-wrap:anywhere}
</style>
</head>
<body>
<div class="card">
<h2>♿ Smart Wheelchair</h2>
<h3>Wi-Fi Configuration</h3>
<p>Enter your Wi-Fi name and password.</p>
<form id="wifiForm">
<input id="ssid" placeholder="Wi-Fi name (SSID)"
maxlength="32" required>
<input id="password" type="password"
placeholder="Wi-Fi password" maxlength="63">
<button type="submit">Connect to Wi-Fi</button>
</form>
<p id="result">Ready to configure</p>
</div>
<script>
document.getElementById('wifiForm').onsubmit=async function(e){
e.preventDefault();
let result=document.getElementById('result');
result.textContent='Connecting. Please wait...';
try{
let r=await fetch('/save?ssid='+
encodeURIComponent(document.getElementById('ssid').value)+
'&pass='+encodeURIComponent(document.getElementById('password').value));
result.textContent=await r.text();
}catch(e){
result.textContent='Check Serial Monitor for connection result.';
}
};
</script>
</body>
</html>
)rawliteral";

// =============== CONTROLLER PAGE ===============
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
<div class="status" id="wifi">Wi-Fi: Connected</div>
<div class="status" id="obstacle">Obstacle: Checking</div>
<div class="status" id="land">Ground: Checking</div>
<p id="message">Controller ready</p>
</div>

<div class="card">
<h3>Movement Control</h3>
<div class="grid">
<div></div><button onclick="move('forward')">▲<br>Forward</button><div></div>
<button onclick="move('left')">◀️<br>Left</button>
<button class="stop" onclick="move('stop')">■ STOP</button>
<button onclick="move('right')">▶️<br>Right</button>
<div></div><button onclick="move('back')">▼<br>Backward</button><div></div>
</div>
<p><small>Movement is blocked when a safety sensor signals danger.</small></p>
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
<button class="emergency" onclick="emergencyAlert()">🚨 EMERGENCY ALERT</button>
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
}finally{clearTimeout(timeout);}
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
}finally{busy=false;}
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
document.getElementById('message').textContent='SAFETY STOP: Check sensors';
}
}catch(e){
document.getElementById('connection').textContent='Controller not responding';
}
}
refreshStatus();
setInterval(refreshStatus,1000);
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
String getLatestLocationFromDatabase();
bool obstacleDetected();
bool landDetected();
bool safeToMove();
void checkSafety();
void sendText(WiFiClient &client,String body);
void sendJson(WiFiClient &client,String body);
void sendWebsite(WiFiClient &client);
void sendSetupWebsite(WiFiClient &client);
void saveCredentials(String ssid,String pass);
bool loadCredentials(String &ssid,String &pass);
void startSetupMode();
bool connectToWiFi(String ssid,String pass);
String urlDecode(String value);
String getQueryValue(String request,String key);

// =============== URL DECODE ===============
String urlDecode(String value){
String decoded;
for(unsigned int i=0;i<value.length();i++){
if(value[i]=='+')decoded+=' ';
else if(value[i]=='%'&&i+2<value.length()){
char hex[3]={value[i+1],value[i+2],0};
decoded+=(char)strtol(hex,nullptr,16);
i+=2;
}else decoded+=value[i];
}
return decoded;
}

String getQueryValue(String request,String key){
String search=key+"=";
int start=request.indexOf(search);
if(start<0)return "";
start+=search.length();
int end=request.indexOf('&',start);
if(end<0)end=request.indexOf(' ',start);
if(end<0)end=request.length();
return urlDecode(request.substring(start,end));
}

// =============== EEPROM SAVE ===============
void saveCredentials(String ssid,String pass){
EEPROM.begin(EEPROM_SIZE);
for(int i=0;i<EEPROM_SIZE;i++)EEPROM.write(i,0);
for(int i=0;i<ssid.length()&&i<32;i++)EEPROM.write(i,ssid[i]);
for(int i=0;i<pass.length()&&i<63;i++)EEPROM.write(33+i,pass[i]);
EEPROM.write(97,1);
EEPROM.commit();
EEPROM.end();
}

bool loadCredentials(String &ssid,String &pass){
EEPROM.begin(EEPROM_SIZE);
if(EEPROM.read(97)!=1){EEPROM.end();return false;}
char s[33],p[64];
for(int i=0;i<32;i++)s[i]=EEPROM.read(i);
s[32]='\0';
for(int i=0;i<63;i++)p[i]=EEPROM.read(33+i);
p[63]='\0';
EEPROM.end();
ssid=String(s);
pass=String(p);
return ssid.length()>0;
}

// =============== WIFI CONNECTION ===============
bool connectToWiFi(String ssid,String pass){
Serial.print("Connecting to ");
Serial.println(ssid);
WiFi.mode(WIFI_STA);
WiFi.begin(ssid.c_str(),pass.c_str());
unsigned long start=millis();
while(WiFi.status()!=WL_CONNECTED&&millis()-start<20000){
delay(250);
Serial.print(".");
}
Serial.println();
if(WiFi.status()==WL_CONNECTED){
setupMode=false;
server.begin();
Serial.println("Wi-Fi connected!");
Serial.print("Controller IP: http://");
Serial.println(WiFi.localIP());
return true;
}
Serial.println("Connection failed.");
WiFi.disconnect();
return false;
}

void startSetupMode(){
// Start hotspot on every boot or reset
WiFi.disconnect();
WiFi.mode(WIFI_AP_STA);

WiFi.softAP("SmartWheelchair_Setup", "wheel123");

Serial.println();
Serial.println("Wi-Fi hotspot started!");
Serial.print("Hotspot name: ");
Serial.println("SmartWheelchair_Setup");
Serial.print("Setup page: http://");
Serial.println(WiFi.softAPIP());
}

// =============== SENSORS ===============
bool obstacleDetected(){
return digitalRead(OBSTACLE_IR)==OBSTACLE_DETECTED_STATE;
}
bool landDetected(){
return digitalRead(LAND_IR)==LAND_PRESENT_STATE;
}
bool safeToMove(){
return !obstacleDetected()&&landDetected();
}
void checkSafety(){
if(!safeToMove())stopMotor();
}

// =============== HTTP RESPONSES ===============
void sendText(WiFiClient &client,String body){
client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
client.print(body.length());
client.print("\r\n\r\n");
client.print(body);
}

void sendJson(WiFiClient &client,String body){
client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
client.print(body.length());
client.print("\r\n\r\n");
client.print(body);
}

void sendWebsite(WiFiClient &client){
size_t len=strlen_P(webpage);
client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
client.print(len);
client.print("\r\n\r\n");
client.write_P(webpage,len);
}

void sendSetupWebsite(WiFiClient &client){
size_t len=strlen_P(setupPage);
client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ");
client.print(len);
client.print("\r\n\r\n");
client.write_P(setupPage,len);
}

// =============== SETUP ===============
void setup(){
Serial.begin(115200);
pinMode(IN1,OUTPUT);
pinMode(IN2,OUTPUT);
pinMode(IN3,OUTPUT);
pinMode(IN4,OUTPUT);
pinMode(EMERGENCY_BUTTON,INPUT_PULLUP);
pinMode(OBSTACLE_IR,INPUT);
pinMode(LAND_IR,INPUT);
stopMotor();

String ssid,pass;
if(loadCredentials(ssid,pass)){
if(connectToWiFi(ssid,pass))return;
}
startSetupMode();
}

// =============== MAIN LOOP ===============
void loop(){
if(setupMode){
WiFiClient client=server.available();
if(!client)return;
client.setTimeout(1500);
unsigned long start=millis();
while(!client.available()&&millis()-start<1500)delay(1);
if(!client.available()){client.stop();return;}

String request=client.readStringUntil('\r');
client.readStringUntil('\n');
Serial.println(request);

if(request.startsWith("GET /save?")){
String newSSID=getQueryValue(request,"ssid");
String newPass=getQueryValue(request,"pass");

if(newSSID.length()==0||newSSID.length()>32||newPass.length()>63){
sendText(client,"Invalid Wi-Fi details.");
client.stop();
return;
}

sendText(client,"Connecting to Wi-Fi. Check Serial Monitor.");
client.stop();
saveCredentials(newSSID,newPass);
if(!connectToWiFi(newSSID,newPass))startSetupMode();
return;
}

sendSetupWebsite(client);
client.stop();
return;
}

// Physical emergency button
checkSafety();
bool buttonState=digitalRead(EMERGENCY_BUTTON);
if(buttonState!=lastEmergencyButtonState)lastEmergencyButtonTime=millis();
if(millis()-lastEmergencyButtonTime>DEBOUNCE_DELAY&&
buttonState==LOW&&lastEmergencyButtonState==HIGH){
emergency();
}
lastEmergencyButtonState=buttonState;

WiFiClient client=server.available();
if(!client)return;
client.setTimeout(1500);
unsigned long timeout=millis()+1500;
while(!client.available()&&millis()<timeout){checkSafety();delay(1);}
if(!client.available()){client.stop();return;}

String request=client.readStringUntil('\r');
client.readStringUntil('\n');
Serial.println(request);

if(request.startsWith("GET / ")||request.startsWith("GET /index.html")){
sendWebsite(client);
}
else if(request.startsWith("GET /status")){
checkSafety();
String body="{\"obstacle\":"+String(obstacleDetected()?"true":"false")+
",\"land\":"+String(landDetected()?"true":"false")+
",\"safe\":"+String(safeToMove()?"true":"false")+
",\"speed\":"+String(motorSpeed)+"}";
sendJson(client,body);
}
else if(request.startsWith("GET /forward")){
if(safeToMove()){forward();sendText(client,"Moving Forward");}
else{stopMotor();sendText(client,"SAFETY STOP: Check sensors");}
}
else if(request.startsWith("GET /back")){
if(safeToMove()){backward();sendText(client,"Moving Backward");}
else{stopMotor();sendText(client,"SAFETY STOP: Check sensors");}
}
else if(request.startsWith("GET /left")){
if(safeToMove()){left();sendText(client,"Turning Left");}
else{stopMotor();sendText(client,"SAFETY STOP: Check sensors");}
}
else if(request.startsWith("GET /right")){
if(safeToMove()){right();sendText(client,"Turning Right");}
else{stopMotor();sendText(client,"SAFETY STOP: Check sensors");}
}
else if(request.startsWith("GET /stop")){
stopMotor();sendText(client,"Stopped");
}
else if(request.startsWith("GET /speed_up")){
motorSpeed+=25;
if(motorSpeed>MAX_SPEED)motorSpeed=MAX_SPEED;
sendText(client,"Speed: "+String(motorSpeed));
}
else if(request.startsWith("GET /speed_down")){
motorSpeed-=25;
if(motorSpeed<MIN_SPEED)motorSpeed=MIN_SPEED;
sendText(client,"Speed: "+String(motorSpeed));
}
else if(request.startsWith("GET /emergency")){
stopMotor();
sendEmergencyEmail();
sendText(client,"Emergency request processed");
}
else{
sendText(client,"Unknown command");
}
client.stop();
checkSafety();
}

// =============== MOTOR CONTROL ===============
void forward(){
if(!safeToMove()){stopMotor();return;}
analogWrite(IN1,motorSpeed);
digitalWrite(IN2,LOW);
analogWrite(IN3,motorSpeed);
digitalWrite(IN4,LOW);
}

void backward(){
if(!safeToMove()){stopMotor();return;}
digitalWrite(IN1,LOW);
analogWrite(IN2,motorSpeed);
digitalWrite(IN3,LOW);
analogWrite(IN4,motorSpeed);
}

void left(){
if(!safeToMove()){stopMotor();return;}
analogWrite(IN1,motorSpeed);
digitalWrite(IN2,LOW);
digitalWrite(IN3,LOW);
analogWrite(IN4,motorSpeed);
}

void right(){
if(!safeToMove()){stopMotor();return;}
digitalWrite(IN1,LOW);
analogWrite(IN2,motorSpeed);
analogWrite(IN3,motorSpeed);
digitalWrite(IN4,LOW);
}

void stopMotor(){
digitalWrite(IN1,LOW);
digitalWrite(IN2,LOW);
digitalWrite(IN3,LOW);
digitalWrite(IN4,LOW);
}

// =============== EMERGENCY ===============
void emergency(){
Serial.println("EMERGENCY ALERT!");
stopMotor();
sendEmergencyEmail();
}

// =============== SEND EMAIL ===============
// =============== EMAILJS EMERGENCY EMAIL ===============
void sendEmergencyEmail() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi disconnected. Email not sent.");
    return;
  }

  // EmailJS configuration
  const char* serviceID  = "service_9bixi9i";
  const char* templateID = "template_xcxb0vn";
  const char* publicKey  = "BuzCBYQvWq_nEKdc2";

  const char* recipient = "samarthtiwari153@gmail.com";

  const char* location =
    "Rabindranath Tagore University, Bhojpur, Chiklod Road, "
    "near Bangrasiya Chouraha, Bhopal, Madhya Pradesh 464993";

  const char* emailURL =
    "https://api.emailjs.com/api/v1.0/email/send";

  Serial.println();
  Serial.println("================================");
  Serial.println("   SMART WHEELCHAIR EMERGENCY");
  Serial.println("================================");

  // Calculate uptime
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

  message += location;

  message +=
      "\n\nDEVICE: Smart Wheelchair Controller\n"
      "DEVICE UPTIME: ";

  message += uptime;

  message +=
      "\n\nPlease verify the situation and arrange emergency "
      "assistance if necessary.";

  // JSON escape helper
  auto escapeJSON = [](const String &input) -> String {
    String output;
    output.reserve(input.length() + 16);

    for (unsigned int i = 0; i < input.length(); i++) {
      char c = input[i];

      switch (c) {
        case '"':  output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\n': output += "\\n";  break;
        case '\r': output += "\\r";  break;
        case '\t': output += "\\t";  break;

        default:
          if ((unsigned char)c >= 0x20) {
            output += c;
          }
          break;
      }
    }

    return output;
  };

  // Construct EmailJS JSON payload
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

  // Send HTTPS request
  WiFiClientSecure secureClient;

  // Testing only: TLS certificate verification is disabled.
  // Configure certificate verification for production.
  secureClient.setInsecure();

  HTTPClient emailHttp;
  emailHttp.setTimeout(15000);

  Serial.println("Connecting to EmailJS...");

  if (!emailHttp.begin(secureClient, emailURL)) {
    Serial.println("ERROR: HTTPS initialization failed.");
    return;
  }

  emailHttp.addHeader("Content-Type", "application/json");
  emailHttp.addHeader("origin", "http://localhost");

  Serial.println("Sending emergency email...");

  int httpCode = emailHttp.POST(payload);

  if (httpCode == 200) {
    Serial.println("SUCCESS: EmailJS accepted the request.");
    Serial.println("Check the recipient inbox and spam folder.");
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

// =============== LOCATION API ===============
String getLatestLocationFromDatabase(){
if(WiFi.status()!=WL_CONNECTED)return "";

http.begin(httpClient,locationApiUrl);
http.setTimeout(7000);
int code=http.GET();
String locationInfo="";

if(code>0){
String response=http.getString();
Serial.println("Location API response:");
Serial.println(response);

if(response.indexOf("\"success\":true")>=0){
int latIndex=response.indexOf("\"latitude\":");
int lonIndex=response.indexOf("\"longitude\":");

if(latIndex>=0&&lonIndex>=0){
int latStart=response.indexOf(":",latIndex)+1;
int latEnd=response.indexOf(",",latStart);
int lonStart=response.indexOf(":",lonIndex)+1;
int lonEnd=response.indexOf(",",lonStart);
if(lonEnd<0)lonEnd=response.indexOf("}",lonStart);

if(latEnd>=0&&lonEnd>=0){
String latitude=response.substring(latStart,latEnd);
String longitude=response.substring(lonStart,lonEnd);
latitude.trim();
longitude.trim();
locationInfo=latitude+", "+longitude;
}
}
}
}else{
Serial.print("Location API error: ");
Serial.println(http.errorToString(code));
}

http.end();
return locationInfo;
}
