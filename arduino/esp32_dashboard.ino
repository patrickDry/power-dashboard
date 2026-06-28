// Libraries: all built into the ESP32 board package — no extra installs needed.
// In Arduino IDE: Tools → Board → ESP32 Arduino → your board

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

const char* WIFI_SSID        = "your-wifi-ssid";
const char* WIFI_PASSWORD    = "your-wifi-password";
const char* REMOTE_DATA_URL  = "http://192.168.1.x/your-endpoint";  // ← your local server
const char* DUCKDNS_DOMAIN   = "yourname";                           // ← just the subdomain, not the full URL
const char* DUCKDNS_TOKEN    = "your-token-here";
const char* CLOUD_API_URL    = "https://xxxxxxxxxx.execute-api.us-east-2.amazonaws.com/data";  // ← API Gateway URL

#define DUCKDNS_INTERVAL_MS 300000  // update every 5 minutes

WebServer server(80);

// ── Latest readings ───────────────────────────────────────────────────────────
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
  document.getElementById('bCharge').textContent = fmt(Math.abs(d.batteryCharge));
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
    readings.batterySoc, readings.batteryVoltage, readings.batteryCharge,
    readings.acLoadCurrent, readings.acLoadTotal,
    readings.solar, readings.hydro, readings.diesel,
    readings.bufferTankCharge, readings.flueTemp,
    readings.flowTemp, readings.returnTemp,
    readings.updatedAt
  );
  server.send(200, "application/json", json);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Read sensors and push to cloud every 5 seconds
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 5000) {
    lastRead = millis();
    readSensors();
    pushToCloud();
  }

  // Update DuckDNS every 5 minutes
  static unsigned long lastDuckDNS = 0;
  if (millis() - lastDuckDNS >= DUCKDNS_INTERVAL_MS) {
    lastDuckDNS = millis();
    updateDuckDNS();
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
      "\"returnTemp\":%.1f"
    "}",
    readings.batterySoc, readings.batteryVoltage, readings.batteryCharge,
    readings.acLoadCurrent, readings.acLoadTotal,
    readings.solar, readings.hydro, readings.diesel,
    readings.bufferTankCharge, readings.flueTemp,
    readings.flowTemp, readings.returnTemp
  );

  HTTPClient http;
  http.begin(CLOUD_API_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int code = http.POST(payload);
  if (code > 0) {
    Serial.printf("Cloud push: %d\n", code);
  } else {
    Serial.printf("Cloud push failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

void updateDuckDNS() {
  HTTPClient http;
  String url = "http://www.duckdns.org/update?domains=";
  url += DUCKDNS_DOMAIN;
  url += "&token=";
  url += DUCKDNS_TOKEN;
  url += "&ip=";  // blank = auto-detect public IP

  http.begin(url);
  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code == HTTP_CODE_OK && response.startsWith("OK")) {
    Serial.println("DuckDNS updated OK");
  } else {
    Serial.printf("DuckDNS failed: %d %s\n", code, response.c_str());
  }
}

// Finds the first occurrence of "key": <number> in a JSON string and returns
// the number. Returns -1 if the key is not found.
float extractJsonFloat(const String& json, const char* key) {
  String needle = "\"";
  needle += key;
  needle += "\"";

  int keyIdx = json.indexOf(needle);
  if (keyIdx == -1) return -1;

  int colonIdx = json.indexOf(':', keyIdx + needle.length());
  if (colonIdx == -1) return -1;

  // Skip whitespace after the colon
  int numStart = colonIdx + 1;
  while (numStart < (int)json.length() && isspace(json[numStart])) numStart++;

  return json.substring(numStart).toFloat();
}

float fetchLoadWhToday() {
  HTTPClient http;
  http.begin(REMOTE_DATA_URL);
  http.setTimeout(3000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Remote fetch failed: %d\n", code);
    http.end();
    return -1;
  }

  String body = http.getString();
  http.end();

  float val = extractJsonFloat(body, "load_wh_today");
  if (val < 0) Serial.println("Remote: load_wh_today not found");
  return val;
}

void readSensors() {
  // Replace these with your actual sensor reads
  readings.batterySoc      = 87.5;
  readings.batteryVoltage  = 52.4;
  readings.batteryCharge   = 1.2;
  readings.acLoadCurrent   = 0.85;
  float loadWh = fetchLoadWhToday();
  if (loadWh >= 0) readings.acLoadTotal = loadWh / 1000.0;  // Wh → kWh
  readings.solar           = 2.1;
  readings.hydro           = 0.8;
  readings.diesel          = 0.0;
  readings.bufferTankCharge= 72.0;
  readings.flueTemp        = 185.0;
  readings.flowTemp        = 68.0;
  readings.returnTemp      = 45.0;
  readings.updatedAt       = millis();
}
