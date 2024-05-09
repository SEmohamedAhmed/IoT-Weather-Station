#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ESP32_MailClient.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <SPI.h>
#include <SD.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#define seaLevelPressure_hPa 1016.25
#define DHTPIN 27         // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11
#define lightPin 34
File myFile;
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// Variables to save date and time
String formattedDate;
String dateStamp;
String timeStamp;
// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "Mr.Mody";
const char* password = "mrMody123@#123";
// To send Emails using Gmail on port 465 (SSL)
#define emailSenderAccount    ""
#define emailSenderPassword   ""
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465
#define emailSubject          "[ALERT] ESP32"
// auth[ydnc flve qjak mngu]
// Default Recipient Email Address
String inputMessage = "";
String enableEmailChecked = "checked";
String inputMessage2 = "true";
// Default Threshold  {threshold_temp, threshold_hum, threshold_press, threshold_alt}
String inputMessage3 = "35.0";
String inputMessage4 = "80.0";
String inputMessage5 = "150000.0";
String inputMessage6 = "50.0";
String lastTemperature;
String lastHumidity;
String lastPressure;
String lastAltitude;
AsyncWebServer server(80);
const char* PARAM_INPUT_1 = "email_input";
const char* PARAM_INPUT_2 = "enable_email_input";
const char* PARAM_INPUT_3 = "threshold_temp";
const char* PARAM_INPUT_4 = "threshold_hum";
const char* PARAM_INPUT_5 = "threshold_press";
const char* PARAM_INPUT_6 = "threshold_alt";
// Interval between sensor readings. Learn more about timers: https://R...content-available-to-author-only...s.com/esp32-pir-motion-sensor-interrupts-timers/
unsigned long previousEmailMillis = 0, prevSDWriteMillis = 0,prevReadFromEsp = 0;  
// interval for sending email 
const long intervalEmail = 10000;
// interval for read from esp32
const long intervalRead = 2000;
// interval for sd card writing
const long intervalSD = 2000;
// The Email Sending data object contains config and data to send
SMTPData smtpData;

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
String readDHTTemperature() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float t = dht.readTemperature(true);
  // Check if any reads failed and exit early (to try again).
  if (isnan(t)) {    
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    return String(t);
  }
}
String readDHTHumidity() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else {
    return String(h);
  }
}
String readBMPPressure(){
    float res = bmp.readPressure();
    if(isnan(res)){
      Serial.println("Failed to read Pressure from BMP sensor!");
      return "--";
    }else{
      return String(res);
    }
}
String readBMPAltitude(){
    float res = bmp.readAltitude(seaLevelPressure_hPa);
    if(isnan(res)){
      Serial.println("Failed to read Altitude from BMP sensor!");
      return "--";
    }else{
      return String(res);
    }
}
// Replaces placeholder with values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return lastTemperature;
  }
  else if(var == "HUMIDITY"){
    return lastHumidity;
  }
  else if(var == "PRESSURE"){
    return lastPressure;
  }
  else if(var == "ALTITUDE"){
    return lastAltitude;
  }
  else if(var == "EMAIL_INPUT"){
    return inputMessage;
  }
  else if(var == "ENABLE_EMAIL"){
    return enableEmailChecked;
  }
  else if(var == "TEMP_THRESHOLD"){
    return inputMessage3;
  }
  else if(var == "HUM_THRESHOLD"){
    return inputMessage4;
  }
  else if(var == "PRESS_THRESHOLD"){
    return inputMessage5;
  }
  else if(var == "ALT_THRESHOLD"){
    return inputMessage6;
  }
  return String();
}
bool sendEmailNotification(String emailMessage){
  // Set the SMTP Server Email host, port, account and password
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
  // enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
  //smtpData.setSTARTTLS(true);
  // Set the sender name and Email
  smtpData.setSender("ESP32", emailSenderAccount);
  // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("High");
  // Set the subject
  smtpData.setSubject(emailSubject);
  // Set the message with HTML format
  smtpData.setMessage(emailMessage, true);
  // Add recipients
  smtpData.addRecipient(inputMessage);
  smtpData.setSendCallback(sendCallback);
  // Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData)) {
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    return false;
  }
  // Clear all data from Email object to free memory
  smtpData.empty();
  return true;
}
// Callback function to get the Email sending status
void sendCallback(SendStatus msg) {
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}
void processEmail(String &lastReading, String &inputThreshold, String readingType, String metric, String &emailMessage){
  float currentReading = lastReading.toFloat();
  if(currentReading > inputThreshold.toFloat()){
    emailMessage += String(readingType + " above threshold. Current " + readingType + " : ") + String(currentReading) + " " + metric + String("<br>");
  }
}
void readFromESP(){
  lastTemperature = readDHTTemperature();
  lastHumidity = readDHTHumidity();
  lastPressure = readBMPPressure();
  lastAltitude = readBMPAltitude();
  prevReadFromEsp = millis();
}
void getDateTime(){
  if(!timeClient.update())
    timeClient.forceUpdate();
  // formattedDate => 2018-05-28 T 16:00:13Z
  formattedDate = timeClient.getFormattedDate();
  // Extract date
  int splitT = formattedDate.indexOf("T");
  dateStamp = formattedDate.substring(0, splitT);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
}
char index_html [] PROGMEM =R"rawliteral(<!DOCTYPE HTML>
<html>

<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        html {
            font-family: Arial;
            display: inline-block;
            margin: 0px auto;
            text-align: center;
        }

        h2 {
            font-size: 3.0rem;
        }

        p {
            font-size: 3.0rem;
        }

        .units {
            font-size: 1.8rem;
        }

        .dht-labels {
            font-size: 3.0rem;
            vertical-align: middle;
            padding-bottom: 15px;
        }

        input[type="submit"] {
            font-size: 20px;
            width: auto;
            height: 40px;
            background-color: #606860;
            color: white;
            cursor: pointer;
        }

        input[type="submit"]:hover {
            background-color: #333934;
        }

        .background-text {
            padding-right: 30px;
        }

        .input-box {
            font-size: large;
            font-weight: bold;
            width: 220px;
            height: 25px;
            border: 1px solid #a19f9f;
            margin: 0;
        }
    </style>
</head>

<body>
    <h2>Weather Station Server</h2>
    <p>
        <img src="https://www.freeiconspng.com/uploads/temperature-icon-png-1.png" width="50"
            alt="Png Temperature Save" />
        <span class="dht-labels">Temperature</span><span id="temperature">: %TEMPERATURE% </span><span
            class="units">Celsius</span>
    </p>
    <p>
        <img src="https://cdn.iconscout.com/icon/free/png-512/free-humidity-4596680-3796777.png?f=webp&w=50">
        <span class="dht-labels">Humidity</span><span id="humidity">: %HUMIDITY% </span><span
            class="units">Percent</span>
    </p>
    <p>
        <img src="https://cdn4.iconfinder.com/data/icons/weather-287/32/92-_pressure-_air-_wind-_weather-512.png"
            height="50" width="50">
        <span class="dht-labels">Pressure</span><span id="pressure">: %PRESSURE% </span><span
            class="units">Pascal</span>
    </p>
    <p>
        <img src="https://cdn-icons-png.flaticon.com/512/9620/9620006.png" height="50" width="50">
        <span class="dht-labels">Altitude</span><span id="altitude">: %ALTITUDE% </span><span class="units">Meter</span>
    </p>
    <hr>
    <form action="/get">
        <span class="background-text">Email Address</span><input class="input-box type=" email" name="email_input"
            value="%EMAIL_INPUT%" required><br>
        <hr>
        <span class="background-text">Enable Email Notification</span><input class="input-box" type="checkbox"
            name="enable_email_input" value="true" %ENABLE_EMAIL%><br>
        <hr>
        <span class="background-text">Temperature Threshold</span><input class="input-box" type="number" step="0.1"
            name="threshold_temp" value="%TEMP_THRESHOLD%" required><br>
        <hr>
        <span class="background-text">Humidity Threshold</span><input class="input-box" type="number" step="0.1"
            max="100" min="0" name="threshold_hum" value="%HUM_THRESHOLD%" required><br>
        <hr>
        <span class="background-text">Pressure Threshold</span><input class="input-box" type="number" step="0.1"
            name="threshold_press" value="%PRESS_THRESHOLD%" required><br>
        <hr>
        <span class="background-text">Altitude Threshold</span> <input class="input-box" type="number" step="0.1"
            name="threshold_alt" value="%ALT_THRESHOLD%" required><br>
        <hr>
        <br>
        <input type="submit" value="Submit Your input data">
    </form>
    <br><br><br><br><br><br><br><br><br><br><br><br><br><br><br>
</body>
<script>
    function fetchDataAndUpdate(elementId, endpoint) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById(elementId).innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", endpoint, true);
        xhttp.send();
    }
    setInterval(function () {
        fetchDataAndUpdate("temperature", "/temperature");
    }, 500);
    setInterval(function () {
        fetchDataAndUpdate("humidity", "/humidity");
    }, 500);
    setInterval(function () {
        fetchDataAndUpdate("pressure", "/pressure");
    }, 500);
    setInterval(function () {
        fetchDataAndUpdate("altitude", "/altitude");
    }, 500);
</script>

</html>)rawliteral";

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.print(".");
  }
  Serial.print("ESP IP Address: ");
  Serial.println(WiFi.localIP());
  pinMode(lightPin, INPUT); // light sensor
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone
  timeClient.setTimeOffset(2 * 60 * 60); // +2 UTC
  //Start the DHT11 sensor
  dht.begin();
  // BMP180
  bmp.begin();
  // SD card module
  SD.begin();
  delay(1000);
  // Send web page to client if path = root path
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  // Receive an HTTP GET request if path = "/get" at <ESP_IP>/get?email_input=<inputMessage>&enable_email_input=<inputMessage2>&threshold_input=<inputMessage3>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // GET email_input value on <ESP_IP>/get?email_input=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
    }
    else {
      inputMessage = "No message sent";
    }
    // GET enable_email_input value on <ESP_IP>/get?enable_email_input=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      enableEmailChecked = "checked";
    }
    else {
      inputMessage2 = "false";
      enableEmailChecked = "";
    }
    // GET threshold_input value on <ESP_IP>/get?threshold_input=<inputMessagei>
    if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage3 = request->getParam(PARAM_INPUT_3)->value();
    }
    if (request->hasParam(PARAM_INPUT_4)) {
      inputMessage4 = request->getParam(PARAM_INPUT_4)->value();
    }
    if (request->hasParam(PARAM_INPUT_5)) {
      inputMessage5 = request->getParam(PARAM_INPUT_5)->value();
    }
    if (request->hasParam(PARAM_INPUT_6)) {
      inputMessage6 = request->getParam(PARAM_INPUT_6)->value();
    } 
    request->send(200, "text/html", "Data sent to your ESP Successfully.<br><a href=\"/\">Return to Home Page</a>");
  });
  server.onNotFound(notFound);
  server.begin();
  // initial Readings 
  readFromESP();
  getDateTime();
}

void loop() {
  unsigned long currentMillis = millis();
  // ESP32 Readings
  if (currentMillis - prevReadFromEsp >= intervalRead){
    readFromESP();
  }
  // send email every interval ms
  if (currentMillis - previousEmailMillis >= intervalEmail && inputMessage2 == "true") {
    String emailMessage = String();
    // temp
    processEmail(lastTemperature, inputMessage3, "Temprature", "Celsius", emailMessage);
    // hum
    processEmail(lastHumidity, inputMessage4, "Humidity", "%", emailMessage);
    // press
    processEmail(lastPressure, inputMessage5, "Pressure", "Pascal", emailMessage);
    // alt
    processEmail(lastAltitude, inputMessage6, "Altitude", "meter", emailMessage);
    
    if(emailMessage.length() > 0) {
      if(sendEmailNotification(emailMessage)){
        previousEmailMillis = currentMillis;   
        Serial.println(emailMessage);
      }
      else {
        Serial.println("Email failed to send, Check internet connection");
      }  
    }
  }
  // Write to SD card
  if(currentMillis - prevSDWriteMillis >= intervalSD){
    getDateTime();
    prevSDWriteMillis = currentMillis;
    if (!SD.exists("/data.txt")){
      myFile = SD.open("/data.txt", FILE_WRITE);
      myFile.println("Temperature(Celsius), Humidity(Percent), Pressure(Pascal), Altitude(Meter), Date, Time");
      myFile.close();
    }
    myFile = SD.open("/data.txt", FILE_APPEND);
    if (myFile) {
      myFile.println(lastTemperature + ", " + lastHumidity + ", " + lastPressure + ", " + lastAltitude
                      + ", " + dateStamp + ", " + timeStamp);
      myFile.close();
    } else {
      Serial.println("error opening data.txt");
    }
  }
}
