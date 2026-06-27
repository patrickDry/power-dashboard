// Additional library needed: PubSubClient by Nick O'Leary (Library Manager)
// WiFiClientSecure is built into the ESP32 board package

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* WIFI_SSID       = "your-wifi-ssid";
const char* WIFI_PASSWORD   = "your-wifi-password";
const char* MQTT_BROKER     = "xxxxxxxxxxxxxxx-ats.iot.eu-west-1.amazonaws.com";  // your IoT endpoint
const char* MQTT_TOPIC      = "power/readings";
const char* MQTT_CLIENT_ID  = "esp32-power-dashboard";

// ── AWS IoT certificates ───────────────────────────────────────────────────────
// Download these from AWS IoT Core when you create your Thing
const char AWS_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
... paste AmazonRootCA1.pem here ...
-----END CERTIFICATE-----
)EOF";

const char DEVICE_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
... paste your device certificate here ...
-----END CERTIFICATE-----
)EOF";

const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
... paste your private key here ...
-----END RSA PRIVATE KEY-----
)EOF";

WiFiClientSecure net;
PubSubClient mqtt(net);

void connectMQTT() {
  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(DEVICE_CERT);
  net.setPrivateKey(PRIVATE_KEY);
  mqtt.setServer(MQTT_BROKER, 8883);
  mqtt.setBufferSize(512);

  Serial.print("Connecting to AWS IoT");
  while (!mqtt.connected()) {
    if (mqtt.connect(MQTT_CLIENT_ID)) {
      Serial.println(" connected");
    } else {
      Serial.printf(" failed (%d), retrying in 5s\n", mqtt.state());
      delay(5000);
    }
  }
}

void publishReadings() {
  if (!mqtt.connected()) connectMQTT();

  char payload[512];
  snprintf(payload, sizeof(payload),
    "{"
      "\"batterySoc\":%.1f,"
      "\"batteryVoltage\":%.1f,"
      "\"batteryCharge\":%.2f,"
      "\"acLoadCurrent\":%.2f,"
      "\"acLoadTotal\":%.2f,"
      "\"solar\":%.2f,"
      "\"hydro\":%.2f,"
      "\"diesel\":%.2f,"
      "\"bufferTankCharge\":%.1f,"
      "\"flueTemp\":%.1f,"
      "\"flowTemp\":%.1f,"
      "\"returnTemp\":%.1f"
    "}",
    readings.batterySoc, readings.batteryVoltage, readings.batteryCharge,
    readings.acLoadCurrent, readings.acLoadTotal,
    readings.solar, readings.hydro, readings.diesel,
    readings.bufferTankCharge, readings.flueTemp,
    readings.flowTemp, readings.returnTemp
  );

  mqtt.publish(MQTT_TOPIC, payload);
}

void loop() {
  mqtt.loop();

  static unsigned long lastPublish = 0;
  if (millis() - lastPublish >= 5000) {
    lastPublish = millis();
    readSensors();
    publishReadings();
  }
}
