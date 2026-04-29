// Minimal ESP32 GPIO blink test for external wiring.
constexpr int kLedPin = 18;
constexpr unsigned long kBlinkIntervalMs = 1000;

void setup() {
  pinMode(kLedPin, OUTPUT);
}

void loop() {
  digitalWrite(kLedPin, HIGH);
  delay(kBlinkIntervalMs);

  digitalWrite(kLedPin, LOW);
  delay(kBlinkIntervalMs);
}
