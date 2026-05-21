/**
 * main.cpp — LeakSense
 * Smart LPG / CH4 Gas Leakage Detection and Automatic Safety System
 * Group 27 · CITS5506 IoT · UWA Semester 1 2026
 *
 * Hardware:
 *   - FireBeetle ESP32-E
 *   - SEN0565 CH4 Gas Sensor (I2C on OLED bus, address 0x74)
 *   - BME280 Atmospheric Sensor (I2C bus 1, address 0x76 or 0x77)
 *   - OLED SSD1306 128x64 (I2C bus 0, address 0x3C or 0x3D)
 *   - HL-52S Relay Module → Exhaust Fan (Danger only) → GPIO 25
 *   - 5V Buzzer (Danger only — 10s cycle, button silences current cycle) → GPIO 32
 *   - Green LED  → Safe state    → GPIO 17
 *   - Yellow LED → Warning state → GPIO 33
 *   - Red LED    → Danger state  → GPIO 16
 *   - Push Button → silences buzzer for current cycle → GPIO 27
 *   - Built-in LED → D9 (heartbeat blink)
 *
 * State machine:
 *   SAFE    < 300 ppm  → Green LED on,  fan off,  buzzer off
 *   WARNING 300–500 ppm → Yellow LED on, fan off,  buzzer off
 *   DANGER  > 500 ppm  → Red LED on,    fan on,   buzzer on (10s cycle)
 *
 * Cloud:
 *   leaksense/latest  → pushed every 5 seconds
 *   leaksense/history → pushed every 5 minutes (for 24hr dashboard)
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

// Display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// BME280
#include <Adafruit_BME280.h>

// SEN0565 CH4 gas sensor
#include <DFRobot_GasSensor.h>

// Firebase
#include <FirebaseESP32.h>

// Credentials
#include "secrets.h"

// ── Pin assignments (preserved from original wiring) ──────────────────────────
constexpr uint8_t kLedPin        = D9;   // Built-in heartbeat LED

constexpr uint8_t kGreenLedPin   = 17;   // Safe indicator
constexpr uint8_t kYellowLedPin  = 33;   // Warning indicator (new)
constexpr uint8_t kRedLedPin     = 16;   // Danger indicator

constexpr uint8_t kOledSdaPin    = 18;   // OLED I2C bus 0
constexpr uint8_t kOledSclPin    = 23;

constexpr uint8_t kBmeSdaPin     = 22;   // BME280 I2C bus 1
constexpr uint8_t kBmeSclPin     = 21;

constexpr uint8_t kRelayPin      = 25;   // HL-52S relay → exhaust fan
constexpr uint8_t kBuzzerPin     = 32;   // 5V buzzer (new)
constexpr uint8_t kButtonPin     = 27;   // Silence button (new, INPUT_PULLUP)

// ── Screen ────────────────────────────────────────────────────────────────────
constexpr uint8_t kScreenWidth  = 128;
constexpr uint8_t kScreenHeight = 64;

// ── I2C addresses ─────────────────────────────────────────────────────────────
constexpr uint8_t kOledAddresses[] = {0x3C, 0x3D};
constexpr uint8_t kBmeAddresses[]  = {0x76, 0x77};
constexpr uint8_t kGasAddress      = 0x74;  // SEN0565

// ── Thresholds (ppm) ──────────────────────────────────────────────────────────
constexpr float kWarningPpm = 300.0f;
constexpr float kDangerPpm  = 500.0f;

// ── Timing ────────────────────────────────────────────────────────────────────
constexpr unsigned long kBlinkIntervalMs    =     500UL;  // heartbeat LED
constexpr unsigned long kSensorReadIntervalMs =  2000UL;  // sensor poll
constexpr unsigned long kFirebaseLatestMs   =   5000UL;  // push latest
constexpr unsigned long kFirebaseHistoryMs  = 300000UL;  // push history (5 min)
constexpr unsigned long kBuzzerCycleMs      =  10000UL;  // buzzer ON duration
constexpr unsigned long kDebounceMs         =     50UL;  // button debounce

// ── Relay polarity ────────────────────────────────────────────────────────────
constexpr bool kRelayActiveLow = true;

// ── System state enum ─────────────────────────────────────────────────────────
enum class GasState { SAFE, WARNING, DANGER };

// ── Two separate I2C buses (preserved from original) ─────────────────────────
TwoWire gOledWire = TwoWire(0);   // OLED + SEN0565 gas sensor
TwoWire gBmeWire  = TwoWire(1);   // BME280

// ── Objects ───────────────────────────────────────────────────────────────────
Adafruit_SSD1306  gDisplay(kScreenWidth, kScreenHeight, &gOledWire, -1);
Adafruit_BME280   gBme;
DFRobot_GasSensor gGasSensor(&gOledWire, kGasAddress);

FirebaseData      gFirebaseData;
FirebaseAuth      gFirebaseAuth;
FirebaseConfig    gFirebaseConfig;

// ── Runtime flags ─────────────────────────────────────────────────────────────
bool gDisplayReady  = false;
bool gBmeReady      = false;
bool gGasReady      = false;
bool gFirebaseReady = false;

uint8_t gDisplayAddress = 0;
uint8_t gBmeAddress     = 0;

// ── State ─────────────────────────────────────────────────────────────────────
GasState gCurrentState = GasState::SAFE;
GasState gPrevState    = GasState::SAFE;

// Sensor readings
float gPpm         = 0.0f;
float gTemperature = 0.0f;
float gHumidity    = 0.0f;
float gPressure    = 0.0f;

// Buzzer
bool          gBuzzerActive   = false;
bool          gBuzzerSilenced = false;
unsigned long gBuzzerStartMs  = 0;

// Button debounce
bool          gLastButtonState = HIGH;
unsigned long gLastDebounceMs  = 0;

// Heartbeat LED
bool          gLedOn        = false;
unsigned long gLastBlinkMs  = 0;

// Timing
unsigned long gLastSensorMs  = 0;
unsigned long gLastLatestMs  = 0;
unsigned long gLastHistoryMs = 0;

// ── Helper: state string ──────────────────────────────────────────────────────
const char* stateStr(GasState s) {
  switch (s) {
    case GasState::WARNING: return "warning";
    case GasState::DANGER:  return "danger";
    default:                return "safe";
  }
}

GasState stateFromPpm(float ppm) {
  if (ppm >= kDangerPpm)  return GasState::DANGER;
  if (ppm >= kWarningPpm) return GasState::WARNING;
  return GasState::SAFE;
}

// ── Relay ─────────────────────────────────────────────────────────────────────
void setRelay(bool on) {
  digitalWrite(kRelayPin, (kRelayActiveLow ? !on : on) ? HIGH : LOW);
}

// ── LEDs ──────────────────────────────────────────────────────────────────────
void updateLeds(GasState state) {
  digitalWrite(kGreenLedPin,  state == GasState::SAFE    ? HIGH : LOW);
  digitalWrite(kYellowLedPin, state == GasState::WARNING ? HIGH : LOW);
  digitalWrite(kRedLedPin,    state == GasState::DANGER  ? HIGH : LOW);
}

// ── Buzzer ────────────────────────────────────────────────────────────────────
void updateBuzzer(unsigned long now, GasState state) {
  if (state != GasState::DANGER) {
    gBuzzerActive   = false;
    gBuzzerSilenced = false;
    digitalWrite(kBuzzerPin, LOW);
    return;
  }

  // Start first cycle or restart after 10s
  if (!gBuzzerActive || (now - gBuzzerStartMs >= kBuzzerCycleMs)) {
    gBuzzerActive   = true;
    gBuzzerSilenced = false;
    gBuzzerStartMs  = now;
    digitalWrite(kBuzzerPin, HIGH);
    return;
  }

  // Mid-cycle: honour silence
  if (gBuzzerSilenced) {
    digitalWrite(kBuzzerPin, LOW);
  }
}

// ── Button ────────────────────────────────────────────────────────────────────
void handleButton(unsigned long now) {
  const bool reading = digitalRead(kButtonPin);

  if (reading != gLastButtonState) {
    gLastDebounceMs = now;
  }

  if ((now - gLastDebounceMs) >= kDebounceMs) {
    // Falling edge = button pressed (active LOW, INPUT_PULLUP)
    if (reading == LOW && gLastButtonState == HIGH) {
      if (gBuzzerActive && !gBuzzerSilenced) {
        gBuzzerSilenced = true;
        digitalWrite(kBuzzerPin, LOW);
        Serial.println("Button pressed — buzzer silenced for this cycle.");
      }
    }
  }

  gLastButtonState = reading;
}

// ── I2C scanner ───────────────────────────────────────────────────────────────
void scanI2CBus(TwoWire& bus, const char* label) {
  Serial.printf("Scanning %s I2C bus...\r\n", label);
  for (uint8_t address = 1; address < 127; ++address) {
    bus.beginTransmission(address);
    if (bus.endTransmission() == 0) {
      Serial.printf("  %s bus device found at 0x%02X\r\n", label, address);
    }
  }
}

// ── Display helpers ───────────────────────────────────────────────────────────
bool initDisplay() {
  for (uint8_t address : kOledAddresses) {
    if (gDisplay.begin(SSD1306_SWITCHCAPVCC, address)) {
      gDisplayAddress = address;
      return true;
    }
  }
  return false;
}

void drawOled(GasState state, float ppm, float temp, float hum) {
  if (!gDisplayReady) return;

  gDisplay.clearDisplay();
  gDisplay.setTextSize(1);
  gDisplay.setTextColor(SSD1306_WHITE);

  gDisplay.setCursor(0, 0);
  gDisplay.println("LeakSense");

  gDisplay.setCursor(0, 12);
  switch (state) {
    case GasState::SAFE:    gDisplay.println("State: SAFE");    break;
    case GasState::WARNING: gDisplay.println("State: WARNING"); break;
    case GasState::DANGER:  gDisplay.println("State: DANGER!"); break;
  }

  gDisplay.setCursor(0, 24);
  gDisplay.printf("Gas:  %.0f ppm\n", ppm);

  gDisplay.setCursor(0, 36);
  gDisplay.printf("Temp: %.1f C\n", temp);

  gDisplay.setCursor(0, 48);
  gDisplay.printf("Hum:  %.1f %%\n", hum);

  gDisplay.display();
}

// ── BME280 init ───────────────────────────────────────────────────────────────
bool initBme() {
  for (uint8_t address : kBmeAddresses) {
    if (gBme.begin(address, &gBmeWire)) {
      gBmeAddress = address;
      return true;
    }
  }
  return false;
}

// ── WiFi ──────────────────────────────────────────────────────────────────────
void initWiFi() {
  Serial.printf("Connecting to WiFi: %s\r\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\r\nWiFi connected — IP: %s\r\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\r\nWiFi failed — running offline. Local safety still active.");
  }
}

// ── Firebase ──────────────────────────────────────────────────────────────────
void initFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;

  gFirebaseConfig.host = FIREBASE_HOST;
  gFirebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&gFirebaseConfig, &gFirebaseAuth);
  Firebase.reconnectWiFi(true);
  gFirebaseReady = true;
  Serial.println("Firebase initialised.");
}

void pushLatest() {
  if (!gFirebaseReady || WiFi.status() != WL_CONNECTED) return;

  const bool fanOn    = (gCurrentState == GasState::DANGER);
  const bool buzzerOn = (gCurrentState == GasState::DANGER && gBuzzerActive && !gBuzzerSilenced);

  FirebaseJson json;
  json.set("ppm_compensated", (int)gPpm);
  json.set("temperature",     gTemperature);
  json.set("humidity",        gHumidity);
  json.set("state",           stateStr(gCurrentState));
  json.set("fan",             fanOn);
  json.set("buzzer",          buzzerOn);
  json.set("timestamp",       (long)(millis() / 1000));

  if (!Firebase.setJSON(gFirebaseData, "/leaksense/latest", json)) {
    Serial.printf("Firebase latest failed: %s\r\n", gFirebaseData.errorReason().c_str());
  }
}

void pushHistory() {
  if (!gFirebaseReady || WiFi.status() != WL_CONNECTED) return;

  const bool fanOn    = (gCurrentState == GasState::DANGER);
  const bool buzzerOn = (gCurrentState == GasState::DANGER && gBuzzerActive && !gBuzzerSilenced);

  FirebaseJson json;
  json.set("ppm_compensated", (int)gPpm);
  json.set("temperature",     gTemperature);
  json.set("humidity",        gHumidity);
  json.set("state",           stateStr(gCurrentState));
  json.set("fan",             fanOn);
  json.set("buzzer",          buzzerOn);
  json.set("timestamp",       (long)(millis() / 1000));

  if (!Firebase.pushJSON(gFirebaseData, "/leaksense/history", json)) {
    Serial.printf("Firebase history failed: %s\r\n", gFirebaseData.errorReason().c_str());
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\r\n=== LeakSense booting ===");

  // GPIO
  pinMode(kLedPin,       OUTPUT);
  pinMode(kGreenLedPin,  OUTPUT);
  pinMode(kYellowLedPin, OUTPUT);
  pinMode(kRedLedPin,    OUTPUT);
  pinMode(kRelayPin,     OUTPUT);
  pinMode(kBuzzerPin,    OUTPUT);
  pinMode(kButtonPin,    INPUT_PULLUP);

  // Safe defaults on boot
  digitalWrite(kGreenLedPin,  HIGH);
  digitalWrite(kYellowLedPin, LOW);
  digitalWrite(kRedLedPin,    LOW);
  digitalWrite(kBuzzerPin,    LOW);
  setRelay(false);

  // Relay self-test
  Serial.println("Relay self-test...");
  setRelay(true);
  delay(500);
  setRelay(false);

  // OLED bus (bus 0: SDA=18, SCL=23)
  gOledWire.begin(kOledSdaPin, kOledSclPin);
  scanI2CBus(gOledWire, "OLED");

  gDisplayReady = initDisplay();
  if (gDisplayReady) {
    Serial.printf("OLED ready at 0x%02X\r\n", gDisplayAddress);
    gDisplay.clearDisplay();
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.setCursor(0, 0);
    gDisplay.println("LeakSense");
    gDisplay.println("Booting...");
    gDisplay.display();
  } else {
    Serial.println("OLED init failed. Check SDA=18, SCL=23.");
  }

  // BME280 bus (bus 1: SDA=22, SCL=21)
  gBmeWire.begin(kBmeSdaPin, kBmeSclPin);
  scanI2CBus(gBmeWire, "BME280");

  gBmeReady = initBme();
  if (gBmeReady) {
    Serial.printf("BME280 ready at 0x%02X\r\n", gBmeAddress);
  } else {
    Serial.println("BME280 init failed. Check SDA=22, SCL=21.");
  }

  // SEN0565 gas sensor (on OLED bus, address 0x74)
  gGasReady = (gGasSensor.begin() == 0);
  if (gGasReady) {
    Serial.printf("SEN0565 ready at 0x%02X\r\n", kGasAddress);
  } else {
    Serial.println("SEN0565 init failed. Check wiring on OLED bus.");
  }

  // WiFi + Firebase
  initWiFi();
  initFirebase();

  const unsigned long now = millis();
  gLastSensorMs  = now;
  gLastLatestMs  = now;
  gLastHistoryMs = now;
  gLastBlinkMs   = now;

  Serial.println("=== LeakSense ready ===\r\n");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  const unsigned long now = millis();

  // ── Heartbeat LED ──────────────────────────────────────────────────────────
  if (now - gLastBlinkMs >= kBlinkIntervalMs) {
    gLastBlinkMs = now;
    gLedOn = !gLedOn;
    digitalWrite(kLedPin, gLedOn ? HIGH : LOW);
  }

  // ── Button ─────────────────────────────────────────────────────────────────
  handleButton(now);

  // ── Sensor poll (every 2s) ─────────────────────────────────────────────────
  if (now - gLastSensorMs >= kSensorReadIntervalMs) {
    gLastSensorMs = now;

    // Gas sensor
    if (gGasReady) {
      gPpm = gGasSensor.readGasConcentrationPPM();
    }

    // BME280
    if (gBmeReady) {
      gTemperature = gBme.readTemperature();
      gHumidity    = gBme.readHumidity();
      gPressure    = gBme.readPressure() / 100.0f;
      Serial.printf("BME280 0x%02X -> T=%.2f C, H=%.2f %%, P=%.2f hPa\r\n",
                    gBmeAddress, gTemperature, gHumidity, gPressure);
    } else {
      Serial.println("BME280 not ready.");
    }

    // Derive new state
    gCurrentState = stateFromPpm(gPpm);

    if (gCurrentState != gPrevState) {
      Serial.printf("State: %s → %s (%.0f ppm)\r\n",
                    stateStr(gPrevState), stateStr(gCurrentState), gPpm);
      gPrevState = gCurrentState;
    }

    // Actuators
    updateLeds(gCurrentState);
    setRelay(gCurrentState == GasState::DANGER);

    // OLED
    drawOled(gCurrentState, gPpm, gTemperature, gHumidity);

    Serial.printf("[%s] ppm=%.0f  T=%.1fC  H=%.1f%%  fan=%s  buzzer=%s\r\n",
                  stateStr(gCurrentState),
                  gPpm,
                  gTemperature,
                  gHumidity,
                  (gCurrentState == GasState::DANGER) ? "ON" : "off",
                  (gBuzzerActive && !gBuzzerSilenced) ? "ON" : "off");
  }

  // ── Buzzer (checked every loop iteration) ─────────────────────────────────
  updateBuzzer(now, gCurrentState);

  // ── Firebase: latest (every 5s) ───────────────────────────────────────────
  if (now - gLastLatestMs >= kFirebaseLatestMs) {
    gLastLatestMs = now;
    pushLatest();
  }

  // ── Firebase: history (every 5 min) ───────────────────────────────────────
  if (now - gLastHistoryMs >= kFirebaseHistoryMs) {
    gLastHistoryMs = now;
    pushHistory();
  }
}