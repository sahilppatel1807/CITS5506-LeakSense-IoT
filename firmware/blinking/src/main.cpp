#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

constexpr uint8_t kLedPin = D9;
constexpr uint8_t kOledSdaPin = 18;
constexpr uint8_t kOledSclPin = 23;
constexpr uint8_t kBmeSdaPin = 22;
constexpr uint8_t kBmeSclPin = 21;
constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr uint8_t kOledAddresses[] = {0x3C, 0x3D};
constexpr uint8_t kBmeAddresses[] = {0x76, 0x77};
constexpr unsigned long kBlinkIntervalMs = 1000;
constexpr unsigned long kSensorReadIntervalMs = 2000;

TwoWire gOledWire = TwoWire(0);
TwoWire gBmeWire = TwoWire(1);
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &gOledWire, -1);
Adafruit_BME280 gBme;

bool gDisplayReady = false;
bool gBmeReady = false;
uint8_t gDisplayAddress = 0;
uint8_t gBmeAddress = 0;
unsigned long gLastSensorReadMs = 0;

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

void drawStatusScreen(float temperatureC, float humidityPct, float pressureHpa) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("PiicoDev Test");
  display.printf("OLED: 0x%02X\r\n", gDisplayAddress);

  if (gBmeReady) {
    display.printf("BME:  0x%02X\r\n", gBmeAddress);
    display.printf("T: %.1f C\r\n", temperatureC);
    display.printf("H: %.1f %%\r\n", humidityPct);
    display.printf("P: %.1f hPa\r\n", pressureHpa);
  } else {
    display.println("BME280 not found");
    display.printf("SDA:%u SCL:%u\r\n", kBmeSdaPin, kBmeSclPin);
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(kLedPin, OUTPUT);
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
}

void loop() {
  digitalWrite(kLedPin, HIGH);
  delay(kBlinkIntervalMs);
  digitalWrite(kLedPin, LOW);
  delay(kBlinkIntervalMs);

  if (millis() - gLastSensorReadMs >= kSensorReadIntervalMs) {
    gLastSensorReadMs = millis();

    float temperatureC = NAN;
    float humidityPct = NAN;
    float pressureHpa = NAN;

    if (gBmeReady) {
      temperatureC = gBme.readTemperature();
      humidityPct = gBme.readHumidity();
      pressureHpa = gBme.readPressure() / 100.0f;

      Serial.printf("BME280 0x%02X -> T=%.2f C, H=%.2f %%, P=%.2f hPa\r\n",
                    gBmeAddress, temperatureC, humidityPct, pressureHpa);
    } else {
      Serial.println("BME280 not ready");
    }

    if (gDisplayReady) {
      drawStatusScreen(temperatureC, humidityPct, pressureHpa);
    }
  }
}
