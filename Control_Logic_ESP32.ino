// --- Blynk Preparation ---
#define BLYNK_TEMPLATE_ID "Insert Yout ID Here"
#define BLYNK_TEMPLATE_NAME "PV Cooling System"
#define BLYNK_AUTH_TOKEN "Insert your Token here"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <UniversalTelegramBot.h>

// --- WiFi Configuration ---
const char* ssid = "NaHBrO";
const char* password = "SCHX0212";
String lastWiFiStatus = "";
String wifiStatus = "Connecting";
unsigned long lastWiFiRetry = 0;

// --- NTP Setting (GMT+8 Malaysia) ---
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 28800; 
const int daylightOffset_sec = 0;
RTC_DS3231 rtc;

// --- Telegram Setting ---
const char* botToken = "Telegram Bot Token";
const char* chatId = "Telegram Chat ID";
WiFiClientSecure client;
UniversalTelegramBot bot(botToken,client);

// --- Google Sheets Data Logging --- 
String googleScriptUrl = "Google Sheets URL";

// --- Hardware Configuration ---
Adafruit_INA219 sensor_uncooledPV(0x40);
Adafruit_INA219 sensor_cooledPV(0x41);
Adafruit_INA219 sensor_pump(0x44);
Adafruit_INA219 sensor_battery(0x45);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const int pins[] = {4, 13, 14, 25, 26, 27};
OneWire ow1(pins[0]); DallasTemperature ts1(&ow1);
OneWire ow2(pins[1]); DallasTemperature ts2(&ow2);
OneWire ow3(pins[2]); DallasTemperature ts3(&ow3);
OneWire ow4(pins[3]); DallasTemperature ts4(&ow4);
OneWire ow5(pins[4]); DallasTemperature ts5(&ow5);
OneWire ow6(pins[5]); DallasTemperature ts6(&ow6);

const int redled = 19; const int greenled = 32;
const int pump1 = 5;   const int pump2 = 18;
const int irradiance = 33;
const int bttnOn = 34; 

// --- Parameters & States ---
float cAtemp = 0, luxPresent = 0;
String pumpStatus = "OFF", systemStatus = "Initializing", batteryStatus = "Checking"; 

bool pumpIsRunning = false, pumpIsResting = false;
int displayPage = 0;

String lastSystemStatus = "";

unsigned long lastTelegramCheck = 0;
bool welcomeSent = false;
bool telegramManualReq = false;

// --- Timers ---
unsigned long lastTempRequest = 0, lastOLEDUpdate = 0;
unsigned long pumpStartTime = 0, cooldownStartTime = 0;
unsigned long lastBlynkUpdate = 0, lastGoogleUpdate = 0, lastSerialLogTime = 0, lastIrradianceUpdate = 0;

// --- Time Helper ---
String getTimeString() {
  struct tm timeinfo;
  char buf[25];

  // Try to get time from NTP via WiFi
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo)) {
    strftime(buf, sizeof(buf), "%d/%m/%y %H:%M:%S", &timeinfo);
    
    // Sync RTC if drift is detected (> 30s)
    DateTime now = rtc.now();
    if (abs((long)now.unixtime() - (long)mktime(&timeinfo)) > 30) {
       rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
    return String(buf);
  } 
  // Fallback to DS3231 Hardware Clock
    else {
      DateTime now = rtc.now();
      sprintf(buf, "%02d/%02d/%02d %02d:%02d:%02d", now.day(), now.month(), now.year() % 100, now.hour(), now.minute(), now.second());
      return String(buf);
  }
}

void irradianceMeasurement() {
  int rawValue = analogRead(irradiance);
  luxPresent = (1.0 - (rawValue / 4095.0)) * 1000.0;
  if (luxPresent < 0) luxPresent = 0;
  if (luxPresent > 1200) luxPresent = 1200;
}

// --- Read the DS18B20 Temp Sensor Reading ---
void displayTempData(String label, DallasTemperature &s1, DallasTemperature &s2, DallasTemperature &s3){
    s1.requestTemperatures();
    s2.requestTemperatures();
    s3.requestTemperatures();

    float t1 = s1.getTempCByIndex(0);
    float t2 = s2.getTempCByIndex(0);
    float t3 = s3.getTempCByIndex(0);

    // Filter out disconnected sensor errors (-127C)
    if (t1 <= -100.0) t1 = 0.00; 
    if (t2 <= -100.0) t2 = 0.00; 
    if (t3 <= -100.0) t3 = 0.00; 

    float tAvg = (t1 + t2 + t3) / 3.0;

    char buffer[200];
    sprintf(buffer,"    %-13s | %8.2f°C%8 | %10.2f°C%10 | %11.2f°C%11 | %10.2f°C ",
            label.c_str(), t1, t2, t3, tAvg);
}

// --- OLED Update ---
void updateOLED(float temp, float irr, String time) {
  if (millis() - lastOLEDUpdate >= 5000) {
    displayPage = (displayPage + 1) % 3;
    lastOLEDUpdate = millis();
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  switch (displayPage) {
    case 0:
      display.println("---- System Info ----");
      display.setCursor(0, 15);
      display.print(WiFi.status() == WL_CONNECTED ? "WiFi: CONNECTED" : "WiFi: DISCONNECTED");
      display.setCursor(0, 30);
      display.printf("MODE: %s", systemStatus.c_str());
      display.setCursor(0, 45);
      display.printf("TIME: %s", time.c_str());
      break;

    case 1:
      display.println("------ Data ------");
      display.setCursor(0, 15);
      display.printf("Solar Irr: %.0f W/m2", irr);
      display.setCursor(0, 30);
      display.printf("PV Temp: %.1f C", temp);
      display.setCursor(0, 45);
      display.printf("Pump: %s", pumpStatus.c_str());
      break;

    case 2:
      display.println("----- Battery -----");
      display.setCursor(0, 15);
      display.printf("Voltage: %.2f V", sensor_battery.getBusVoltage_V());
      display.setCursor(0, 30);
      display.printf("Current: %.2f mA", sensor_battery.getCurrent_mA());
      display.setCursor(0, 45);
      display.printf("Status: %s", batteryStatus.c_str());
      delay(500);
      break;
  }
  display.display();
}

void sendDataToGoogle() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  StaticJsonDocument<2048> doc;
  doc["irr"] = luxPresent; doc["uV"] = sensor_uncooledPV.getBusVoltage_V(); doc["uC"] = sensor_uncooledPV.getCurrent_mA();
  doc["uP"] = sensor_uncooledPV.getPower_mW(); doc["cV"] = sensor_cooledPV.getBusVoltage_V(); doc["cC"] = sensor_cooledPV.getCurrent_mA();
  doc["cP"] = sensor_cooledPV.getPower_mW(); doc["bV"] = sensor_battery.getBusVoltage_V(); doc["bC"] = sensor_battery.getCurrent_mA();
  doc["bP"] = sensor_battery.getPower_mW(); doc["pV"] = sensor_pump.getBusVoltage_V(); doc["pC"] = sensor_pump.getCurrent_mA();
  doc["pP"] = sensor_pump.getPower_mW();doc["pump"] = pumpStatus;

  ts1.requestTemperatures(); ts2.requestTemperatures(); ts3.requestTemperatures();
  ts4.requestTemperatures(); ts5.requestTemperatures(); ts6.requestTemperatures();

  // 3. Uncooled Individual & Avg
  float uT = ts1.getTempCByIndex(0); float uM = ts2.getTempCByIndex(0); float uB = ts3.getTempCByIndex(0);
  doc["uTtemp"] = uT;doc["uMtemp"] = uM; doc["uBtemp"] = uB; doc["uAtemp"] = (uT + uM + uB) / 3.0;

  // 4. Cooled Individual & Avg
  float cT = ts4.getTempCByIndex(0);
  float cM = ts5.getTempCByIndex(0);
  float cB = ts6.getTempCByIndex(0);
  doc["cTtemp"] = cT;
  doc["cMtemp"] = cM;
  doc["cBtemp"] = cB;
  doc["cAtemp"] = (cT + cM + cB) / 3.0;

  String jsonStr;
  serializeJson(doc, jsonStr);
  http.begin(googleScriptUrl);
  http.addHeader("Content-Type", "application/json");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.POST(jsonStr);
  http.end();
}

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + 
    "/sendMessage?chat_id=" + String(chatId) + "&text=" + message;
    
    http.begin(url);
    int httpCode = http.GET(); // Simple GET request

    http.end();
  }
}

void handleTelegramRequest(float cAtemp, float luxPresent, int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // This JSON creates 3 vertical buttons with Emojis
    String keyboardJson = "[ [\"View System Status\"], [\"Activate Pump ON\"], [\"Emergency STOP\"]]";

    if (text == "/start" || text == "menu") {
      bot.sendMessageWithReplyKeyboard(chat_id, "Welcome to PV Cooling System! Choose an option:", "", keyboardJson);    
    }

    // --- HANDLING THE BUTTON CLICKS ---
    if (text == "View System Status") {
      float bV = sensor_battery.getBusVoltage_V();
      String report = "*Panel Temp:* " + String(cAtemp) + "°C\n";
      report += "*Irradiance:* " + String(luxPresent) + " W/m2\n";
      report += "*Battery:* " + String(bV) + " V\n";
      report += "*System:* " + String(systemStatus);
      bot.sendMessage(chat_id, report, "Markdown");
    }

    if (text == "Activate Pump ON") {
      telegramManualReq = true;
    }

    if (text == "Emergency STOP") {
      digitalWrite(pump1, HIGH); digitalWrite(pump2, HIGH);
      pumpStatus = "OFF"; 
      pumpIsRunning = false; pumpIsResting = false; // Reset the pump status
      bot.sendMessage(chat_id, "All pumps have been deactivated.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  client.setInsecure();
  rtc.begin();
  
  pinMode(pump1, OUTPUT); pinMode(pump2, OUTPUT);
  digitalWrite(pump1, HIGH); digitalWrite(pump2, HIGH);
  pinMode(bttnOn, INPUT_PULLUP); // GPIO 34
  pinMode(redled, OUTPUT); pinMode(greenled, OUTPUT);

  WiFi.begin(ssid, password);
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  ts1.begin(); ts2.begin(); ts3.begin(); ts4.begin(); ts5.begin(); ts6.begin();
  sensor_uncooledPV.begin(); sensor_cooledPV.begin(); sensor_pump.begin(); sensor_battery.begin();
  
  unsigned long startWait = millis();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000) { 
    delay(100); 
    }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    wifiStatus = "Online";
    sendTelegramMessage("System connected to WiFi. NTP Time Syncing...");
  }

}

void loop() {
  unsigned long currentMillis = millis();
  String currentTimeStr = getTimeString();
  int physicalBtnState = digitalRead(bttnOn);

  Blynk.run();

  // --- 1. WiFi & Time Restoration ---
  if (WiFi.status() != WL_CONNECTED) {
    wifiStatus = "Offline";
    if (currentMillis - lastWiFiRetry >= 10000) {
      WiFi.begin(ssid, password);
      lastWiFiRetry = currentMillis;
    }
  } 
  
  else{
    // Standard Blynk keep-alive
    Blynk.run(); 
    
    // TRIGGER: Runs ONLY when the system transitions back to Online
    if(wifiStatus == "Offline") {
      wifiStatus = "Reconnect";

      if(wifiStatus != lastWiFiStatus){
        sendTelegramMessage("System Reconnected to WiFi.");
      }

      lastWiFiStatus = wifiStatus;
      
      Blynk.config(BLYNK_AUTH_TOKEN);
      Blynk.run();

      // Time Resynchronizing
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

      // --- The Sync Window (5 Seconds) ---
      unsigned long syncWait = millis();
      struct tm timeinfo;
      while(millis() - syncWait < 5000) { 
        if(getLocalTime(&timeinfo)) {
          break; 
        }
        Blynk.run(); yield(); delay(100); 
      }
    }
  }

  // --- 2. Measurements ---
  if(currentMillis - lastIrradianceUpdate >= 15000) {
    irradianceMeasurement();
    lastIrradianceUpdate = currentMillis;
  }
  
  batteryStatus = (sensor_battery.getCurrent_mA() >= 0) ? "Discharging" : "Charging";
  updateOLED(cAtemp, luxPresent, currentTimeStr);

  // --- 3. Cooling Control ---
  if(luxPresent < 800) {
    systemStatus = "Sleep";
    digitalWrite(redled, LOW); digitalWrite(greenled, HIGH);
    digitalWrite(pump1, HIGH); digitalWrite(pump2, HIGH);
    pumpStatus = "OFF"; pumpIsRunning = false;

    if(systemStatus != lastSystemStatus){
      sendTelegramMessage("SLEEP MODE: Current Irradiance is " + String(luxPresent) + " W/m2.");
    }

    lastSystemStatus = systemStatus;

    if(physicalBtnState == LOW || telegramManualReq){
      sendTelegramMessage("Action Denied: Irradiance too low. System is SLEEP.");
      telegramManualReq = false;
    } 
  }

  else{
    systemStatus = "Active";
    digitalWrite(greenled, LOW); digitalWrite(redled, HIGH);
    
    if(systemStatus != lastSystemStatus){
    sendTelegramMessage("ACTIVE MODE: Current Irradiance is " + String(luxPresent) + " W/m2.");
    }

    lastSystemStatus = systemStatus;

    if(currentMillis - lastTempRequest >= 2000) {
      ts4.requestTemperatures(); ts5.requestTemperatures(); ts6.requestTemperatures();
      cAtemp = (ts4.getTempCByIndex(0) + ts5.getTempCByIndex(0) + ts6.getTempCByIndex(0)) / 3.0;
      lastTempRequest = currentMillis;
    }

    if ((cAtemp >= 35.0) && !pumpIsRunning && !pumpIsResting) {
      digitalWrite(pump1, LOW); digitalWrite(pump2, LOW);
      pumpStatus = "ON"; pumpIsRunning = true; pumpStartTime = currentMillis;
      
      String alert = "PV ALERT: Temperature reached " + String(cAtemp) + " °C. Pumps Activated.";
      sendTelegramMessage(alert);
      }

    else if((physicalBtnState == LOW || telegramManualReq) && !pumpIsRunning && !pumpIsResting){
      digitalWrite(pump1, LOW); digitalWrite(pump2, LOW);
      pumpStatus = "ON"; pumpIsRunning = true; pumpStartTime = currentMillis;

      String notify = "Pump activated manually.";
      sendTelegramMessage(notify);
      telegramManualReq = false;
    }
  }
  
  // --- 4. Pump Status Checking ---
  if(pumpIsRunning && currentMillis - pumpStartTime >= 30000) {
    digitalWrite(pump1, HIGH); digitalWrite(pump2, HIGH);
    pumpStatus = "OFF"; pumpIsRunning = false; pumpIsResting = true;
    cooldownStartTime = currentMillis;
            
    String resting = "Pump deactivated. Waiting for PV to cool down.";
    sendTelegramMessage(resting);
    }
    

  if(pumpIsResting && currentMillis - cooldownStartTime >= 120000) {
    pumpIsResting = false;

    String monitoring = "Cool down completed. Continue temperature monitoring. Current temperature:" + 
    String(cAtemp) + " °C";
    sendTelegramMessage(monitoring);
  }
  

  // --- 5. Reporting & Data Logging ---
  if(currentMillis - lastSerialLogTime >= 30000) { // Logging Data to GS every one minute
    sendDataToGoogle();
    lastSerialLogTime = currentMillis;
  }
  
  if(WiFi.status() == WL_CONNECTED && currentMillis - lastBlynkUpdate >= 5000) {
    Blynk.virtualWrite(V0, cAtemp);
    Blynk.virtualWrite(V1, currentTimeStr);
    Blynk.virtualWrite(V4, sensor_battery.getBusVoltage_V());
    Blynk.virtualWrite(V5, luxPresent);
    Blynk.virtualWrite(V2, pumpStatus);
    Blynk.virtualWrite(V3, systemStatus);
    lastBlynkUpdate = currentMillis;
  }

  
  if(millis() - lastTelegramCheck > 2000) { // Check every 2 seconds
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleTelegramRequest(cAtemp, luxPresent, numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTelegramCheck = millis();
  }

}
