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
constexpr unsigned long kRedBlinkIntervalMs = 500;
constexpr unsigned long kSensorReadIntervalMs = 2000;
constexpr unsigned long kFirebaseHistoryIntervalMs = 60000;
constexpr unsigned long kGasBaselineDurationMs = 30000;
constexpr unsigned long kLedSelfTestIntervalMs = 500;
constexpr unsigned long kDangerAlarmDurationMs = 10000;
constexpr unsigned long kDetectionPauseDurationMs = 10000;
constexpr unsigned long kButtonDebounceMs = 50;
constexpr float kSen0565WarningDeltaThresholdV = 0.18f;
constexpr float kSen0565WarningRatioThreshold = 1.10f;
constexpr float kSen0565DangerDeltaThresholdV = 0.33f;
constexpr float kSen0565DangerRatioThreshold = 1.18f;
constexpr float kSen0565ExtremeDeltaThresholdV = 0.52f;
constexpr float kSen0565ExtremeRatioThreshold = 1.30f;
constexpr uint8_t kGasLevelConfirmReads = 2;
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
  float warningDeltaThresholdV;
  float warningRatioThreshold;
  float dangerDeltaThresholdV;
  float dangerRatioThreshold;
  float extremeDeltaThresholdV;
  float extremeRatioThreshold;
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
 
bool gLedOn = false;
 
bool gGasDetected = false;
bool gDangerAlarmActive = false;
bool gDetectionPaused = false;
bool gRedBlinkOn = false;
bool gGreenBlinkOn = false;
bool gLastButtonReading = HIGH;
bool gButtonStableState = HIGH;
GasLevel gGasLevel = GasLevel::Safe;
GasLevel gLastGasLevel = GasLevel::Safe;
GasLevel gPendingGasLevel = GasLevel::Safe;
uint8_t gPendingGasLevelCount = 0;
unsigned long gStartMs = 0;
unsigned long gLastBlinkMs = 0;
unsigned long gLastRedBlinkMs = 0;
unsigned long gLastGreenBlinkMs = 0;
unsigned long gLastSensorReadMs = 0;
unsigned long gLastFirebaseHistoryMs = 0;
unsigned long gDangerAlarmStartMs = 0;
unsigned long gDetectionPauseStartMs = 0;
unsigned long gLastButtonChangeMs = 0;
GasSensorState gSen0565Sensor = {"SEN0565",
                                 kSen0565AnalogPin,
                                 kSen0565WarningDeltaThresholdV,
                                 kSen0565WarningRatioThreshold,
                                 kSen0565DangerDeltaThresholdV,
                                 kSen0565DangerRatioThreshold,
                                 kSen0565ExtremeDeltaThresholdV,
                                 kSen0565ExtremeRatioThreshold,
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
  gGreenBlinkOn = true;
  gLastGreenBlinkMs = now;
  setStatusLeds(true, false, false);
}
 
void updateDetectionPause(unsigned long now) {
  if (!gDetectionPaused) {
    return;
  }
 
  if (now - gDetectionPauseStartMs >= kDetectionPauseDurationMs) {
    gDetectionPaused = false;
    gGreenBlinkOn = false;
    gLastSensorReadMs = 0;
    Serial.println("Detection pause ended; monitoring resumed");
    return;
  }
 
  if (now - gLastGreenBlinkMs >= kBlinkIntervalMs) {
    gLastGreenBlinkMs = now;
    gGreenBlinkOn = !gGreenBlinkOn;
  }
  setStatusLeds(gGreenBlinkOn, false, false);
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
 
bool connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
 
  Serial.printf("Connecting to WiFi SSID='%s'...\r\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi connected, IP=%s\r\n", WiFi.localIP().toString().c_str());
      gWiFiConnected = true;
      return true;
    }
    Serial.print('.');
    delay(500);
  }
 
  Serial.println();
  Serial.println("WiFi connection failed");
  gWiFiConnected = false;
  return false;
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
 
String firebasePayload(float ppm, float rawPpm, float temperatureC, float humidityPct, const char* state, bool fan, bool buzzer) {
  String payload = "{";
  payload += "\"ppm_compensated\":" + jsonFloat(ppm, 1) + ",";
  payload += "\"ppm_raw\":" + jsonFloat(rawPpm, 1) + ",";
  payload += "\"temperature\":" + jsonFloat(temperatureC, 1) + ",";
  payload += "\"humidity\":" + jsonFloat(humidityPct, 1) + ",";
  payload += "\"state\":\"" + String(state) + "\",";
  payload += "\"fan\":" + String(fan ? "true" : "false") + ",";
  payload += "\"buzzer\":" + String(buzzer ? "true" : "false") + ",";
  payload += "\"timestamp\":{\".sv\":\"timestamp\"}";
  payload += "}";
  return payload;
}
 
bool uploadReadingToFirebase(const char* node,
                             bool append,
                             float ppmCompensated,
                             float ppmRaw,
                             float temperatureC,
                             float humidityPct,
                             const char* state,
                             bool fan,
                             bool buzzer) {
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
  const String payload = firebasePayload(ppmCompensated, ppmRaw, temperatureC, humidityPct, state, fan, buzzer);
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
                            float temperatureC,
                            float humidityPct,
                            const char* state,
                            bool fan,
                            bool buzzer) {
  return uploadReadingToFirebase("latest",
                                 false,
                                 ppmCompensated,
                                 ppmRaw,
                                 temperatureC,
                                 humidityPct,
                                 state,
                                 fan,
                                 buzzer);
}
 
bool uploadHistoryToFirebase(float ppmCompensated,
                             float ppmRaw,
                             float temperatureC,
                             float humidityPct,
                             const char* state,
                             bool fan,
                             bool buzzer) {
  return uploadReadingToFirebase("history",
                                 true,
                                 ppmCompensated,
                                 ppmRaw,
                                 temperatureC,
                                 humidityPct,
                                 state,
                                 fan,
                                 buzzer);
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
 
void handleAlarmCancelButton(unsigned long now) {
  const bool reading = digitalRead(kAlarmCancelButtonPin);
  if (reading != gLastButtonReading) {
    gLastButtonReading = reading;
    gLastButtonChangeMs = now;
  }
 
  if (now - gLastButtonChangeMs < kButtonDebounceMs || reading == gButtonStableState) {
    return;
  }
 
  gButtonStableState = reading;
  if (gButtonStableState == LOW) {
    if (gDangerAlarmActive) {
      Serial.println("Detection paused by GPIO14 button for 10 seconds");
      startDetectionPause(now);
    } else {
      Serial.println("GPIO14 button ignored because alarm is not active");
    }
  }
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
  if (!sensor.baselineReady || sensor.baselineVoltage <= 0.01f) {
    return GasLevel::Safe;
  }

  const float delta = gasDeltaMagnitude(sensor);
  const float ratio = gasNormalizedRatio(sensor);
  if (delta > sensor.extremeDeltaThresholdV || ratio > sensor.extremeRatioThreshold) {
    return GasLevel::Danger;
  }
  if (delta > sensor.dangerDeltaThresholdV || ratio > sensor.dangerRatioThreshold) {
    return GasLevel::Danger;
  }
  if (delta > sensor.warningDeltaThresholdV || ratio > sensor.warningRatioThreshold) {
    return GasLevel::Warning;
  }
  return GasLevel::Safe;
}
 
void sampleGasSensor(GasSensorState& sensor) {
  sensor.raw = analogRead(sensor.analogPin);
  sensor.voltage = readAnalogVoltage(sensor.analogPin);
  updateGasBaseline(sensor);
  const float delta = gasDeltaMagnitude(sensor);
  const float ratio = gasNormalizedRatio(sensor);
  sensor.extreme = sensor.baselineReady &&
                   (delta > sensor.extremeDeltaThresholdV || ratio > sensor.extremeRatioThreshold);
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
  gLastButtonReading = digitalRead(kAlarmCancelButtonPin);
  gButtonStableState = gLastButtonReading;
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
  Serial.println("Keep SEN0565 in clean air for the first 30 seconds.");
  Serial.println("Warm SEN0565 for at least 5 minutes before trusting its thresholds.");
 
  if (connectToWiFi()) {
    Serial.println("WiFi connected, ready for Firebase upload.");
  } else {
    Serial.println("WiFi not connected. Will retry in loop().");
  }
}
 
void loop() {
  const unsigned long now = millis();
  handleAlarmCancelButton(now);
  updateDangerAlarm(now);
  updateStatusLeds(now);
 
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
    gLastGasLevel = gGasLevel;
    gGasLevel = confirmGasLevel(gSen0565Sensor.level, gSen0565Sensor.extreme);
    gGasDetected = gGasLevel == GasLevel::Danger;
    if (gGasDetected && gLastGasLevel != GasLevel::Danger) {
      startDangerAlarm(now);
    }
    updateDangerAlarm(now);
    updateStatusLeds(now);
 
    if (gBmeReady) {
      temperatureC = gBme.readTemperature();
      humidityPct = gBme.readHumidity();
      pressureHpa = gBme.readPressure() / 100.0f;
 
      Serial.printf("BME280 0x%02X -> T=%.2f C, H=%.2f %%, P=%.2f hPa\r\n",
                    gBmeAddress, temperatureC, humidityPct, pressureHpa);
    } else {
      Serial.println("BME280 not ready");
    }
 
    printGasSensor(gSen0565Sensor);
    Serial.printf("Alarm source -> SEN0565 raw=%s, confirmed=%s, alarm=%s, fan=%s\r\n",
                  gasLevelName(gSen0565Sensor.level),
                  gasLevelName(gGasLevel),
                  gDangerAlarmActive ? "ON" : "off",
                  gDangerAlarmActive ? "ON" : "off");
 
 
 
 
 
 
 
 
    if (!gWiFiConnected) {
      if (connectToWiFi()) {
        Serial.println("WiFi reconnected in loop().");
      }
    }
 
    const float ppmCompensated = estimatePpm(gSen0565Sensor);
    const float ppmRaw = gSen0565Sensor.raw * 0.25f;
    const char* stateName = gasLevelName(gGasLevel);
    const bool fanOn = gDangerAlarmActive;
    const bool buzzerOn = gDangerAlarmActive;
 
    if (gWiFiConnected) {
      uploadLatestToFirebase(ppmCompensated,
                             ppmRaw,
                             temperatureC,
                             humidityPct,
                             stateName,
                             fanOn,
                             buzzerOn);
 
      if (gLastFirebaseHistoryMs == 0 || now - gLastFirebaseHistoryMs >= kFirebaseHistoryIntervalMs) {
        if (uploadHistoryToFirebase(ppmCompensated,
                                    ppmRaw,
                                    temperatureC,
                                    humidityPct,
                                    stateName,
                                    fanOn,
                                    buzzerOn)) {
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
 
 
