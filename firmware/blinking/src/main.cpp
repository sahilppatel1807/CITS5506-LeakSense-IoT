#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

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
constexpr unsigned long kMicsBaselineDurationMs = 10000;
constexpr float kMicsGasDeltaThresholdV = 0.25f;
constexpr float kMicsGasRatioThreshold = 1.40f;
constexpr bool kRelayActiveLow = true;

TwoWire gOledWire = TwoWire(0);
TwoWire gBmeWire = TwoWire(1);
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &gOledWire, -1);
Adafruit_BME280 gBme;

bool gDisplayReady = false;
bool gBmeReady = false;
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
    gGasDetected = isMicsGasDetected(micsVoltage);
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

    if (gDisplayReady) {
      drawStatusScreen(temperatureC, humidityPct, pressureHpa, micsRaw, micsVoltage, gGasDetected);
    }
  }
}
