#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <PubSubClient.h>
#include <ui.h>

#include <lvgl.h>
#include "lvgl_v8_port.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ArduinoModbus.h>
#include <WebServer.h>
#include <HTTPClient.h>

using namespace esp_panel::drivers;
using namespace esp_panel::board;

struct {
  float batterySoc;
  float batteryVoltage;
  float batteryCharge;
  float acLoadCurrent;
  float acLoadTotal;
  float solar;
  float hydro;
  float diesel;
  float bufferTankCharge;
  float flueTemp;
  float flowTemp;
  float returnTemp;
  unsigned long updatedAt;  // millis() of last update
} readings;

const char* CLOUD_API_URL    = "https://fxm27v6wq4.execute-api.us-east-2.amazonaws.com/data";  // ← API Gateway URL
const char* RELAY_API_URL = "https://fxm27v6wq4.execute-api.us-east-2.amazonaws.com/relay";

const int udpPort = 3333;
char incomingPacket[256];
//Are we currently connected?
bool connected = false;

char reusableString[128];

//The udp library class
WiFiUDP udp;
const char * networkName = "internet";
// Set your Static IP address
IPAddress local_IP(10, 0, 0, 52);
// Set your Gateway IP address
IPAddress gateway(10, 0, 0, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(10, 0, 0, 1);   // optional  

const unsigned long REPORT_INTERVAL_UDP = 1000;
const unsigned long REPORT_INTERVAL_SELECTLIVE = 63000;
const unsigned long REPORT_INTERVAL_PUSH = 5000;
const unsigned long REPORT_INTERVAL_BOILER = 60000;
WiFiClient wifiClient;

IPAddress boiler(10, 0, 0, 2);
IPAddress boilerRelayAddress(10, 0, 0, 11);
ModbusTCPClient modbusTCPClient(wifiClient);

bool shouldLoadDump = false;
bool isLoadDumping = false;
float loadDumpLowVoltage = 480;
float loadDumpHighVoltage = 520;
float batteryVoltage = 0;

WebServer server(80);
unsigned long last_read_udp = 0;
unsigned long last_read_boiler = 0;
unsigned long last_read_selectlive = 0;
unsigned long last_push = 0;

// ── HTML dashboard (served at /) ──────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Power Dashboard</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0f1117; color: #fff; font-family: system-ui, sans-serif; padding: 16px; }
  header { display: flex; align-items: center; gap: 8px; padding-bottom: 16px; border-bottom: 1px solid #2a2d3a; margin-bottom: 20px; }
  header h1 { font-size: 1.1rem; font-weight: 600; }
  .cards { display: flex; flex-direction: column; gap: 16px; max-width: 480px; margin: 0 auto; }
  .card { background: #1a1d27; border: 1px solid #2a2d3a; border-radius: 16px; padding: 20px; display: flex; flex-direction: column; gap: 16px; }
  .card-title { font-size: 0.65rem; font-weight: 700; text-transform: uppercase; letter-spacing: 0.12em; color: rgba(255,255,255,0.4); display: flex; align-items: center; gap: 6px; }
  .big-value { font-size: 3rem; font-weight: 700; font-variant-numeric: tabular-nums; line-height: 1; }
  .big-unit { font-size: 1.5rem; color: rgba(255,255,255,0.35); }
  .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 12px; }
  .stat-label { font-size: 0.7rem; color: rgba(255,255,255,0.4); margin-bottom: 2px; }
  .stat-value { font-size: 1.4rem; font-weight: 600; font-variant-numeric: tabular-nums; }
  .stat-unit { font-size: 0.8rem; color: rgba(255,255,255,0.45); }
  .stat-sub { font-size: 0.7rem; color: rgba(255,255,255,0.3); margin-top: 1px; }
  .gauge-track { width: 100%; height: 16px; border-radius: 999px; background: rgba(255,255,255,0.1); overflow: hidden; }
  .gauge-fill { height: 100%; border-radius: 999px; transition: width 0.5s ease; }
  .load-track { width: 100%; height: 16px; border-radius: 999px; background: rgba(255,255,255,0.1); overflow: hidden; margin-top: 4px; }
  .load-fill { height: 100%; border-radius: 999px; transition: width 0.5s ease; background: linear-gradient(to right, #22c55e, #f97316); }
  .scale { display: flex; justify-content: space-between; font-size: 0.7rem; color: rgba(255,255,255,0.3); margin-top: 4px; }
  .prod-bar { display: flex; width: 100%; height: 16px; border-radius: 999px; overflow: hidden; gap: 2px; background: rgba(255,255,255,0.1); }
  .prod-seg { height: 100%; transition: width 0.5s ease; }
  .legend { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 4px; }
  .legend-item { display: flex; align-items: center; gap: 5px; font-size: 0.7rem; }
  .legend-dot { width: 10px; height: 10px; border-radius: 2px; }
  .legend-val { color: #fff; font-weight: 600; }
  .legend-label { color: rgba(255,255,255,0.45); }
  .status { font-size: 0.7rem; color: rgba(255,255,255,0.3); text-align: right; margin-bottom: 4px; }
  .row-between { display: flex; align-items: flex-end; justify-content: space-between; }
</style>
</head>
<body>
<header><span>⚡</span><h1>Power Dashboard</h1></header>
<p class="status" id="status">Connecting…</p>
<div class="cards">

  <!-- Battery -->
  <div class="card">
    <div class="card-title"><span>🔋</span> Battery</div>
    <div>
      <span class="big-value" id="bSoc">--</span><span class="big-unit">%</span>
    </div>
    <div class="gauge-track"><div class="gauge-fill" id="bSocBar" style="width:0%"></div></div>
    <div class="grid-2">
      <div>
        <div class="stat-label">Voltage</div>
        <span class="stat-value" id="bVolt">--</span> <span class="stat-unit">V</span>
      </div>
      <div>
        <div class="stat-label">Power</div>
        <span class="stat-value" id="bCharge">--</span> <span class="stat-unit">kW</span>
        <div class="stat-sub" id="bChargeSub"></div>
      </div>
    </div>
  </div>

  <!-- AC Load -->
  <div class="card">
    <div class="card-title"><span>⚡</span> AC Load</div>
    <div class="row-between">
      <div>
        <span class="big-value" id="acCurrent">--</span><span class="big-unit"> kW</span>
      </div>
      <div style="text-align:right">
        <div class="stat-label">Total today</div>
        <span class="stat-value" id="acTotal">--</span> <span class="stat-unit">kWh</span>
      </div>
    </div>
    <div>
      <div class="load-track"><div class="load-fill" id="acBar" style="width:0%"></div></div>
      <div class="scale"><span>0 kW</span><span>7.5 kW</span></div>
    </div>
  </div>

  <!-- Production -->
  <div class="card">
    <div class="card-title"><span>🌿</span> Power Production</div>
    <div>
      <div class="stat-label">Total generation</div>
      <span class="stat-value" id="prodTotal">--</span> <span class="stat-unit">kW</span>
    </div>
    <div class="prod-bar">
      <div class="prod-seg" id="prodSolarSeg" style="background:#facc15;width:0%"></div>
      <div class="prod-seg" id="prodHydroSeg" style="background:#3b82f6;width:0%"></div>
      <div class="prod-seg" id="prodDieselSeg" style="background:#64748b;width:0%"></div>
    </div>
    <div class="legend">
      <div class="legend-item">
        <div class="legend-dot" style="background:#facc15"></div>
        <span class="legend-label">Solar</span>
        <span class="legend-val" id="prodSolar">--</span>
        <span class="legend-label">kW</span>
      </div>
      <div class="legend-item">
        <div class="legend-dot" style="background:#3b82f6"></div>
        <span class="legend-label">Hydro</span>
        <span class="legend-val" id="prodHydro">--</span>
        <span class="legend-label">kW</span>
      </div>
      <div class="legend-item">
        <div class="legend-dot" style="background:#64748b"></div>
        <span class="legend-label">Diesel</span>
        <span class="legend-val" id="prodDiesel">--</span>
        <span class="legend-label">kW</span>
      </div>
    </div>
  </div>

  <!-- Heating -->
  <div class="card">
    <div class="card-title"><span>🔥</span> Heating System</div>
    <div>
      <span class="big-value" id="hTank">--</span><span class="big-unit">%</span>
    </div>
    <div class="gauge-track"><div class="gauge-fill" id="hTankBar" style="width:0%"></div></div>
    <div class="grid-3">
      <div>
        <div class="stat-label">Flue temp</div>
        <span class="stat-value" id="hFlue">--</span> <span class="stat-unit">°C</span>
      </div>
      <div>
        <div class="stat-label">Flow temp</div>
        <span class="stat-value" id="hFlow">--</span> <span class="stat-unit">°C</span>
      </div>
      <div>
        <div class="stat-label">Return temp</div>
        <span class="stat-value" id="hReturn">--</span> <span class="stat-unit">°C</span>
      </div>
    </div>
  </div>

</div>
<script>
function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }
function fmt(v, d=2) { return Number(v).toFixed(d); }

function socColor(pct) {
  if (pct > 60) return '#22c55e';
  if (pct > 30) return '#fbbf24';
  return '#ef4444';
}
function tankColor(pct) {
  if (pct > 60) return '#f97316';
  if (pct > 30) return '#fbbf24';
  return '#60a5fa';
}

function update(d) {
  // Battery
  document.getElementById('bSoc').textContent = fmt(d.batterySoc, 1);
  const socBar = document.getElementById('bSocBar');
  socBar.style.width = clamp(d.batterySoc, 0, 100) + '%';
  socBar.style.background = socColor(d.batterySoc);
  document.getElementById('bVolt').textContent = fmt(d.batteryVoltage, 1);
  document.getElementById('bCharge').textContent = fmt(d.batteryCharge);
  document.getElementById('bChargeSub').textContent = d.batteryCharge >= 0 ? 'charging' : 'discharging';

  // AC Load
  document.getElementById('acCurrent').textContent = fmt(d.acLoadCurrent);
  document.getElementById('acTotal').textContent = fmt(d.acLoadTotal);
  document.getElementById('acBar').style.width = clamp((d.acLoadCurrent / 7.5) * 100, 0, 100) + '%';

  // Production
  const total = d.solar + d.hydro + d.diesel;
  document.getElementById('prodTotal').textContent = fmt(total);
  document.getElementById('prodSolar').textContent = fmt(d.solar);
  document.getElementById('prodHydro').textContent = fmt(d.hydro);
  document.getElementById('prodDiesel').textContent = fmt(d.diesel);
  const pct = s => total > 0 ? clamp((s / total) * 100, 0, 100) + '%' : '0%';
  document.getElementById('prodSolarSeg').style.width = pct(d.solar);
  document.getElementById('prodHydroSeg').style.width = pct(d.hydro);
  document.getElementById('prodDieselSeg').style.width = pct(d.diesel);

  // Heating
  document.getElementById('hTank').textContent = fmt(d.bufferTankCharge, 1);
  const tankBar = document.getElementById('hTankBar');
  tankBar.style.width = clamp(d.bufferTankCharge, 0, 100) + '%';
  tankBar.style.background = tankColor(d.bufferTankCharge);
  document.getElementById('hFlue').textContent = fmt(d.flueTemp, 1);
  document.getElementById('hFlow').textContent = fmt(d.flowTemp, 1);
  document.getElementById('hReturn').textContent = fmt(d.returnTemp, 1);

  document.getElementById('status').textContent =
    'Last updated ' + new Date().toLocaleTimeString();
}

async function poll() {
  try {
    const res = await fetch('/data');
    if (res.ok) update(await res.json());
  } catch(e) {
    document.getElementById('status').textContent = 'Connection error — retrying…';
  }
}

poll();
setInterval(poll, 5000);
</script>
</body>
</html>)html";

/**
 * To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 */
 // #include <demos/lv_demos.h>
 // #include <examples/lv_examples.h>

void setup()
{
    Serial.begin(115200);

    Serial.println("Initializing board");
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    // When avoid tearing function is enabled, the frame buffer number should be set in the board driver
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
     * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
     * "bounce buffer" functionality to enhance the RGB data bandwidth.
     * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
     */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20);
    }
#endif
#endif
    assert(board->begin());
    lvgl_port_init(board->getLCD(), board->getTouch());

    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

  
    /**
     * Try an example. Don't forget to uncomment header.
     * See all the examples online: https://docs.lvgl.io/master/examples.html
     * source codes: https://github.com/lvgl/lvgl/tree/e7f88efa5853128bf871dde335c0ca8da9eb7731/examples
     */
    //  lv_example_btn_1();

    /**
     * Or try out a demo.
     * Don't forget to uncomment header and enable the demos in `lv_conf.h`. E.g. `LV_USE_DEMO_WIDGETS`
     */
    //lv_demo_widgets();
    // lv_demo_benchmark();
    // lv_demo_music();
    // lv_demo_stress();

    ui_init();
    /* Release the mutex */
    lvgl_port_unlock();

    connectToWiFi(networkName);
    /*
    // Uncomment for local webserver
    server.on("/",     handleRoot);
    server.on("/data", handleData);
    server.begin();
    */
    update_dumping_ui_active(false);
}

void loop()
{
    //mqtt.loop();
    server.handleClient();
    if (millis() - last_read_udp >= REPORT_INTERVAL_UDP) {
      last_read_udp = millis();
      //readSensors();
      //randomizeUI();
      do_udp_and_relay_checks();
  }
  if (millis() - last_read_selectlive >= REPORT_INTERVAL_SELECTLIVE) {
    if (connected) {
      readSelectLive();
    }
     last_read_selectlive = millis();
  }
  if (millis() - last_push >= REPORT_INTERVAL_PUSH) {
    if (connected) {
      pushToCloud();
    }
     last_push = millis();
  }
  if (millis() - last_read_boiler > REPORT_INTERVAL_BOILER && connected) {
      last_read_boiler = millis();
      if (connected) {
        queryBoiler();
      }
    }
}


void do_udp_and_relay_checks() {
   if(connected){
      // Parse UDP Packet
      int packetSize = udp.parsePacket();
      if (packetSize)
      {
        //Serial.printf("Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
        int len = udp.read(incomingPacket, 255);

        // Packet should be 14 bytes
        if (len == 14)
        {
          int i = 0;
          batteryVoltage = unpackShort(incomingPacket, i);
          i+=2;

          short batterySoC = unpackShort(incomingPacket, i);
          i+=2;
          
          short batteryPower = unpackShort(incomingPacket, i);
          i+=2;
          
          short acLoad = unpackShort(incomingPacket, i);
          i+=2;
          
          short acPower = unpackShort(incomingPacket, i);
          i+=2;
          
          short hydroPower = unpackShort(incomingPacket, i);
          i+=2;
          
          short solarPower = unpackShort(incomingPacket, i);

          updateUI(batteryVoltage, batterySoC, batteryPower, acLoad, acPower, hydroPower, solarPower);
        }

        if (isLoadDumping && batteryVoltage <= loadDumpLowVoltage)
        {
          isLoadDumping = false;
          boilerRelay(false);
        }
        else if (shouldLoadDump && isLoadDumping == false && batteryVoltage >= loadDumpHighVoltage)
        {
          isLoadDumping = true;
          boilerRelay(true);
        }
      }
   }
}

void pushToCloud() {
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
      "\"returnTemp\":%.1f,"
      "\"relayActualState\":%s"
    "}",
    readings.batterySoc, readings.batteryVoltage, -readings.batteryCharge,
    readings.acLoadCurrent, readings.acLoadTotal,
    readings.solar, readings.hydro, readings.diesel,
    readings.bufferTankCharge, readings.flueTemp,
    readings.flowTemp, readings.returnTemp,
    shouldLoadDump ? "true" : "false"
  );

  {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.begin(secureClient, CLOUD_API_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(2000);


    int code = http.POST(payload);
    
    if (code == 200) {
      String resp = http.getString();
     
      // The Lambda returns the pending dashboard command in the response.
      // Only act when the user explicitly changed it (one-shot flag).
      if (resp.indexOf("\"relayDashboardChanged\":true") >= 0) {
        bool newState = resp.indexOf("\"relayDashboardState\":true") >= 0;

        shouldLoadDump = newState;
        if (shouldLoadDump == false) {
          lv_obj_clear_state(ui_LoadDumpSwitch, LV_STATE_CHECKED);
           if (isLoadDumping)
            {
            boilerRelay(false);
           isLoadDumping = false;
          }
        }
        else {
          lv_obj_add_state(ui_LoadDumpSwitch, LV_STATE_CHECKED);
        }
        Serial.printf("Dashboard relay override: %s\n", newState ? "ON" : "OFF");
      }
    }
     //Serial.printf("heap=%u largest=%u\n",
     //ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    http.end();
  }
}

void checkDashboardRelay() {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient http;
  http.begin(secureClient, RELAY_API_URL);
  http.setTimeout(2000);

  int code = http.GET();
  Serial.println(code);
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    Serial.println(body);
    // Only act if dashboard flagged a change
    if (body.indexOf("\"relayDashboardChanged\":true") >= 0) {
      bool newState = body.indexOf("\"relayDashboardState\":true") >= 0;
      if (newState != shouldLoadDump) {
        shouldLoadDump = newState;
        if (shouldLoadDump == false) {
          lv_obj_clear_state(ui_LoadDumpSwitch, LV_STATE_CHECKED);
           if (isLoadDumping)
            {
            boilerRelay(false);
           isLoadDumping = false;
          }
        }
        else {
          lv_obj_add_state(ui_LoadDumpSwitch, LV_STATE_CHECKED);
        }
      }
      Serial.printf("Dashboard relay override: %s\n", newState ? "ON" : "OFF");

      // Clear the changed flag
      http.end();
      http.begin(secureClient, RELAY_API_URL);
      http.sendRequest("DELETE", "");
    }
  }
  http.end();
}


void randomizeUI()
{
  short batteryVoltage = random(0,570);
  short batterySoC = random(700, 1000);
  short batteryPower = random(-400, 400);
  short acLoad = random(0, 750);
  short acPower = random(0, 360);
  short hydroPower = random(0, 150);
  short solarPower = random(0, 820);

  updateUI(batteryVoltage, batterySoC, batteryPower, acLoad, -acPower, hydroPower, solarPower);
}

void updateUI(short batteryVoltage, short batterySoC, short batteryPower, short acLoad, short acPower, short hydroPower, short solarPower)
{
  lvgl_port_lock(-1);
  float batterySoCFloat = batterySoC / 10.0;
  float batterySoCHue = 0;
  if (batterySoCFloat > 30)
  { 
    batterySoCHue = (batterySoCFloat - 30) * 0.01 * 185;
  }
  snprintf(reusableString, 128, "%u%c", batterySoC / 10, 37);
  lv_label_set_text(ui_BatterySoCLabel, reusableString);
  lv_arc_set_value(ui_BatterySoCIndicator, batterySoCFloat);
  lv_obj_set_style_arc_color(ui_BatterySoCIndicator, lv_color_hsv_to_rgb(batterySoCHue, 80, 90), LV_PART_INDICATOR);
  
  snprintf(reusableString, 128, "%.1fv", batteryVoltage / 10.0);
  lv_label_set_text(ui_BatteryVoltageLabel, reusableString);

  snprintf(reusableString, 128, "%.2f", -batteryPower / 100.0);
  lv_label_set_text(ui_BatteryChargeLabel, reusableString);

  if (batteryPower > 0)
  {
    float batteryPowerValue = constrain(-(float)batteryPower / 400.0, -1, 0);
    lv_arc_set_value(ui_BatteryChargeIndicator, batteryPowerValue * 50);
    //lv_slider_set_left_value(ui_BatteryChargeIndicator, (1 - batteryPowerValue) * 50, LV_ANIM_OFF);
    lv_obj_set_style_arc_color(ui_BatteryChargeIndicator, lv_color_hsv_to_rgb(10, 80, 90), LV_PART_INDICATOR);
  }
  else
  {
    float batteryPowerValue = constrain(-(float)batteryPower / 400.0, 0, 1);
    lv_arc_set_value(ui_BatteryChargeIndicator, batteryPowerValue * 50);
    //lv_slider_set_left_value(ui_BatteryChargeIndicator, 50, LV_ANIM_OFF);
    lv_obj_set_style_arc_color(ui_BatteryChargeIndicator, lv_color_hsv_to_rgb(130, 80, 90), LV_PART_INDICATOR);
  }

  snprintf(reusableString, 128, "%.2f", acLoad / 100.0);
  lv_label_set_text(ui_LoadNow, reusableString);
  float acLoadValue = constrain((float)acLoad / 750.0, 0, 1);
  lv_arc_set_value(ui_LoadIndicator, acLoadValue * 100);
  lv_obj_set_style_arc_color(ui_LoadIndicator, lv_color_hsv_to_rgb((1 - acLoadValue) * 150, 80, 90), LV_PART_INDICATOR | LV_STATE_DEFAULT);

  snprintf(reusableString, 128, "%.2f", (-acPower + hydroPower + solarPower) / 100.0);
  lv_label_set_text(ui_PowerTotal, reusableString);

  snprintf(reusableString, 128, "%.2f", -acPower / 100.0);
  lv_label_set_text(ui_PowerGenerator, reusableString);

  snprintf(reusableString, 128, "%.2f", hydroPower / 100.0);
  lv_label_set_text(ui_PowerHydro1, reusableString);

  snprintf(reusableString, 128, "%.2f", solarPower / 100.0);
  lv_label_set_text(ui_PowerSolar, reusableString);

  //snprintf(reusableString, 128, "%.2f", (solarPower + hydroPower + acPower) / 100.0);
  //lv_label_set_text(ui_TotalLabel, reusableString);
  lvgl_port_unlock();

  readings.batterySoc      = batterySoCFloat;
  readings.batteryVoltage  = batteryVoltage / 10.0;
  readings.batteryCharge   = batteryPower / 100.0;
  readings.acLoadCurrent   = acLoad / 100.0;
 
  readings.solar           = solarPower / 100.0;
  readings.hydro           = hydroPower / 100.0;
  readings.diesel          = -acPower / 100.0;

  readings.updatedAt       = millis();
}

void updateUIBoiler(short bufferCharge, short bufferHi, short bufferMid, short bufferLo, short boilerFlu, short boilerFlow, short boilerReturn)
{
  lvgl_port_lock(-1);
  snprintf(reusableString, 128, "%u%c", bufferCharge / 10, 37);
  lv_label_set_text(ui_BoilerBufferSoC, reusableString);
  
  snprintf(reusableString, 128, "%u%c", boilerFlu / 10, 39);
  lv_label_set_text(ui_BoilerFlueTemp, reusableString);

  snprintf(reusableString, 128, "%u%c", boilerFlow / 10, 39);
  lv_label_set_text(ui_BoilerFlowTemp, reusableString);

  snprintf(reusableString, 128, "%u%c", boilerReturn / 10, 39);
  lv_label_set_text(ui_BoilerReturnTemp, reusableString);
  lvgl_port_unlock();

  readings.bufferTankCharge= bufferCharge / 10;
  readings.flueTemp        = boilerFlu / 10;
  readings.flowTemp        = boilerFlow / 10;
  readings.returnTemp      = boilerReturn / 10;
}

short unpackShort(char buf[14], int index)
{
  return buf[index]<<8 | buf[index + 1];
}

void connectToWiFi(const char * ssid){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  WiFi.config(local_IP, gateway, subnet, primaryDNS);

  //Initiate connection
  WiFi.begin(ssid);

  Serial.println("Waiting for WIFI connection...");
}

float* hsv2rgb(float h, float s, float b, float* rgb) {
  rgb[0] = b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  rgb[1] = b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  rgb[2] = b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  return rgb;
}

float fract(float x) { return x - int(x); }
float mix(float a, float b, float t) { return a + (b - a) * t; }
float step(float e, float x) { return x < e ? 0.0 : 1.0; }

//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          //When connected set 
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          Serial.println(udp.begin(udpPort));
          connected = true;
          boilerRelay(false);
          last_read_boiler = millis() - REPORT_INTERVAL_BOILER + 2000;
          //queryBoiler();
          break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
      default: break;
    }
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  char json[512];
  snprintf(json, sizeof(json),
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
      "\"returnTemp\":%.1f,"
      "\"updatedAt\":%lu"
    "}",
    readings.batterySoc, readings.batteryVoltage, -readings.batteryCharge,
    readings.acLoadCurrent, readings.acLoadTotal,
    readings.solar, readings.hydro, readings.diesel,
    readings.bufferTankCharge, readings.flueTemp,
    readings.flowTemp, readings.returnTemp,
    readings.updatedAt
  );
  server.send(200, "application/json", json);
}

void readSelectLive() {
  float loadWh = fetchLoadWhToday();
  if (loadWh >= 0) {
    readings.acLoadTotal = loadWh;
    snprintf(reusableString, 128, "%.1f kWh", loadWh);
    lvgl_port_lock(-1);
    lv_label_set_text(ui_LoadToday, reusableString);
    lvgl_port_unlock();
  }
}

// Parses the number following "key": directly out of the raw buffer with no
// heap allocation or string copies. Returns -1 if the key isn't found.
float extractJsonFloat(const char* json, const char* key) {
  char needle[64];
  snprintf(needle, sizeof(needle), "\"%s\"", key);

  const char* p = strstr(json, needle);
  if (!p) return -1;

  p = strchr(p + strlen(needle), ':');
  if (!p) return -1;
  p++;  // skip the colon

  while (*p && isspace((unsigned char)*p)) p++;  // skip whitespace

  return strtod(p, nullptr);  // stops at first non-numeric char, no copy
}

float fetchLoadWhToday() {
  HTTPClient http;
  http.begin("http://10.0.0.3/cgi-bin/solarmonweb/devices/D4F8D7ACF28882CC0234930108B87FDA/point");
  http.setTimeout(3000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Remote fetch failed: %d\n", code);
    http.end();
    return -1;
  }

  String body = http.getString();
  http.end();

  float val = extractJsonFloat(body.c_str(), "load_wh_today");
  if (val < 0) Serial.println("Remote: load_wh_today not found");
  return val;
}

void queryBoiler()
{
  if (!modbusTCPClient.begin(boiler)) {
          Serial.println("Modbus TCP Client failed to connect!");
        } else {
          //Serial.println("Modbus TCP Client connected");

          int count = modbusTCPClient.requestFrom(0x01, HOLDING_REGISTERS, 1000, 14);
          if (count == 0)
          {
            Serial.print(modbusTCPClient.lastError());
          }
          else
          {
            short bufferPercentage = 0;
            short boilerFlow = 0;
            short boilerReturn = 0;
            short boilerFlu = 0;
            short bufferHi = 0;
            short bufferMid = 0;
            short bufferLo = 0;
            for(int i = 0; i < count; i++)
            {
              short value = modbusTCPClient.read();
              switch(i)
              {
                case 1: bufferPercentage = value; break;
                case 3: bufferHi = value; break;
                case 5: bufferMid = value; break;
                case 7: bufferLo = value; break;
                case 9: boilerFlow = value; break;
                case 11: boilerReturn = value; break;
                case 13: boilerFlu = value; break;
              }
            }
            modbusTCPClient.stop();
            updateUIBoiler(bufferPercentage, bufferHi, bufferMid, bufferLo, boilerFlu, boilerFlow, boilerReturn);
          }
        }
}

void dump_dropdown_changed(lv_event_t * e) {

}

void dumpVoltageSliderChanged(lv_event_t * e)
{
  loadDumpLowVoltage = lv_slider_get_left_value(ui_DumpSlider);
  loadDumpHighVoltage = lv_slider_get_value(ui_DumpSlider);

  snprintf(reusableString, 128, "%.1f", loadDumpLowVoltage / 10.0);
  lv_label_set_text(ui_DumpLevelLow, reusableString);

  snprintf(reusableString, 128, "%.1f", loadDumpHighVoltage / 10.0);
  lv_label_set_text(ui_DumpLevelHigh, reusableString);
}

void dumpLoadEnabledSwitchChanged(lv_event_t * e)
{
  shouldLoadDump = lv_obj_has_state(ui_LoadDumpSwitch, LV_STATE_CHECKED);
  Serial.println(shouldLoadDump);
  if (shouldLoadDump)
  {
    //lv_obj_clear_flag(ui_LoadDumpingEnabledContainer, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    if (isLoadDumping)
    {
      boilerRelay(false);
      isLoadDumping = false;
    }
    //lv_obj_add_flag(ui_LoadDumpingEnabledContainer, LV_OBJ_FLAG_HIDDEN);
  }
}

void boilerRelay(bool on)
{
  Serial.print("Element ");
  Serial.println(on);

  if (WiFi.isConnected())
  {
    WiFiClient client;
    if (client.connect(boilerRelayAddress, 80)) {
      String s;
      if (on)
      {
        s = "GET /relay/0?turn=on HTTP/1.1";
        update_dumping_ui_active(true);
      }
      else
      {
        s = "GET /relay/0?turn=off HTTP/1.1";
        update_dumping_ui_active(false);
      }
      client.println(s);
      client.println("Host: 10.0.0.11");
      client.println("Connection: close");
      client.println("");

      client.stop();
    }
    else
    {
      Serial.println("Down with Shelly!");
    }
  }
}

void update_dumping_ui_active(bool on) {
  lvgl_port_lock(-1);
  if (on)
  {
    lv_obj_clear_flag(ui_DumpingActive, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_add_flag(ui_DumpingActive, LV_OBJ_FLAG_HIDDEN);
  }
  lvgl_port_unlock();
}