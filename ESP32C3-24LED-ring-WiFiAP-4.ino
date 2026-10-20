
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

// Factory-programmed MAC address: unique identifier per board.
String esp32ID;

// =====================================================
// SETTINGS
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
.status {
  background: #171d27;
  padding: 12px;
  border-radius: 8px;
  margin-bottom: 18px;
}
.device-id {
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
input[type=range] {
  width: 100%;
  accent-color: #38bdf8;
}
input[type=number] {
  box-sizing: border-box;
  width: 100%;
  padding: 12px;
  font-size: 17px;
  border-radius: 8px;
  border: 1px solid #475569;
  background: #171d27;
  color: white;
}
.value {
  font-size: 18px;
  font-weight: bold;
  color: #7dd3fc;
}
.hint {
  font-size: 13px;
  color: #aab4c5;
  line-height: 1.5;
}
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

  <button id="toggle" class="toggle"
    onclick="toggleStrobe()">Loading...</button>

  <div class="section">
    <h3>Brightness</h3>
    <div class="color-label">
      <label for="brightness">Brightness</label>
      <span class="value" id="brightnessText">100</span>
    </div>
    <input id="brightness" type="range"
      min="0" max="255" step="1" value="100"
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
    <input id="red" type="range" min="0" max="255" step="1"
      value="255"
      oninput="previewValue('red',this.value)"
      onchange="setValue('red',this.value)">

    <div class="color-label">
      <label for="green">Green</label>
      <span class="value" id="greenText">255</span>
    </div>
    <input id="green" type="range" min="0" max="255" step="1"
      value="255"
      oninput="previewValue('green',this.value)"
      onchange="setValue('green',this.value)">

    <div class="color-label">
      <label for="blue">Blue</label>
      <span class="value" id="blueText">255</span>
    </div>
    <input id="blue" type="range" min="0" max="255" step="1"
      value="255"
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

    <input id="group" type="range"
      min="1" max="24" step="1" value="3"
      oninput="previewValue('group',this.value)"
      onchange="setValue('group',this.value)">

    <p class="hint">
      Sets how many consecutive LEDs light together, followed
      by the same number of LEDs switched off.
    </p>
  </div>

  <div class="section">
    <h3>Strobe Interval</h3>

    <label for="interval">Milliseconds per phase</label>
    <p class="value"><span id="intervalText">100</span> ms</p>

    <input id="interval" type="number"
      min="0" max="60000" step="1" value="100">

    <button onclick="submitInterval()">Set Interval</button>

    <p class="hint">
      Set the interval in milliseconds per phase.
      Minimum 0 ms, maximum 60000 ms.
      Set to 0 ms for solid light with no strobing.
      Press Set Interval to apply and save your setting.
    </p>
  </div>
</div>

<script>
const allowed = [];
for (let i = 0; i <= 240; i += 20) allowed.push(i);
allowed.push(255);

let settings = {
  brightness: 100,
  red: 255,
  green: 255,
  blue: 255,
  group: 3,
  interval: 100,
  enabled: true,
  id: 'Loading...'
};

function snap(value) {
  value = Number(value);
  return allowed.reduce((best, v) =>
    Math.abs(v - value) < Math.abs(best - value) ? v : best
  );
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
    document.getElementById('swatch').style.background =
      `rgb(${temp.red},${temp.green},${temp.blue})`;
  }
}

async function sendSetting(key, value) {
  const response = await fetch(
    '/api/settings?' + encodeURIComponent(key) + '=' +
    encodeURIComponent(value),
    {method: 'POST'}
  );

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
    alert('Could not update setting. Check your connection.');
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
    alert('Could not set interval. Check your connection.');
    await refreshStatus().catch(console.error);
  }
}

async function toggleStrobe() {
  try {
    await sendSetting('enabled', settings.enabled ? 0 : 1);
  } catch (e) {
    alert('Could not change strobe state.');
  }
}

async function refreshStatus() {
  const response = await fetch('/api/status');
  if (!response.ok) throw new Error('Status request failed');

  const s = await response.json();
  settings = s;

  // Display this board's unique identifier.
  document.getElementById('deviceId').textContent = s.id;

  for (const key of ['brightness', 'red', 'green', 'blue']) {
    document.getElementById(key).value = s[key];
    document.getElementById(key + 'Text').textContent = s[key];
  }

  document.getElementById('group').value = s.group;
  document.getElementById('groupText').textContent = s.group;

  document.getElementById('interval').value = s.interval;
  document.getElementById('intervalText').textContent = s.interval;

  document.getElementById('status').textContent =
    s.enabled ? 'ON' : 'OFF';

  const button = document.getElementById('toggle');
  button.textContent = s.enabled ? 'Turn Strobe OFF' : 'Turn Strobe ON';
  button.className = s.enabled ? 'off' : 'toggle';

  updateSwatch();
}

refreshStatus().catch(console.error);
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

      bool ledIsOn = (strobeInterval == 0)
                       ? true
                       : (groupIsOn == onPhase);

      if (ledIsOn) {
        strip.setPixelColor(
          i,
          strip.Color(redValue, greenValue, blueValue)
        );
      }
    }
  }

  strip.show();
}

// =====================================================
// SAVE SETTINGS
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
// STATUS API
// =====================================================

void handleStatus(AsyncWebServerRequest *request) {
  String json = "{";

  json += "\"id\":\"" + esp32ID + "\",";
  json += "\"brightness\":" + String(brightness) + ",";
  json += "\"red\":" + String(redValue) + ",";
  json += "\"green\":" + String(greenValue) + ",";
  json += "\"blue\":" + String(blueValue) + ",";
  json += "\"group\":" + String(ledsPerGroup) + ",";
  json += "\"interval\":" + String(strobeInterval) + ",";
  json += "\"enabled\":";
  json += (strobeEnabled ? "true" : "false");
  json += "}";

  request->send(200, "application/json", json);
}

// =====================================================
// SETTINGS API
// =====================================================

void handleSettings(AsyncWebServerRequest *request) {
  bool changed = false;

  if (request->hasParam("brightness")) {
    int v = constrain(
      request->getParam("brightness")->value().toInt(), 0, 255
    );
    v = (v == 255) ? 255 : (v / 20) * 20;
    brightness = (uint8_t)v;
    strip.setBrightness(brightness);
    changed = true;
  }

  if (request->hasParam("red")) {
    int v = constrain(
      request->getParam("red")->value().toInt(), 0, 255
    );
    v = (v == 255) ? 255 : (v / 20) * 20;
    redValue = (uint8_t)v;
    changed = true;
  }

  if (request->hasParam("green")) {
    int v = constrain(
      request->getParam("green")->value().toInt(), 0, 255
    );
    v = (v == 255) ? 255 : (v / 20) * 20;
    greenValue = (uint8_t)v;
    changed = true;
  }

  if (request->hasParam("blue")) {
    int v = constrain(
      request->getParam("blue")->value().toInt(), 0, 255
    );
    v = (v == 255) ? 255 : (v / 20) * 20;
    greenValue = greenValue;
    blueValue = (uint8_t)v;
    changed = true;
  }

  if (request->hasParam("group")) {
    int v = request->getParam("group")->value().toInt();
    v = constrain(v, 1, NUM_LEDS);
    ledsPerGroup = (uint8_t)v;
    changed = true;
  }

  if (request->hasParam("interval")) {
    long v = request->getParam("interval")->value().toInt();
    v = constrain(v, 0L, 60000L);
    strobeInterval = (uint32_t)v;
    previousMillis = millis();
    onPhase = true;
    changed = true;
  }

  if (request->hasParam("enabled")) {
    String v = request->getParam("enabled")->value();
    strobeEnabled = (v == "1" || v == "true");
    previousMillis = millis();
    onPhase = true;
    changed = true;
  }

  if (changed) {
    saveSettings();
    updateLeds();
  }

  request->send(200, "application/json", "{\"ok\":true}");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Read the factory-programmed MAC address.
  // ESP.getEfuseMac() is available before Wi-Fi starts.
  char idBuffer[13];
  snprintf(idBuffer, sizeof(idBuffer), "%012llX",
           (unsigned long long)ESP.getEfuseMac());
  esp32ID = String(idBuffer);
  AP_SSID = AP_SSID + esp32ID;
  
  Serial.print("ESP32 ID: ");
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

  WiFi.mode(WIFI_AP);

  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Wi-Fi AP started.");
    Serial.print("Network: ");
    Serial.println(AP_SSID);
    Serial.print("Open this address: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start Wi-Fi AP!");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/settings", HTTP_POST, handleSettings);

  server.begin();
  Serial.println("Web server started.");
}

// =====================================================
// NON-BLOCKING STROBE
// =====================================================

void loop() {
  if (!strobeEnabled) {
    return;
  }

  if (strobeInterval == 0) {
    return;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= strobeInterval) {
    previousMillis = currentMillis;
    onPhase = !onPhase;
    updateLeds();
  }
}
