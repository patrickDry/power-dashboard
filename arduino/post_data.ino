// For ESP32: uses WiFi.h + HTTPClient.h (built into ESP32 board package)
// For ESP8266: replace with ESP8266WiFi.h + ESP8266HTTPClient.h

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Install via Library Manager: "ArduinoJson" by Benoit Blanchon

const char* WIFI_SSID     = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";
const char* SERVER_URL    = "http://192.168.1.x:3000/api/data";  // your PC's local IP

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    postData();
  }
  delay(30000);  // post every 30 seconds
}

void postData() {
  // --- Read your actual sensor values here ---
  float batterySoc     = 87.5;
  float batteryVoltage = 52.4;
  float batteryCharge  = 1.2;
  float acLoadCurrent  = 0.85;
  float acLoadTotal    = 6.3;
  float solar          = 2.1;
  float hydro          = 0.8;
  float diesel         = 0.0;
  float bufferTank     = 72.0;
  float flueTemp       = 185.0;
  float flowTemp       = 68.0;
  float returnTemp     = 45.0;

  // Build JSON
  JsonDocument doc;
  doc["battery"]["stateOfCharge"] = batterySoc;
  doc["battery"]["voltage"]       = batteryVoltage;
  doc["battery"]["charge"]        = batteryCharge;
  doc["acLoad"]["current"]        = acLoadCurrent;
  doc["acLoad"]["totalToday"]     = acLoadTotal;
  doc["production"]["solar"]      = solar;
  doc["production"]["hydro"]      = hydro;
  doc["production"]["diesel"]     = diesel;
  doc["heating"]["bufferTankCharge"]  = bufferTank;
  doc["heating"]["flueTemperature"]   = flueTemp;
  doc["heating"]["flowTemperature"]   = flowTemp;
  doc["heating"]["returnTemperature"] = returnTemp;

  String body;
  serializeJson(doc, body);

  // POST
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  if (code > 0) {
    Serial.printf("POST %d: %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}
