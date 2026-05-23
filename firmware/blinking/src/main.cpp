#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include "secrets.h"
 
constexpr uint8_t kLedPin = D9;
constexpr uint8_t kGreenLedPin = 17;
constexpr uint8_t kRedLedPin = 4;
constexpr uint8_t kYellowLedPin = 16;
constexpr uint8_t kAlarmCancelButtonPin = 14;
constexpr uint8_t kOledSdaPin = 18;
constexpr uint8_t kOledSclPin = 23;
constexpr uint8_t kBmeSdaPin = 22;
constexpr uint8_t kBmeSclPin = 21;
constexpr uint8_t kAlarmOutputPin = 25;
constexpr uint8_t kFanRelayPin = 26;
constexpr int kSen0565AnalogPin = 39;
constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr uint8_t kOledAddresses[] = {0x3C, 0x3D};
constexpr uint8_t kBmeAddresses[] = {0x76, 0x77};
constexpr uint8_t kGasSamplesPerRead = 32;
constexpr unsigned long kBlinkIntervalMs = 500;
constexpr unsigned long kRedBlinkIntervalMs = 75;
constexpr unsigned long kSensorReadIntervalMs = 2000;
constexpr unsigned long kFirebaseHistoryIntervalMs = 300000;
constexpr unsigned long kWifiConnectTimeoutMs = 15000;
constexpr unsigned long kWifiRetryIntervalMs = 30000;
constexpr unsigned long kGasBaselineDurationMs = 10000;
constexpr unsigned long kLedSelfTestIntervalMs = 500;
constexpr unsigned long kDangerAlarmDurationMs = 10000;
constexpr unsigned long kDetectionPauseDurationMs = 10000;
constexpr float kSen0565WarningVoltageThresholdV = 1.60f;
constexpr float kSen0565DangerVoltageThresholdV = 1.90f;
constexpr float kSen0565ExtremeVoltageThresholdV = 2.20f;
constexpr float kThermalRiskTempRiseC = 5.0f;
constexpr float kThermalRiskAbsoluteTempC = 55.0f;
constexpr uint8_t kGasLevelConfirmReads = 5;
constexpr bool kAlarmOutputActiveLow = true;
constexpr bool kFanRelayActiveLow = true;
constexpr float kGasBaselineFollowAlpha = 0.02f;
constexpr float kGasBaselineSettleDeltaV = 0.05f;
constexpr float kGasBaselineSettleRatio = 1.03f;
 
enum class GasLevel {
  Safe,
  Warning,
  Danger,
};
 
struct GasSensorState {
  const char* name;
  int analogPin;
  float warningVoltageThresholdV;
  float dangerVoltageThresholdV;
  float extremeVoltageThresholdV;
  float baselineVoltage;
  bool baselineReady;
  int raw;
  float voltage;
  GasLevel level;
  bool extreme;
  bool detected;
};
 
TwoWire gOledWire = TwoWire(0);
TwoWire gBmeWire = TwoWire(1);
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &gOledWire, -1);
Adafruit_BME280 gBme;
 
bool gDisplayReady = false;
bool gBmeReady = false;
uint8_t gDisplayAddress = 0;
uint8_t gBmeAddress = 0;
 
bool gWiFiConnected = false;
bool gWiFiConnecting = false;
unsigned long gWiFiConnectStartMs = 0;
unsigned long gLastWiFiAttemptMs = 0;
 
bool gLedOn = false;
 
bool gGasDetected = false;
bool gDangerAlarmActive = false;
bool gDetectionPaused = false;
bool gRedBlinkOn = false;
bool gAlarmCancelButtonLatched = false;
GasLevel gGasLevel = GasLevel::Safe;
GasLevel gLastGasLevel = GasLevel::Safe;
GasLevel gPendingGasLevel = GasLevel::Safe;
uint8_t gPendingGasLevelCount = 0;
unsigned long gStartMs = 0;
unsigned long gLastBlinkMs = 0;
unsigned long gLastRedBlinkMs = 0;
unsigned long gLastSensorReadMs = 0;
unsigned long gLastFirebaseHistoryMs = 0;
unsigned long gDangerAlarmStartMs = 0;
unsigned long gDetectionPauseStartMs = 0;
float gLastTemperatureC = NAN;
GasSensorState gSen0565Sensor = {"SEN0565",
                                 kSen0565AnalogPin,
                                 kSen0565WarningVoltageThresholdV,
                                 kSen0565DangerVoltageThresholdV,
                                 kSen0565ExtremeVoltageThresholdV,
                                 0.0f,
                                 false,
                                 0,
                                 0.0f,
                                 GasLevel::Safe,
                                 false,
                                 false};
 
const char* gasLevelName(GasLevel level) {
  switch (level) {
    case GasLevel::Danger:
      return "DANGER";
    case GasLevel::Warning:
      return "WARNING";
    case GasLevel::Safe:
    default:
      return "SAFE";
  }
}
 
void setAlarmOutput(bool enabled) {
  const uint8_t activeState = kAlarmOutputActiveLow ? LOW : HIGH;
  const uint8_t inactiveState = kAlarmOutputActiveLow ? HIGH : LOW;
  digitalWrite(kAlarmOutputPin, enabled ? activeState : inactiveState);
}
 
void setFanRelay(bool enabled) {
  const uint8_t activeState = kFanRelayActiveLow ? LOW : HIGH;
  const uint8_t inactiveState = kFanRelayActiveLow ? HIGH : LOW;
  digitalWrite(kFanRelayPin, enabled ? activeState : inactiveState);
}
 
void setStatusLeds(bool greenOn, bool redOn, bool yellowOn);
void handleAlarmCancelButton(unsigned long now);
void updateDangerAlarm(unsigned long now);
void updateStatusLeds(unsigned long now);
void serviceAlarmControls(unsigned long now);
void startWiFiConnection(unsigned long now);
void serviceWiFi(unsigned long now);
 
void startDangerAlarm(unsigned long now) {
  gDangerAlarmActive = true;
  gDangerAlarmStartMs = now;
  gRedBlinkOn = true;
  gLastRedBlinkMs = now;
  setAlarmOutput(true);
  setFanRelay(true);
}
 
void stopDangerAlarm() {
  gDangerAlarmActive = false;
  gGasDetected = false;
  gGasLevel = GasLevel::Safe;
  gPendingGasLevel = GasLevel::Safe;
  gPendingGasLevelCount = 0;
  gRedBlinkOn = false;
  setAlarmOutput(false);
  setFanRelay(false);
  setStatusLeds(true, false, false);
}
 
void startDetectionPause(unsigned long now) {
  stopDangerAlarm();
  gDetectionPaused = true;
  gDetectionPauseStartMs = now;
}
 
void updateDetectionPause(unsigned long now) {
  if (!gDetectionPaused) {
    return;
  }
 
  if (now - gDetectionPauseStartMs >= kDetectionPauseDurationMs) {
    gDetectionPaused = false;
    gLastSensorReadMs = 0;
    Serial.println("Detection pause ended; monitoring resumed");
    return;
  }
}
 
void updateDangerAlarm(unsigned long now) {
  if (!gDangerAlarmActive) {
    setAlarmOutput(false);
    setFanRelay(false);
    return;
  }
 
  if (now - gDangerAlarmStartMs >= kDangerAlarmDurationMs) {
    stopDangerAlarm();
    return;
  }
 
  setAlarmOutput(true);
  setFanRelay(true);
}
 
void setStatusLeds(bool greenOn, bool redOn, bool yellowOn) {
  digitalWrite(kGreenLedPin, greenOn ? HIGH : LOW);
  digitalWrite(kRedLedPin, redOn ? HIGH : LOW);
  digitalWrite(kYellowLedPin, yellowOn ? HIGH : LOW);
}
 
void runLedSelfTest() {
  setStatusLeds(false, false, false);
  delay(100);
  Serial.println("LED self-test: green -> yellow -> red");
 
  setStatusLeds(true, false, false);
  delay(kLedSelfTestIntervalMs);
  setStatusLeds(false, false, true);
  delay(kLedSelfTestIntervalMs);
  setStatusLeds(false, true, false);
  delay(kLedSelfTestIntervalMs);
  setStatusLeds(false, false, false);
}
 
void startWiFiConnection(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED || gWiFiConnecting) {
    return;
  }

  Serial.printf("Starting WiFi connection to SSID='%s'...\r\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  gWiFiConnecting = true;
  gWiFiConnectStartMs = now;
  gLastWiFiAttemptMs = now;
}

void serviceWiFi(unsigned long now) {
  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (!gWiFiConnected) {
      Serial.printf("WiFi connected, IP=%s\r\n", WiFi.localIP().toString().c_str());
    }
    gWiFiConnected = true;
    gWiFiConnecting = false;
    return;
  }

  if (gWiFiConnected) {
    Serial.println("WiFi disconnected. Local monitoring continues; Firebase upload paused.");
  }
  gWiFiConnected = false;

  if (gWiFiConnecting) {
    if (now - gWiFiConnectStartMs >= kWifiConnectTimeoutMs) {
      Serial.println("WiFi connection timed out. Local monitoring continues; will retry later.");
      WiFi.disconnect(false);
      gWiFiConnecting = false;
    }
    return;
  }

  if (gLastWiFiAttemptMs == 0 || now - gLastWiFiAttemptMs >= kWifiRetryIntervalMs) {
    startWiFiConnection(now);
  }
}
 
float gasRatio(const GasSensorState& sensor);
float gasNormalizedRatio(const GasSensorState& sensor);
float gasDeltaMagnitude(const GasSensorState& sensor);

float estimatePpm(const GasSensorState& sensor) {
  if (!sensor.baselineReady || sensor.baselineVoltage <= 0.0f) {
    return 0.0f;
  }

  const float ratio = gasNormalizedRatio(sensor);
  if (ratio <= 1.0f) {
    return 0.0f;
  }
  if (ratio <= 2.2f) {
    return (ratio - 1.0f) * 250.0f;
  }
  return 300.0f + (ratio - 2.2f) * 500.0f;
}
 
String jsonFloat(float value, unsigned int decimalPlaces) {
  if (isnan(value) || isinf(value)) {
    return "null";
  }
  return String(value, decimalPlaces);
}
 
String firebasePayload(float ppm,
                       float rawPpm,
                       float gasVoltage,
                       float temperatureC,
                       float humidityPct,
                       const char* state,
                       bool fan,
                       bool buzzer,
                       bool thermalRisk) {
  String payload = "{";
  payload += "\"ppm_compensated\":" + jsonFloat(ppm, 1) + ",";
  payload += "\"ppm_raw\":" + jsonFloat(rawPpm, 1) + ",";
  payload += "\"voltage\":" + jsonFloat(gasVoltage, 3) + ",";
  payload += "\"temperature\":" + jsonFloat(temperatureC, 1) + ",";
  payload += "\"humidity\":" + jsonFloat(humidityPct, 1) + ",";
  payload += "\"state\":\"" + String(state) + "\",";
  payload += "\"fan\":" + String(fan ? "true" : "false") + ",";
  payload += "\"buzzer\":" + String(buzzer ? "true" : "false") + ",";
  payload += "\"thermal_risk\":" + String(thermalRisk ? "true" : "false") + ",";
  payload += "\"timestamp\":{\".sv\":\"timestamp\"}";
  payload += "}";
  return payload;
}
 
bool uploadReadingToFirebase(const char* node,
                             bool append,
                             float ppmCompensated,
                             float ppmRaw,
                             float gasVoltage,
                             float temperatureC,
                             float humidityPct,
                             const char* state,
                             bool fan,
                             bool buzzer,
                             bool thermalRisk) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
 
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String("https://") + FIREBASE_HOST + "/leaksense/" + node + ".json?auth=" + FIREBASE_AUTH;
 
  if (!http.begin(client, url)) {
    Serial.println("Firebase: begin failed");
    return false;
  }
 
  http.addHeader("Content-Type", "application/json");
  const String payload = firebasePayload(ppmCompensated,
                                         ppmRaw,
                                         gasVoltage,
                                         temperatureC,
                                         humidityPct,
                                         state,
                                         fan,
                                         buzzer,
                                         thermalRisk);
  const int httpCode = append ? http.POST(payload) : http.PUT(payload);
 
  if (httpCode <= 0) {
    Serial.printf("Firebase: request failed (%d)\r\n", httpCode);
    http.end();
    return false;
  }
 
  if (httpCode >= 200 && httpCode < 300) {
    Serial.printf("Firebase: uploaded %s reading, code=%d\r\n", node, httpCode);
    http.end();
    return true;
  }
 
  Serial.printf("Firebase: upload error %d -> %s\r\n", httpCode, http.errorToString(httpCode).c_str());
  http.end();
  return false;
}
 
bool uploadLatestToFirebase(float ppmCompensated,
                            float ppmRaw,
                            float gasVoltage,
                            float temperatureC,
                            float humidityPct,
                            const char* state,
                            bool fan,
                            bool buzzer,
                            bool thermalRisk) {
  return uploadReadingToFirebase("latest",
                                 false,
                                 ppmCompensated,
                                 ppmRaw,
                                 gasVoltage,
                                 temperatureC,
                                 humidityPct,
                                 state,
                                 fan,
                                 buzzer,
                                 thermalRisk);
}
 
bool uploadHistoryToFirebase(float ppmCompensated,
                             float ppmRaw,
                             float gasVoltage,
                             float temperatureC,
                             float humidityPct,
                             const char* state,
                             bool fan,
                             bool buzzer,
                             bool thermalRisk) {
  return uploadReadingToFirebase("history",
                                 true,
                                 ppmCompensated,
                                 ppmRaw,
                                 gasVoltage,
                                 temperatureC,
                                 humidityPct,
                                 state,
                                 fan,
                                 buzzer,
                                 thermalRisk);
}
 
void updateStatusLeds(unsigned long now) {
  if (gDetectionPaused) {
    updateDetectionPause(now);
    return;
  }
 
  if (gDangerAlarmActive) {
    if (now - gLastRedBlinkMs >= kRedBlinkIntervalMs) {
      gLastRedBlinkMs = now;
      gRedBlinkOn = !gRedBlinkOn;
    }
    setStatusLeds(false, gRedBlinkOn, false);
    return;
  }
 
  switch (gGasLevel) {
    case GasLevel::Danger:
      setStatusLeds(false, true, false);
      break;
    case GasLevel::Warning:
      setStatusLeds(false, false, true);
      break;
    case GasLevel::Safe:
    default:
      setStatusLeds(true, false, false);
      break;
  }
}

void serviceAlarmControls(unsigned long now) {
  handleAlarmCancelButton(now);
  updateDangerAlarm(now);
  updateStatusLeds(now);
}
 
void handleAlarmCancelButton(unsigned long now) {
  const bool pressed = digitalRead(kAlarmCancelButtonPin) == LOW;
  if (!pressed) {
    gAlarmCancelButtonLatched = false;
    return;
  }

  if (gAlarmCancelButtonLatched) {
    return;
  }

  gAlarmCancelButtonLatched = true;
  Serial.println("GPIO14 button pressed: alarm stopped; gas detection paused for 10 seconds");
  startDetectionPause(now);
}
 
GasLevel confirmGasLevel(GasLevel newLevel, bool extremeDanger) {
  if (newLevel == GasLevel::Safe) {
    gPendingGasLevel = GasLevel::Safe;
    gPendingGasLevelCount = 0;
    return GasLevel::Safe;
  }
 
  if (newLevel == GasLevel::Warning) {
    gPendingGasLevel = GasLevel::Warning;
    gPendingGasLevelCount = 0;
    return GasLevel::Warning;
  }
 
  if (extremeDanger) {
    gPendingGasLevel = GasLevel::Danger;
    gPendingGasLevelCount = 0;
    return GasLevel::Danger;
  }
 
  if (gPendingGasLevel != GasLevel::Danger) {
    gPendingGasLevel = GasLevel::Danger;
    gPendingGasLevelCount = 1;
  } else if (gPendingGasLevelCount < kGasLevelConfirmReads) {
    ++gPendingGasLevelCount;
  }
  if (gPendingGasLevelCount >= kGasLevelConfirmReads) {
    gPendingGasLevelCount = 0;
    return GasLevel::Danger;
  }
 
  return GasLevel::Warning;
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
 
float readAnalogVoltage(int analogPin) {
  uint32_t total = 0;
  for (uint8_t i = 0; i < kGasSamplesPerRead; ++i) {
    total += analogRead(analogPin);
    serviceAlarmControls(millis());
    delay(2);
  }
 
  const float raw = total / static_cast<float>(kGasSamplesPerRead);
  return raw * 3.3f / 4095.0f;
}

void updateGasBaseline(GasSensorState& sensor) {
  if (sensor.baselineVoltage <= 0.0f) {
    sensor.baselineVoltage = sensor.voltage;
    return;
  }

  if (!sensor.baselineReady) {
    sensor.baselineVoltage = (sensor.baselineVoltage * 0.85f) + (sensor.voltage * 0.15f);
    if (millis() - gStartMs >= kGasBaselineDurationMs) {
      sensor.baselineReady = true;
      Serial.printf("%s baseline ready: %.3f V\r\n", sensor.name, sensor.baselineVoltage);
    }
    return;
  }

  if (gasDeltaMagnitude(sensor) < kGasBaselineSettleDeltaV &&
      gasNormalizedRatio(sensor) < kGasBaselineSettleRatio) {
    sensor.baselineVoltage =
        (sensor.baselineVoltage * (1.0f - kGasBaselineFollowAlpha)) +
        (sensor.voltage * kGasBaselineFollowAlpha);
  }
}

float gasRatio(const GasSensorState& sensor) {
  return sensor.baselineVoltage > 0.01f ? sensor.voltage / sensor.baselineVoltage : 0.0f;
}

float gasNormalizedRatio(const GasSensorState& sensor) {
  const float ratio = gasRatio(sensor);
  if (ratio <= 0.0f) {
    return 1.0f;
  }
  return ratio >= 1.0f ? ratio : (1.0f / ratio);
}

float gasDeltaMagnitude(const GasSensorState& sensor) {
  return fabsf(sensor.voltage - sensor.baselineVoltage);
}

GasLevel evaluateGasLevel(const GasSensorState& sensor) {
  if (sensor.voltage >= sensor.extremeVoltageThresholdV) {
    return GasLevel::Danger;
  }
  if (sensor.voltage >= sensor.dangerVoltageThresholdV) {
    return GasLevel::Danger;
  }
  if (sensor.voltage >= sensor.warningVoltageThresholdV) {
    return GasLevel::Warning;
  }
  return GasLevel::Safe;
}

bool hasThermalRisk(float temperatureC) {
  if (!isfinite(temperatureC)) {
    return false;
  }

  const bool highTemperature = temperatureC >= kThermalRiskAbsoluteTempC;
  const bool rapidRise = isfinite(gLastTemperatureC) &&
                         (temperatureC - gLastTemperatureC) >= kThermalRiskTempRiseC;

  return highTemperature || rapidRise;
}
 
void sampleGasSensor(GasSensorState& sensor) {
  sensor.raw = analogRead(sensor.analogPin);
  sensor.voltage = readAnalogVoltage(sensor.analogPin);
  updateGasBaseline(sensor);
  sensor.extreme = sensor.baselineReady &&
                   sensor.voltage >= sensor.extremeVoltageThresholdV;
  sensor.level = evaluateGasLevel(sensor);
  sensor.detected = sensor.level == GasLevel::Danger;
}

void printGasSensor(const GasSensorState& sensor) {
  if (sensor.baselineReady) {
    const float signedDelta = sensor.voltage - sensor.baselineVoltage;
    Serial.printf("%s -> raw=%d, V=%.3f, baseline=%.3f, delta=%+.3f, absDelta=%.3f, ratio=%.2f, state=%s, extreme=%s\r\n",
                  sensor.name,
                  sensor.raw,
                  sensor.voltage,
                  sensor.baselineVoltage,
                  signedDelta,
                  gasDeltaMagnitude(sensor),
                  gasNormalizedRatio(sensor),
                  gasLevelName(sensor.level),
                  sensor.extreme ? "YES" : "no");
  } else {
    Serial.printf("%s baselining -> raw=%d, V=%.3f\r\n",
                  sensor.name,
                  sensor.raw,
                  sensor.voltage);
  }
}
 
void drawStatusScreen(float temperatureC,
                      float humidityPct,
                      float pressureHpa,
                      const GasSensorState& sensor,
                      GasLevel confirmedLevel,
                      bool gasDetected,
                      bool fanOn) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("LeakSense SEN0565");
 
  if (gBmeReady) {
    display.printf("T:%.1fC H:%.0f%% P:%.0f\r\n", temperatureC, humidityPct, pressureHpa);
 
  } else {
    display.println("BME280 not found");
 
  }
 
  display.printf("Raw:%4d V:%.2f\r\n", sensor.raw, sensor.voltage);
  if (sensor.baselineReady) {
    display.printf("Base:%.2f R:%.2f\r\n", sensor.baselineVoltage, gasNormalizedRatio(sensor));
    display.printf("State:%s\r\n", gasLevelName(confirmedLevel));
    display.printf("Alarm:%s Fan:%s\r\n", gasDetected ? "ON" : "off", fanOn ? "ON" : "off");
 
 
  } else {
    const unsigned long elapsedMs = millis() - gStartMs;
    const unsigned long cappedElapsedMs =
        elapsedMs > kGasBaselineDurationMs ? kGasBaselineDurationMs : elapsedMs;
    const unsigned long remainingSec = (kGasBaselineDurationMs - cappedElapsedMs) / 1000;
    display.println("Baselining SEN0565");
 
    display.printf("Time left: %lus\r\n", remainingSec);
  }
 
  display.display();
}
 
void setup() {
  Serial.begin(115200);
  pinMode(kLedPin, OUTPUT);
  pinMode(kGreenLedPin, OUTPUT);
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kYellowLedPin, OUTPUT);
  pinMode(kAlarmOutputPin, OUTPUT);
  pinMode(kFanRelayPin, OUTPUT);
  pinMode(kAlarmCancelButtonPin, INPUT_PULLUP);
  gAlarmCancelButtonLatched = false;
  setAlarmOutput(false);
  setFanRelay(false);
  runLedSelfTest();
  updateStatusLeds(millis());
 
  analogReadResolution(12);
  analogSetPinAttenuation(kSen0565AnalogPin, ADC_11db);
  gStartMs = millis();
  delay(200);
 
 
 
 
 
 
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
 
  Serial.printf("%s test starting (A=GPIO%d)\r\n", gSen0565Sensor.name, gSen0565Sensor.analogPin);
  Serial.println("SEN0565 alarm contribution: enabled");
  Serial.println("Keep SEN0565 in clean air for the first 10 seconds.");
  Serial.println("Warm SEN0565 for at least 5 minutes before trusting its thresholds.");
 
  startWiFiConnection(millis());
  Serial.println("WiFi will connect in background. Local monitoring is available offline.");
}
 
void loop() {
  const unsigned long now = millis();
  serviceAlarmControls(now);
  serviceWiFi(now);
 
  if (now - gLastBlinkMs >= kBlinkIntervalMs) {
    gLastBlinkMs = now;
    gLedOn = !gLedOn;
    digitalWrite(kLedPin, gLedOn ? HIGH : LOW);
  }
 
  if (!gDetectionPaused && now - gLastSensorReadMs >= kSensorReadIntervalMs) {
    gLastSensorReadMs = now;
 
    float temperatureC = NAN;
    float humidityPct = NAN;
    float pressureHpa = NAN;
    sampleGasSensor(gSen0565Sensor);
    if (gDetectionPaused) {
      return;
    }
    if (gBmeReady) {
      temperatureC = gBme.readTemperature();
      humidityPct = gBme.readHumidity();
      pressureHpa = gBme.readPressure() / 100.0f;
 
      Serial.printf("BME280 0x%02X -> T=%.2f C, H=%.2f %%, P=%.2f hPa\r\n",
                    gBmeAddress, temperatureC, humidityPct, pressureHpa);
    } else {
      Serial.println("BME280 not ready");
    }

    gLastGasLevel = gGasLevel;
    gGasLevel = confirmGasLevel(gSen0565Sensor.level, gSen0565Sensor.extreme);
    const bool thermalRisk = hasThermalRisk(temperatureC);
    if (thermalRisk) {
      gGasLevel = GasLevel::Danger;
      Serial.println("Thermal risk escalation: abnormal temperature rise/high temperature");
    }
    gGasDetected = gGasLevel == GasLevel::Danger;
    if (gGasDetected && gLastGasLevel != GasLevel::Danger) {
      startDangerAlarm(now);
    }
    updateDangerAlarm(now);
    updateStatusLeds(now);
    if (isfinite(temperatureC)) {
      gLastTemperatureC = temperatureC;
    }
 
    printGasSensor(gSen0565Sensor);
    Serial.printf("Alarm source -> SEN0565 raw=%s, confirmed=%s, alarm=%s, fan=%s\r\n",
                  gasLevelName(gSen0565Sensor.level),
                  gasLevelName(gGasLevel),
                  gDangerAlarmActive ? "ON" : "off",
                  gDangerAlarmActive ? "ON" : "off");
 
 
 
 
 
 
 
 
    const float ppmCompensated = estimatePpm(gSen0565Sensor);
    const float ppmRaw = gSen0565Sensor.raw * 0.25f;
    const char* stateName = gasLevelName(gGasLevel);
    const bool fanOn = gDangerAlarmActive;
    const bool buzzerOn = gDangerAlarmActive;
    const bool shouldUploadAlertHistory =
        gGasLevel == GasLevel::Warning || gGasLevel == GasLevel::Danger;
 
    if (WiFi.status() == WL_CONNECTED) {
      uploadLatestToFirebase(ppmCompensated,
                             ppmRaw,
                             gSen0565Sensor.voltage,
                             temperatureC,
                             humidityPct,
                             stateName,
                             fanOn,
                             buzzerOn,
                             thermalRisk);
 
      if (shouldUploadAlertHistory ||
          gLastFirebaseHistoryMs == 0 ||
          now - gLastFirebaseHistoryMs >= kFirebaseHistoryIntervalMs) {
        if (uploadHistoryToFirebase(ppmCompensated,
                                    ppmRaw,
                                    gSen0565Sensor.voltage,
                                    temperatureC,
                                    humidityPct,
                                    stateName,
                                    fanOn,
                                    buzzerOn,
                                    thermalRisk)) {
          gLastFirebaseHistoryMs = now;
        }
      }
    }
 
    if (gDisplayReady) {
      drawStatusScreen(temperatureC,
                       humidityPct,
                       pressureHpa,
                       gSen0565Sensor,
                       gGasLevel,
                       gDangerAlarmActive,
                       gDangerAlarmActive);
    }
  }
}
 
 
