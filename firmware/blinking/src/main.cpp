#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

constexpr const char* kWifiSsid = "YOUR_2_4GHZ_WIFI_SSID";
constexpr const char* kWifiPassword = "YOUR_WIFI_PASSWORD";
constexpr const char* kLocalApSsid = "LeakSense-ESP32";
constexpr const char* kLocalApPassword = "12345678";
constexpr const char* kFirebaseLatestUrl =
    "https://YOUR_PROJECT-default-rtdb.firebaseio.com/leaksense/latest.json";

constexpr uint8_t kLedPin = D9;
constexpr uint8_t kGreenLedPin = 17;
constexpr uint8_t kRedLedPin = 16;
constexpr uint8_t kOledSdaPin = 18;
constexpr uint8_t kOledSclPin = 23;
constexpr uint8_t kBmeSdaPin = 22;
constexpr uint8_t kBmeSclPin = 21;
constexpr uint8_t kRelayPin = 25;
constexpr int kMicsAnalogPin = 36;
constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr uint8_t kOledAddresses[] = {0x3C, 0x3D};
constexpr uint8_t kBmeAddresses[] = {0x76, 0x77};
constexpr uint8_t kMicsSamplesPerRead = 32;
constexpr unsigned long kBlinkIntervalMs = 500;
constexpr unsigned long kAlarmBlinkIntervalMs = 250;
constexpr unsigned long kSensorReadIntervalMs = 2000;
constexpr unsigned long kFirebaseUploadIntervalMs = 5000;
constexpr unsigned long kMicsBaselineDurationMs = 10000;
constexpr float kMicsGasDeltaThresholdV = 0.25f;
constexpr float kMicsGasRatioThreshold = 1.40f;
constexpr bool kRelayActiveLow = true;

TwoWire gOledWire = TwoWire(0);
TwoWire gBmeWire = TwoWire(1);
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &gOledWire, -1);
Adafruit_BME280 gBme;
WebServer gServer(80);

bool gDisplayReady = false;
bool gBmeReady = false;
bool gLocalServerReady = false;
uint8_t gDisplayAddress = 0;
uint8_t gBmeAddress = 0;
float gMicsBaselineVoltage = 0.0f;
bool gMicsBaselineReady = false;
bool gLedOn = false;
bool gAlarmLedOn = false;
bool gGasDetected = false;
unsigned long gStartMs = 0;
unsigned long gLastBlinkMs = 0;
unsigned long gLastAlarmBlinkMs = 0;
unsigned long gLastSensorReadMs = 0;
unsigned long gLastFirebaseUploadMs = 0;

float gLatestTemperatureC = 0.0f;
float gLatestHumidityPct = 0.0f;
float gLatestPressureHpa = 0.0f;
float gLatestMicsVoltage = 0.0f;
int gLatestMicsRaw = 0;
int gLatestPpmRaw = 0;
int gLatestPpmCompensated = 0;
const char* gLatestState = "calibrating";
bool gLatestAlarmOn = false;
unsigned long gLatestReadingMs = 0;

float jsonNumberOrZero(float value);

const char kLocalDashboardHtml[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LeakSense Local</title>
  <style>
    *{box-sizing:border-box} body{margin:0;font-family:Arial,sans-serif;background:#f3f4f6;color:#111827}
    main{max-width:820px;margin:0 auto;padding:20px} header{display:flex;justify-content:space-between;gap:12px;align-items:center;margin-bottom:16px}
    h1{font-size:24px;margin:0}.sub{color:#6b7280;margin-top:4px}.badge{border-radius:999px;padding:8px 14px;font-weight:700;text-transform:capitalize}
    .safe{background:#d1fae5;color:#065f46}.warning{background:#fef3c7;color:#92400e}.danger{background:#fee2e2;color:#991b1b}.calibrating{background:#e5e7eb;color:#374151}
    .grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}.card{background:white;border:1px solid #e5e7eb;border-radius:12px;padding:18px}
    .label{color:#6b7280;font-size:13px}.value{font-size:34px;font-weight:700;margin-top:6px}.wide{grid-column:1/-1}
    .row{display:flex;justify-content:space-between;padding:9px 0;border-bottom:1px solid #f3f4f6}.row:last-child{border-bottom:0}
    @media(max-width:640px){.grid{grid-template-columns:1fr}header{align-items:flex-start;flex-direction:column}}
  </style>
</head>
<body>
<main>
  <header>
    <div><h1>LeakSense Local</h1><div class="sub">Direct ESP32 dashboard</div></div>
    <div id="stateBadge" class="badge calibrating">calibrating</div>
  </header>
  <section class="grid">
    <div class="card"><div class="label">Gas level</div><div id="ppm" class="value">-- ppm</div></div>
    <div class="card"><div class="label">Temperature</div><div id="temp" class="value">-- C</div></div>
    <div class="card"><div class="label">Humidity</div><div id="hum" class="value">-- %</div></div>
    <div class="card"><div class="label">MiCS raw</div><div id="raw" class="value">--</div></div>
    <div class="card wide">
      <div class="row"><span>Fan / relay</span><strong id="fan">--</strong></div>
      <div class="row"><span>Buzzer / alarm</span><strong id="buzzer">--</strong></div>
      <div class="row"><span>Router WiFi</span><strong id="wifi">--</strong></div>
      <div class="row"><span>AP IP</span><strong id="apIp">--</strong></div>
      <div class="row"><span>Last update</span><strong id="updated">--</strong></div>
    </div>
  </section>
</main>
<script>
async function refresh(){
  try{
    const r=await fetch('/data',{cache:'no-store'});
    const d=await r.json();
    const state=d.state||'calibrating';
    document.getElementById('stateBadge').className='badge '+state;
    document.getElementById('stateBadge').textContent=state;
    document.getElementById('ppm').textContent=Math.round(d.ppm_compensated||0)+' ppm';
    document.getElementById('temp').textContent=Number(d.temperature||0).toFixed(1)+' C';
    document.getElementById('hum').textContent=Number(d.humidity||0).toFixed(1)+' %';
    document.getElementById('raw').textContent=Math.round(d.ppm_raw||0);
    document.getElementById('fan').textContent=d.fan?'ON':'off';
    document.getElementById('buzzer').textContent=d.buzzer?'ON':'off';
    document.getElementById('wifi').textContent=d.router_connected?d.router_ip:'offline';
    document.getElementById('apIp').textContent=d.ap_ip;
    document.getElementById('updated').textContent=new Date().toLocaleTimeString();
  }catch(e){document.getElementById('updated').textContent='connection lost'}
}
refresh(); setInterval(refresh,2000);
</script>
</body>
</html>
)rawliteral";

String boolJson(bool value) {
  return value ? "true" : "false";
}

String buildLatestReadingJson() {
  String payload = "{";
  payload += "\"ppm_compensated\":" + String(gLatestPpmCompensated) + ",";
  payload += "\"ppm_raw\":" + String(gLatestPpmRaw) + ",";
  payload += "\"mics_raw\":" + String(gLatestMicsRaw) + ",";
  payload += "\"mics_voltage\":" + String(gLatestMicsVoltage, 3) + ",";
  payload += "\"temperature\":" + String(jsonNumberOrZero(gLatestTemperatureC), 1) + ",";
  payload += "\"humidity\":" + String(jsonNumberOrZero(gLatestHumidityPct), 1) + ",";
  payload += "\"pressure\":" + String(jsonNumberOrZero(gLatestPressureHpa), 1) + ",";
  payload += "\"state\":\"" + String(gLatestState) + "\",";
  payload += "\"fan\":" + boolJson(gLatestAlarmOn) + ",";
  payload += "\"buzzer\":" + boolJson(gLatestAlarmOn) + ",";
  payload += "\"baseline_ready\":" + boolJson(gMicsBaselineReady) + ",";
  payload += "\"router_connected\":" + boolJson(WiFi.status() == WL_CONNECTED) + ",";
  payload += "\"router_ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  payload += "\"timestamp\":" + String(gLatestReadingMs);
  payload += "}";
  return payload;
}

void handleLocalDashboard() {
  gServer.send_P(200, "text/html", kLocalDashboardHtml);
}

void handleLocalData() {
  gServer.sendHeader("Access-Control-Allow-Origin", "*");
  gServer.send(200, "application/json", buildLatestReadingJson());
}

void handleNotFound() {
  gServer.send(404, "text/plain", "Not found. Try / or /data");
}

void startLocalServer() {
  if (gLocalServerReady) {
    return;
  }

  gServer.on("/", handleLocalDashboard);
  gServer.on("/data", handleLocalData);
  gServer.onNotFound(handleNotFound);
  gServer.begin();
  gLocalServerReady = true;
  Serial.println("Local dashboard started.");
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(kLocalApSsid, kLocalApPassword);
  Serial.print("Local AP SSID: ");
  Serial.println(kLocalApSsid);
  Serial.print("Local AP password: ");
  Serial.println(kLocalApPassword);
  Serial.print("Local AP IP: ");
  Serial.println(WiFi.softAPIP());
  startLocalServer();

  WiFi.begin(kWifiSsid, kWifiPassword);

  Serial.print("Connecting to router WiFi");
  const unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    gServer.handleClient();
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Router WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Router WiFi failed. Local AP dashboard is still available.");
  }
}

int estimateGasPpm(float micsVoltage) {
  if (!gMicsBaselineReady || gMicsBaselineVoltage <= 0.01f) {
    return 0;
  }

  const float ratio = micsVoltage / gMicsBaselineVoltage;
  return max(0, static_cast<int>((ratio - 1.0f) * 1000.0f));
}

const char* stateFromPpm(int ppm) {
  if (ppm >= 500) {
    return "danger";
  }
  if (ppm >= 300) {
    return "warning";
  }
  return "safe";
}

float jsonNumberOrZero(float value) {
  return isnan(value) ? 0.0f : value;
}

void uploadToFirebase(float temperatureC,
                      float humidityPct,
                      int ppmRaw,
                      int ppmCompensated,
                      const char* state,
                      bool alarmOn) {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, kFirebaseLatestUrl)) {
    Serial.println("Firebase HTTP begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"ppm_compensated\":" + String(ppmCompensated) + ",";
  payload += "\"ppm_raw\":" + String(ppmRaw) + ",";
  payload += "\"temperature\":" + String(jsonNumberOrZero(temperatureC), 1) + ",";
  payload += "\"humidity\":" + String(jsonNumberOrZero(humidityPct), 1) + ",";
  payload += "\"state\":\"" + String(state) + "\",";
  payload += "\"fan\":" + String(alarmOn ? "true" : "false") + ",";
  payload += "\"buzzer\":" + String(alarmOn ? "true" : "false") + ",";
  payload += "\"timestamp\":" + String(millis());
  payload += "}";

  const int code = http.PUT(payload);
  Serial.printf("Firebase PUT -> HTTP %d\r\n", code);
  if (code <= 0) {
    Serial.println(http.errorToString(code));
  }

  http.end();
}

void updateLatestReading(float temperatureC,
                         float humidityPct,
                         float pressureHpa,
                         int micsRaw,
                         float micsVoltage,
                         int ppmRaw,
                         int ppmCompensated,
                         const char* state,
                         bool alarmOn) {
  gLatestTemperatureC = temperatureC;
  gLatestHumidityPct = humidityPct;
  gLatestPressureHpa = pressureHpa;
  gLatestMicsRaw = micsRaw;
  gLatestMicsVoltage = micsVoltage;
  gLatestPpmRaw = ppmRaw;
  gLatestPpmCompensated = ppmCompensated;
  gLatestState = gMicsBaselineReady ? state : "calibrating";
  gLatestAlarmOn = alarmOn;
  gLatestReadingMs = millis();
}

void setRelayAlarm(bool enabled) {
  const uint8_t activeState = kRelayActiveLow ? LOW : HIGH;
  const uint8_t inactiveState = kRelayActiveLow ? HIGH : LOW;
  digitalWrite(kRelayPin, enabled ? activeState : inactiveState);
}

void updateStatusLeds(unsigned long now) {
  if (!gGasDetected) {
    gAlarmLedOn = false;
    digitalWrite(kGreenLedPin, HIGH);
    digitalWrite(kRedLedPin, LOW);
    return;
  }

  digitalWrite(kGreenLedPin, LOW);
  if (now - gLastAlarmBlinkMs >= kAlarmBlinkIntervalMs) {
    gLastAlarmBlinkMs = now;
    gAlarmLedOn = !gAlarmLedOn;
    digitalWrite(kRedLedPin, gAlarmLedOn ? HIGH : LOW);
  }
}

void scanI2CBus(TwoWire& bus, const char* label) {
  Serial.printf("Scanning %s I2C bus...\r\n", label);
  for (uint8_t address = 1; address < 127; ++address) {
    bus.beginTransmission(address);
    if (bus.endTransmission() == 0) {
      Serial.printf("%s bus device found at 0x%02X\r\n", label, address);
    }
  }
}

bool initDisplay() {
  for (uint8_t address : kOledAddresses) {
    if (display.begin(SSD1306_SWITCHCAPVCC, address)) {
      gDisplayAddress = address;
      return true;
    }
  }
  return false;
}

bool initBme() {
  for (uint8_t address : kBmeAddresses) {
    if (gBme.begin(address, &gBmeWire)) {
      gBmeAddress = address;
      return true;
    }
  }
  return false;
}

float readMicsVoltage() {
  uint32_t total = 0;
  for (uint8_t i = 0; i < kMicsSamplesPerRead; ++i) {
    total += analogRead(kMicsAnalogPin);
    delay(2);
  }

  const float raw = total / static_cast<float>(kMicsSamplesPerRead);
  return raw * 3.3f / 4095.0f;
}

void updateMicsBaseline(float voltage) {
  if (gMicsBaselineReady) {
    return;
  }

  if (gMicsBaselineVoltage <= 0.0f) {
    gMicsBaselineVoltage = voltage;
  } else {
    gMicsBaselineVoltage = (gMicsBaselineVoltage * 0.85f) + (voltage * 0.15f);
  }

  if (millis() - gStartMs >= kMicsBaselineDurationMs) {
    gMicsBaselineReady = true;
    Serial.printf("MiCS-5524 baseline ready: %.3f V\r\n", gMicsBaselineVoltage);
  }
}

bool isMicsGasDetected(float voltage) {
  if (!gMicsBaselineReady || gMicsBaselineVoltage <= 0.01f) {
    return false;
  }

  const float delta = voltage - gMicsBaselineVoltage;
  const float ratio = voltage / gMicsBaselineVoltage;
  return delta > kMicsGasDeltaThresholdV || ratio > kMicsGasRatioThreshold;
}

void drawStatusScreen(float temperatureC,
                      float humidityPct,
                      float pressureHpa,
                      int micsRaw,
                      float micsVoltage,
                      bool gasDetected) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("LeakSense Test");

  if (gBmeReady) {
    display.printf("T:%.1fC H:%.1f%%\r\n", temperatureC, humidityPct);
    display.printf("P: %.1f hPa\r\n", pressureHpa);
  } else {
    display.println("BME280 not found");
    display.printf("BME SDA:%u SCL:%u\r\n", kBmeSdaPin, kBmeSclPin);
  }

  display.printf("MiCS raw: %d\r\n", micsRaw);
  display.printf("MiCS V: %.3f\r\n", micsVoltage);

  if (gMicsBaselineReady) {
    const float ratio = gMicsBaselineVoltage > 0.01f ? micsVoltage / gMicsBaselineVoltage : 0.0f;
    display.printf("Base: %.3f R:%.2f\r\n", gMicsBaselineVoltage, ratio);
    display.printf("Gas: %s Alarm:%s\r\n", gasDetected ? "YES" : "no", gasDetected ? "ON" : "off");
  } else {
    const unsigned long elapsedMs = millis() - gStartMs;
    const unsigned long cappedElapsedMs =
        elapsedMs > kMicsBaselineDurationMs ? kMicsBaselineDurationMs : elapsedMs;
    const unsigned long remainingSec = (kMicsBaselineDurationMs - cappedElapsedMs) / 1000;
    display.println("Calibrating MiCS...");
    display.println("Reading baseline");
    display.printf("Time left: %lus\r\n", remainingSec);
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.print("ESP32 WiFi MAC address: ");
  Serial.println(WiFi.macAddress());
  connectWiFi();
  pinMode(kLedPin, OUTPUT);
  pinMode(kGreenLedPin, OUTPUT);
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kRelayPin, OUTPUT);
  updateStatusLeds(millis());
  setRelayAlarm(false);
  analogReadResolution(12);
  analogSetPinAttenuation(kMicsAnalogPin, ADC_11db);
  gStartMs = millis();
  delay(200);

  Serial.println("Relay self-test on GPIO25");
  setRelayAlarm(true);
  delay(500);
  setRelayAlarm(false);

  Serial.printf("OLED test starting (SDA=%u, SCL=%u)\r\n", kOledSdaPin, kOledSclPin);
  gOledWire.begin(kOledSdaPin, kOledSclPin);
  scanI2CBus(gOledWire, "OLED");

  gDisplayReady = initDisplay();
  if (gDisplayReady) {
    Serial.printf("OLED initialized at 0x%02X\r\n", gDisplayAddress);
  } else {
    Serial.println("OLED init failed. Check VCC/GND/SDA/SCL and address.");
  }

  Serial.printf("BME280 test starting (SDA=%u, SCL=%u)\r\n", kBmeSdaPin, kBmeSclPin);
  gBmeWire.begin(kBmeSdaPin, kBmeSclPin);
  scanI2CBus(gBmeWire, "BME280");

  gBmeReady = initBme();
  if (gBmeReady) {
    Serial.printf("BME280 initialized at 0x%02X\r\n", gBmeAddress);
  } else {
    Serial.println("BME280 init failed. Check VCC/GND/SDA/SCL and ADR/CS pins.");
  }

  Serial.printf("MiCS-5524 test starting (AO=GPIO%d)\r\n", kMicsAnalogPin);
  Serial.println("Keep MiCS-5524 in clean air for the first 10 seconds.");
}

void loop() {
  const unsigned long now = millis();
  gServer.handleClient();
  updateStatusLeds(now);

  if (now - gLastBlinkMs >= kBlinkIntervalMs) {
    gLastBlinkMs = now;
    gLedOn = !gLedOn;
    digitalWrite(kLedPin, gLedOn ? HIGH : LOW);
  }

  if (now - gLastSensorReadMs >= kSensorReadIntervalMs) {
    gLastSensorReadMs = now;

    float temperatureC = NAN;
    float humidityPct = NAN;
    float pressureHpa = NAN;
    const int micsRaw = analogRead(kMicsAnalogPin);
    const float micsVoltage = readMicsVoltage();
    updateMicsBaseline(micsVoltage);
    const int ppmCompensated = estimateGasPpm(micsVoltage);
    const int ppmRaw = static_cast<int>(micsVoltage * 1000.0f);
    const char* state = stateFromPpm(ppmCompensated);
    gGasDetected = isMicsGasDetected(micsVoltage) || strcmp(state, "safe") != 0;
    setRelayAlarm(gGasDetected);

    if (gBmeReady) {
      temperatureC = gBme.readTemperature();
      humidityPct = gBme.readHumidity();
      pressureHpa = gBme.readPressure() / 100.0f;

      Serial.printf("BME280 0x%02X -> T=%.2f C, H=%.2f %%, P=%.2f hPa\r\n",
                    gBmeAddress, temperatureC, humidityPct, pressureHpa);
    } else {
      Serial.println("BME280 not ready");
    }

    if (gMicsBaselineReady) {
      const float delta = micsVoltage - gMicsBaselineVoltage;
      const float ratio = gMicsBaselineVoltage > 0.01f ? micsVoltage / gMicsBaselineVoltage : 0.0f;
      Serial.printf("MiCS-5524 -> raw=%d, V=%.3f, baseline=%.3f, delta=%+.3f, ratio=%.2f, gas=%s\r\n",
                    micsRaw,
                    micsVoltage,
                    gMicsBaselineVoltage,
                    delta,
                    ratio,
                    gGasDetected ? "YES" : "no");
    } else {
      Serial.printf("MiCS-5524 baselining -> raw=%d, V=%.3f\r\n", micsRaw, micsVoltage);
    }

    updateLatestReading(temperatureC,
                        humidityPct,
                        pressureHpa,
                        micsRaw,
                        micsVoltage,
                        ppmRaw,
                        ppmCompensated,
                        state,
                        gGasDetected);

    if (gDisplayReady) {
      drawStatusScreen(temperatureC, humidityPct, pressureHpa, micsRaw, micsVoltage, gGasDetected);
    }

    if (now - gLastFirebaseUploadMs >= kFirebaseUploadIntervalMs) {
      gLastFirebaseUploadMs = now;
      uploadToFirebase(temperatureC, humidityPct, ppmRaw, ppmCompensated, state, gGasDetected);
    }
  }
}
