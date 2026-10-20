#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

// =====================================================
// CONFIGURATION
// =====================================================

#define LED_PIN   3
#define NUM_LEDS  24

String AP_SSID = "32-";
const char* AP_PASSWORD = "Strobe1234";

// =====================================================
// OBJECTS
// =====================================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
AsyncWebServer server(80);
Preferences preferences;

// =====================================================
// DEVICE ID
// =====================================================

String esp32ID;

// =====================================================
// WIFI SETTINGS
// =====================================================

String wifiSSID = "";
String wifiPassword = "";

bool rebootPending = false;
unsigned long rebootAt = 0;

// =====================================================
// LED SETTINGS
// =====================================================

uint8_t brightness = 100;
uint8_t redValue = 255;
uint8_t greenValue = 255;
uint8_t blueValue = 255;

uint32_t strobeInterval = 100;
uint8_t ledsPerGroup = 3;
bool strobeEnabled = true;

// =====================================================
// STROBE STATE
// =====================================================

unsigned long previousMillis = 0;
bool onPhase = true;

// =====================================================
// JSON STRING ESCAPING
// =====================================================

String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '"':  output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\b': output += "\\b";  break;
      case '\f': output += "\\f";  break;
      case '\n': output += "\\n";  break;
      case '\r': output += "\\r";  break;
      case '\t': output += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) {
          output += ' ';
        } else {
          output += c;
        }
        break;
    }
  }
  return output;
}

// =====================================================
// PARAMETER HELPER (Checks URL Query and POST Body)
// =====================================================

bool getParamValue(AsyncWebServerRequest *request, const String& name, String& outValue) {
  if (request->hasParam(name, false)) {
    outValue = request->getParam(name, false)->value();
    return true;
  }
  if (request->hasParam(name, true)) {
    outValue = request->getParam(name, true)->value();
    return true;
  }
  return false;
}

// =====================================================
// WEB PAGE
// =====================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 LED Controller</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #151922;
  color: #f1f5f9;
  max-width: 480px;
  margin: 25px auto;
  padding: 16px;
}
.card {
  background: #252c39;
  padding: 22px;
  border-radius: 14px;
}
h2 { margin-top: 0; }
h3 { margin-bottom: 12px; }
.section { margin-bottom: 28px; }
.status, .device-id {
  background: #171d27;
  padding: 12px;
  border-radius: 8px;
  margin-bottom: 18px;
  overflow-wrap: anywhere;
}
button {
  width: 100%;
  padding: 13px;
  font-size: 16px;
  border: 0;
  border-radius: 8px;
  color: white;
  background: #0284c7;
  cursor: pointer;
  margin-top: 10px;
}
button.off { background: #dc2626; }
button.toggle { background: #16a34a; margin-top: 0; }
button:disabled { opacity: 0.6; cursor: wait; }
input[type=range] {
  width: 100%;
  accent-color: #38bdf8;
}
input[type=number], input[type=text], input[type=password] {
  box-sizing: border-box;
  width: 100%;
  padding: 12px;
  font-size: 16px;
  border-radius: 8px;
  border: 1px solid #475569;
  background: #171d27;
  color: white;
  margin-top: 7px;
}
label { display: block; margin-top: 12px; }
.value { font-size: 18px; font-weight: bold; color: #7dd3fc; }
.hint { font-size: 13px; color: #aab4c5; line-height: 1.5; }
.color-label {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 12px;
}
.swatch {
  height: 35px;
  border-radius: 7px;
  margin: 10px 0;
  border: 1px solid #64748b;
}
.wifi-status {
  background: #171d27;
  border-radius: 8px;
  padding: 12px;
  margin-top: 12px;
  line-height: 1.6;
  overflow-wrap: anywhere;
}
.message {
  margin-top: 12px;
  font-size: 14px;
  line-height: 1.5;
  overflow-wrap: anywhere;
}
.wifi-section {
  background: #171d27;
  border: 1px solid #475569;
  border-radius: 10px;
  margin-bottom: 28px;
  overflow: hidden;
}
.wifi-section summary {
  list-style: none;
  cursor: pointer;
  padding: 16px;
  font-size: 17px;
  font-weight: bold;
  display: flex;
  align-items: center;
  justify-content: space-between;
  user-select: none;
}
.wifi-section summary::-webkit-details-marker { display: none; }
.wifi-section summary::after {
  content: "+";
  color: #7dd3fc;
  font-size: 24px;
  font-weight: normal;
  margin-left: 12px;
}
.wifi-section[open] summary::after { content: "\2212"; }
.wifi-section summary:hover { background: #252c39; }
.wifi-content { padding: 0 16px 16px 16px; }
.wifi-content h3 { margin-top: 4px; }
</style>
</head>
<body>
<div class="card">
  <h2>LED Strobe Controller</h2>

  <div class="device-id">
    ESP32 ID: <strong id="deviceId">Loading...</strong>
  </div>

  <div class="status">
    Strobe status: <strong id="status">Loading...</strong>
  </div>

  <button id="toggle" class="toggle" onclick="toggleStrobe()">Loading...</button>

  <details class="wifi-section">
    <summary>Wi-Fi Configuration</summary>
    <div class="wifi-content">
      <div class="wifi-status">
        <strong>Access Point</strong><br>
        Network: <span id="apSSID">Loading...</span><br>
        Address: <span id="apIP">Loading...</span>
      </div>
      <div class="wifi-status">
        <strong>Wi-Fi Station</strong><br>
        Status: <span id="staStatus">Loading...</span><br>
        Address: <span id="staIP">Not connected</span>
      </div>
      <form id="wifiForm" onsubmit="saveWiFi(event)">
        <label for="wifiSSID">Router Wi-Fi name (SSID)</label>
        <input type="text" id="wifiSSID" name="ssid" maxlength="32" autocomplete="off" required placeholder="Enter router Wi-Fi name">

        <label for="wifiPassword">Router Wi-Fi password</label>
        <input type="password" id="wifiPassword" name="password" maxlength="63" autocomplete="new-password" placeholder="Enter router password">

        <p class="hint">
          Enter credentials and save. The ESP32 will reboot.
          <br><br>
          Leave password empty to retain the current password for the same SSID, or for open networks.
        </p>

        <button type="submit" id="wifiSaveButton">Save Wi-Fi and Reboot</button>
        <div class="message" id="wifiMessage" role="status"></div>
      </form>
    </div>
  </details>

  <div class="section">
    <h3>Brightness</h3>
    <div class="color-label">
      <label for="brightness">Brightness</label>
      <span class="value" id="brightnessText">100</span>
    </div>
    <input id="brightness" type="range" min="0" max="255" step="1" value="100"
      oninput="previewValue('brightness',this.value)"
      onchange="setValue('brightness',this.value)">
    <p class="hint">Steps of 20, with 255 as maximum.</p>
  </div>

  <div class="section">
    <h3>LED Colour</h3>
    <div class="color-label">
      <label for="red">Red</label>
      <span class="value" id="redText">255</span>
    </div>
    <input id="red" type="range" min="0" max="255" step="1" value="255"
      oninput="previewValue('red',this.value)"
      onchange="setValue('red',this.value)">

    <div class="color-label">
      <label for="green">Green</label>
      <span class="value" id="greenText">255</span>
    </div>
    <input id="green" type="range" min="0" max="255" step="1" value="255"
      oninput="previewValue('green',this.value)"
      onchange="setValue('green',this.value)">

    <div class="color-label">
      <label for="blue">Blue</label>
      <span class="value" id="blueText">255</span>
    </div>
    <input id="blue" type="range" min="0" max="255" step="1" value="255"
      oninput="previewValue('blue',this.value)"
      onchange="setValue('blue',this.value)">

    <div class="swatch" id="swatch"></div>
    <p class="hint">RGB values use steps of 20, with 255 as maximum.</p>
  </div>

  <div class="section">
    <h3>LEDs Per Group</h3>
    <div class="color-label">
      <label for="group">Group size</label>
      <span class="value"><span id="groupText">3</span> LEDs</span>
    </div>
    <input id="group" type="range" min="1" max="24" step="1" value="3"
      oninput="previewValue('group',this.value)"
      onchange="setValue('group',this.value)">
    <p class="hint">Sets consecutive lit LEDs followed by the same count unlit.</p>
  </div>

  <div class="section">
    <h3>Strobe Interval</h3>
    <label for="interval">Milliseconds per phase</label>
    <p class="value"><span id="intervalText">100</span> ms</p>
    <input id="interval" type="number" min="0" max="60000" step="1" value="100">
    <button onclick="submitInterval()">Set Interval</button>
    <p class="hint">0 ms gives a continuous solid light.</p>
  </div>
</div>

<script>
const allowed = [];
for (let i = 0; i <= 240; i += 20) allowed.push(i);
allowed.push(255);

let settings = {
  brightness: 100, red: 255, green: 255, blue: 255,
  group: 3, interval: 100, enabled: true, id: 'Loading...'
};

function snap(value) {
  value = Number(value);
  return allowed.reduce((best, v) => Math.abs(v - value) < Math.abs(best - value) ? v : best);
}

function updateSwatch() {
  document.getElementById('swatch').style.background =
    `rgb(${settings.red},${settings.green},${settings.blue})`;
}

function previewValue(key, value) {
  let v = key === 'group' ? Number(value) : snap(value);
  document.getElementById(key + 'Text').textContent = v;
  if (['red', 'green', 'blue'].includes(key)) {
    const temp = {...settings, [key]: v};
    document.getElementById('swatch').style.background = `rgb(${temp.red},${temp.green},${temp.blue})`;
  }
}

async function sendSetting(key, value) {
  const response = await fetch('/api/settings?' + encodeURIComponent(key) + '=' + encodeURIComponent(value), {method: 'POST'});
  if (!response.ok) throw new Error('Request failed');
  await refreshStatus();
}

async function setValue(key, value) {
  if (key === 'group') {
    value = Math.max(1, Math.min(24, Number(value)));
  } else {
    value = snap(value);
  }
  document.getElementById(key).value = value;
  document.getElementById(key + 'Text').textContent = value;
  try {
    await sendSetting(key, value);
  } catch (e) {
    alert('Could not update setting.');
    await refreshStatus().catch(console.error);
  }
}

async function submitInterval() {
  const input = document.getElementById('interval');
  let value = Number(input.value);
  if (!Number.isFinite(value)) value = settings.interval;
  value = Math.round(Math.max(0, Math.min(60000, value)));
  input.value = value;
  try {
    await sendSetting('interval', value);
  } catch (e) {
    alert('Could not set interval.');
    await refreshStatus().catch(console.error);
  }
}

async function toggleStrobe() {
  try {
    await sendSetting('enabled', 'toggle');
  } catch (e) {
    alert('Could not change strobe state.');
  }
}

async function saveWiFi(event) {
  event.preventDefault();
  const ssid = document.getElementById('wifiSSID').value;
  const password = document.getElementById('wifiPassword').value;
  const button = document.getElementById('wifiSaveButton');
  const message = document.getElementById('wifiMessage');

  if (!ssid.trim()) {
    message.textContent = 'Please enter a Wi-Fi network name.';
    return;
  }

  button.disabled = true;
  message.textContent = 'Saving Wi-Fi credentials...';

  try {
    const body = new URLSearchParams();
    body.set('ssid', ssid);
    body.set('password', password);

    const response = await fetch('/api/wifi', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body.toString()
    });

    const result = await response.json();
    if (!response.ok || !result.ok) throw new Error(result.error || 'Could not save');
    message.textContent = 'Saved. ESP32 is rebooting. Reconnect in a few moments.';
  } catch (e) {
    message.textContent = 'Failed: ' + e.message;
    button.disabled = false;
  }
}

async function refreshStatus() {
  const response = await fetch('/api/status');
  if (!response.ok) throw new Error('Status failed');
  const s = await response.json();
  settings = s;

  document.getElementById('deviceId').textContent = s.id;
  for (const key of ['brightness', 'red', 'green', 'blue']) {
    document.getElementById(key).value = s[key];
    document.getElementById(key + 'Text').textContent = s[key];
  }
  document.getElementById('group').value = s.group;
  document.getElementById('groupText').textContent = s.group;
  document.getElementById('interval').value = s.interval;
  document.getElementById('intervalText').textContent = s.interval;

  document.getElementById('status').textContent = s.enabled ? 'ON' : 'OFF';
  const toggleBtn = document.getElementById('toggle');
  toggleBtn.textContent = s.enabled ? 'Turn Strobe OFF' : 'Turn Strobe ON';
  toggleBtn.className = s.enabled ? 'off' : 'toggle';

  document.getElementById('apSSID').textContent = s.apSSID;
  document.getElementById('apIP').textContent = s.apIP;
  document.getElementById('staStatus').textContent = s.staConnected ? ('Connected to ' + (s.staSSID || 'router')) : (s.staConfigured ? 'Connecting...' : 'Not configured');
  document.getElementById('staIP').textContent = s.staConnected ? s.staIP : 'Not connected';

  const ssidField = document.getElementById('wifiSSID');
  if (!ssidField.value) ssidField.value = s.staSSID || '';
  updateSwatch();
}

refreshStatus().catch(console.error);
setInterval(() => { refreshStatus().catch(() => {}); }, 5000);
</script>
</body>
</html>
)rawliteral";

// =====================================================
// LED CONTROL
// =====================================================

void updateLeds() {
  strip.clear();

  if (strobeEnabled) {
    for (int i = 0; i < NUM_LEDS; i++) {
      bool groupIsOn = ((i / ledsPerGroup) % 2 == 0);
      bool ledIsOn = (strobeInterval == 0) ? true : (groupIsOn == onPhase);

      if (ledIsOn) {
        strip.setPixelColor(i, strip.Color(redValue, greenValue, blueValue));
      }
    }
  }

  strip.show();
}

// =====================================================
// PERSISTENCE
// =====================================================

void saveSettings() {
  preferences.putUChar("brightness", brightness);
  preferences.putUChar("red", redValue);
  preferences.putUChar("green", greenValue);
  preferences.putUChar("blue", blueValue);
  preferences.putUInt("interval", strobeInterval);
  preferences.putUChar("group", ledsPerGroup);
  preferences.putBool("enabled", strobeEnabled);
}

// =====================================================
// JSON BUILDER
// =====================================================

String buildStatusJson() {
  bool staConnected = (WiFi.status() == WL_CONNECTED);

  String json = "{";
  json += "\"ok\":true,";
  json += "\"id\":\"" + jsonEscape(esp32ID) + "\",";
  json += "\"brightness\":" + String(brightness) + ",";
  json += "\"red\":" + String(redValue) + ",";
  json += "\"green\":" + String(greenValue) + ",";
  json += "\"blue\":" + String(blueValue) + ",";
  json += "\"group\":" + String(ledsPerGroup) + ",";
  json += "\"interval\":" + String(strobeInterval) + ",";
  json += "\"enabled\":" + String(strobeEnabled ? "true" : "false") + ",";
  json += "\"apSSID\":\"" + jsonEscape(AP_SSID) + "\",";
  json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"staConfigured\":" + String(wifiSSID.length() > 0 ? "true" : "false") + ",";
  json += "\"staConnected\":" + String(staConnected ? "true" : "false") + ",";
  json += "\"staSSID\":\"" + jsonEscape(staConnected ? WiFi.SSID() : wifiSSID) + "\",";
  json += "\"staIP\":\"" + (staConnected ? WiFi.localIP().toString() : "") + "\"";
  json += "}";

  return json;
}

// =====================================================
// STATUS API (GET)
// =====================================================

void handleStatus(AsyncWebServerRequest *request) {
  request->send(200, "application/json", buildStatusJson());
}

// =====================================================
// SINGLE VARIABLE QUERY API (GET /api/get?var=...)
// =====================================================

void handleGet(AsyncWebServerRequest *request) {
  String varName;
  if (!getParamValue(request, "var", varName) &&
      !getParamValue(request, "param", varName) &&
      !getParamValue(request, "name", varName) &&
      !getParamValue(request, "field", varName)) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"Provide a parameter name via ?var=...\"}");
    return;
  }

  varName.toLowerCase();
  varName.trim();

  String resultVal = "";
  bool found = true;
  bool isNumber = false;
  bool isBool = false;

  if (varName == "brightness") {
    resultVal = String(brightness);
    isNumber = true;
  } else if (varName == "red") {
    resultVal = String(redValue);
    isNumber = true;
  } else if (varName == "green") {
    resultVal = String(greenValue);
    isNumber = true;
  } else if (varName == "blue") {
    resultVal = String(blueValue);
    isNumber = true;
  } else if (varName == "group") {
    resultVal = String(ledsPerGroup);
    isNumber = true;
  } else if (varName == "interval") {
    resultVal = String(strobeInterval);
    isNumber = true;
  } else if (varName == "enabled" || varName == "strobe") {
    resultVal = strobeEnabled ? "true" : "false";
    isBool = true;
  } else if (varName == "id") {
    resultVal = esp32ID;
  } else if (varName == "apssid") {
    resultVal = AP_SSID;
  } else if (varName == "apip") {
    resultVal = WiFi.softAPIP().toString();
  } else if (varName == "staconfigured") {
    resultVal = (wifiSSID.length() > 0 ? "true" : "false");
    isBool = true;
  } else if (varName == "staconnected") {
    resultVal = (WiFi.status() == WL_CONNECTED ? "true" : "false");
    isBool = true;
  } else if (varName == "stassid") {
    resultVal = (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : wifiSSID);
  } else if (varName == "staip") {
    resultVal = (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "");
  } else {
    found = false;
  }

  if (!found) {
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"Unknown parameter\"}");
    return;
  }

  // If plain/raw is requested, return direct text
  if (request->hasParam("raw") || request->hasParam("plain")) {
    request->send(200, "text/plain", resultVal);
    return;
  }

  String json = "{\"ok\":true,\"" + varName + "\":";
  if (isNumber || isBool) {
    json += resultVal;
  } else {
    json += "\"" + jsonEscape(resultVal) + "\"";
  }
  json += "}";

  request->send(200, "application/json", json);
}

// =====================================================
// LED SETTINGS API (GET or POST)
// =====================================================

void handleSettings(AsyncWebServerRequest *request) {
  String val;
  bool changed = false;
  bool rawMode = request->hasParam("raw"); // Pass &raw=1 to skip 20-step snapping

  // Delegate to handleGet if a variable query is issued through /api/settings
  if (getParamValue(request, "var", val) || getParamValue(request, "get", val)) {
    handleGet(request);
    return;
  }

  if (getParamValue(request, "brightness", val)) {
    int v = constrain(val.toInt(), 0, 255);
    if (!rawMode) {
      v = (v == 255) ? 255 : (v / 20) * 20;
    }
    brightness = (uint8_t)v;
    strip.setBrightness(brightness);
    changed = true;
  }

  if (getParamValue(request, "red", val)) {
    int v = constrain(val.toInt(), 0, 255);
    if (!rawMode) {
      v = (v == 255) ? 255 : (v / 20) * 20;
    }
    redValue = (uint8_t)v;
    changed = true;
  }

  if (getParamValue(request, "green", val)) {
    int v = constrain(val.toInt(), 0, 255);
    if (!rawMode) {
      v = (v == 255) ? 255 : (v / 20) * 20;
    }
    greenValue = (uint8_t)v;
    changed = true;
  }

  if (getParamValue(request, "blue", val)) {
    int v = constrain(val.toInt(), 0, 255);
    if (!rawMode) {
      v = (v == 255) ? 255 : (v / 20) * 20;
    }
    blueValue = (uint8_t)v;
    changed = true;
  }

  if (getParamValue(request, "group", val)) {
    int v = constrain(val.toInt(), 1, NUM_LEDS);
    ledsPerGroup = (uint8_t)v;
    changed = true;
  }

  if (getParamValue(request, "interval", val)) {
    long v = constrain(val.toInt(), 0L, 60000L);
    strobeInterval = (uint32_t)v;
    previousMillis = millis();
    onPhase = true;
    changed = true;
  }

  if (getParamValue(request, "enabled", val) || getParamValue(request, "strobe", val)) {
    val.toLowerCase();
    if (val == "toggle") {
      strobeEnabled = !strobeEnabled;
    } else {
      strobeEnabled = (val == "1" || val == "true" || val == "on");
    }
    previousMillis = millis();
    onPhase = true;
    changed = true;
  }

  if (changed) {
    saveSettings();
    updateLeds();
  }

  // Returns full updated state as JSON
  request->send(200, "application/json", buildStatusJson());
}

// =====================================================
// WIFI CREDENTIALS & QUERY API (GET or POST)
// =====================================================

void handleWiFiSettings(AsyncWebServerRequest *request) {
  String newSSID;
  String newPassword = "";

  // If no SSID provided, return current Wi-Fi status
  if (!getParamValue(request, "ssid", newSSID)) {
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    String json = "{";
    json += "\"ok\":true,";
    json += "\"apSSID\":\"" + jsonEscape(AP_SSID) + "\",";
    json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"staConfigured\":" + String(wifiSSID.length() > 0 ? "true" : "false") + ",";
    json += "\"staConnected\":" + String(staConnected ? "true" : "false") + ",";
    json += "\"staSSID\":\"" + jsonEscape(staConnected ? WiFi.SSID() : wifiSSID) + "\",";
    json += "\"staIP\":\"" + (staConnected ? WiFi.localIP().toString() : "") + "\"";
    json += "}";
    request->send(200, "application/json", json);
    return;
  }

  getParamValue(request, "password", newPassword);
  newSSID.trim();

  if (newSSID.length() == 0 || newSSID.length() > 32) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"SSID must contain 1-32 characters\"}");
    return;
  }

  if (newPassword.length() > 63 || (newPassword.length() > 0 && newPassword.length() < 8)) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"Password must be empty or 8-63 characters\"}");
    return;
  }

  // Retain existing password if blank and same SSID
  if (newPassword.length() == 0 && newSSID == wifiSSID) {
    newPassword = wifiPassword;
  }

  preferences.putString("wifiSSID", newSSID);
  preferences.putString("wifiPassword", newPassword);

  wifiSSID = newSSID;
  wifiPassword = newPassword;

  Serial.println();
  Serial.println("New Wi-Fi credentials saved. Rebooting...");

  rebootPending = true;
  rebootAt = millis() + 1500;

  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Credentials saved. Rebooting to connect...\"}");
}

// =====================================================
// START WIFI
// =====================================================

void startWiFi() {
  WiFi.mode(WIFI_AP_STA);

  bool apStarted = WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD);
  if (apStarted) {
    Serial.println("\nWi-Fi access point started.");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  if (wifiSSID.length() > 0) {
    Serial.print("\nConnecting to router SSID: ");
    Serial.println(wifiSSID);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  char idBuffer[13];
  snprintf(idBuffer, sizeof(idBuffer), "%012llX", (unsigned long long)ESP.getEfuseMac());
  esp32ID = String(idBuffer);
  AP_SSID += esp32ID;

  Serial.println("\nESP32-C3 LED Controller");
  Serial.print("Device ID: ");
  Serial.println(esp32ID);

  strip.begin();
  strip.clear();
  strip.show();

  preferences.begin("strobe", false);

  brightness = preferences.getUChar("brightness", 100);
  redValue = preferences.getUChar("red", 255);
  greenValue = preferences.getUChar("green", 255);
  blueValue = preferences.getUChar("blue", 255);
  strobeInterval = preferences.getUInt("interval", 100);
  ledsPerGroup = preferences.getUChar("group", 3);
  strobeEnabled = preferences.getBool("enabled", true);

  wifiSSID = preferences.getString("wifiSSID", "");
  wifiPassword = preferences.getString("wifiPassword", "");

  brightness = constrain(brightness, 0, 255);
  redValue = constrain(redValue, 0, 255);
  greenValue = constrain(greenValue, 0, 255);
  blueValue = constrain(blueValue, 0, 255);
  strobeInterval = constrain(strobeInterval, 0U, 60000U);
  ledsPerGroup = constrain(ledsPerGroup, 1, NUM_LEDS);

  strip.setBrightness(brightness);

  previousMillis = millis();
  onPhase = true;
  updateLeds();

  startWiFi();

  // Route declarations supporting both GET and POST
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/get", HTTP_GET, handleGet);
  server.on("/api/settings", HTTP_GET | HTTP_POST, handleSettings);
  server.on("/api/wifi", HTTP_GET | HTTP_POST, handleWiFiSettings);

  server.begin();

  Serial.println("\nWeb server started.");
  Serial.print("Access URL: http://");
  Serial.println(WiFi.softAPIP());
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  if (rebootPending && (long)(millis() - rebootAt) >= 0) {
    Serial.println("Rebooting now...");
    delay(50);
    ESP.restart();
  }

  if (!strobeEnabled || strobeInterval == 0) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= strobeInterval) {
    previousMillis = currentMillis;
    onPhase = !onPhase;
    updateLeds();
  }
}
