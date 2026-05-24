# LeakSense - Smart LPG Gas Leakage Detection and Automatic Safety System

## Overview

LeakSense is an IoT-based smart LPG safety system that monitors gas sensor
voltage, evaluates environmental risk, activates local safety outputs, and
publishes live readings to Firebase for remote dashboard monitoring.

The system combines:

- SEN0565 analog gas sensing
- BME280 temperature and humidity sensing
- ESP32 local safety logic
- Relay-controlled ventilation
- Buzzer and LED alerts
- OLED local status display
- Firebase Realtime Database telemetry
- Browser dashboard with live chart, history, and notifications

---

## Problem

Residential LPG leaks can create fire, explosion, and health hazards. Basic
home detectors usually provide only a local audible alarm, with no automatic
ventilation, remote monitoring, or historical event log. LeakSense addresses
this gap with a low-cost IoT safety system that responds locally and reports
state changes remotely.

---

## System Architecture

```text
SEN0565 Gas Sensor (analog) -- GPIO39
                                  |
BME280 (I2C bus 1) -------------- | 
                                  v
                              ESP32-E
                                  |
      ---------------------------------------------------------
      |              |              |           |             |
  Relay/Fan       Buzzer       RGB LEDs       OLED        Wi-Fi
      |              |              |           |             |
 Ventilation     Alarm      State display  Local UI     Firebase
                                                            |
                                                    Web Dashboard
```

### Five IoT Components

| # | Component | Implementation |
|---:|---|---|
| 1 | Sensors | SEN0565 gas sensor + BME280 temperature/humidity sensor |
| 2 | Computing node | FireBeetle ESP32-E |
| 3 | Communication | Wi-Fi + HTTPS Firebase REST upload |
| 4 | Cloud and dashboard | Firebase Realtime Database + HTML/CSS/JavaScript dashboard |
| 5 | Actions | Relay-controlled fan, buzzer, green/yellow/red LEDs, OLED |

---

## Safety Logic

LeakSense uses gas voltage thresholds from the SEN0565 and a thermal
escalation rule from the BME280.

| State | Gas condition | Local response | Dashboard/cloud |
|---|---|---|---|
| Safe | Voltage < 1.60 V | Green LED on, fan off, buzzer off | Live reading shown |
| Warning | Voltage >= 1.60 V and < 1.90 V | Yellow LED on | Warning state logged |
| Danger | Voltage >= 1.90 V | Red LED, fan on, buzzer cycle | Danger alert logged |
| Extreme | Voltage >= 2.20 V | Same local response as Danger | Extreme dashboard label |

Thermal escalation promotes Warning to Danger when the temperature rises by
5 C or more between readings, or when absolute temperature reaches 55 C.

Local actuation runs on the ESP32. Fan, buzzer, and LED safety behaviour
continues even when Wi-Fi or Firebase is unavailable.

---

## Hardware Components

| Component | Purpose |
|---|---|
| FireBeetle ESP32-E | Main IoT controller |
| SEN0565 MEMS CH4 gas sensor | Analog gas voltage input |
| BME280 atmospheric sensor | Temperature and humidity sensing |
| OLED SSD1306 display | Local live status output |
| 5 V relay module | Exhaust fan switching |
| Delta 5 V / 40 mm fan | Ventilation response |
| Gravity digital buzzer | Audible alarm |
| Green LED | Safe state indicator |
| Yellow LED | Warning state indicator |
| Red LED | Danger and Extreme indicator |
| Push button | Silences current alarm cycle |
| Breadboard and jumper wires | Circuit assembly |

---

## Wiring Summary

| Signal / device | ESP32 pin |
|---|---|
| SEN0565 analog output | GPIO39 |
| OLED SDA | GPIO18 |
| OLED SCL | GPIO23 |
| BME280 SDA | GPIO22 |
| BME280 SCL | GPIO21 |
| Relay / fan control | GPIO26 |
| Buzzer | GPIO25 |
| Green LED | GPIO17 |
| Yellow LED | GPIO16 |
| Red LED | GPIO4 |
| Alarm cancel button | GPIO14 |

The OLED and BME280 use separate I2C buses to avoid address conflicts and make
wiring/debugging clearer.

---

## Repository Structure

```text
CITS5506-LeakSense-IoT/
|
├── dashboard/
│   ├── index.html
│   ├── css/
│   │   └── style.css
│   ├── js/
│   │   ├── charts.js
│   │   ├── dashboard.js
│   │   ├── firebase.js
│   │   ├── mock.js
│   │   ├── secrets.example.js
│   │   └── secrets.js              # local only, not committed
│   └── service-worker.js
|
├── firmware/
│   └── blinking/
│       ├── platformio.ini
│       └── src/
│           ├── main.cpp
│           ├── secrets.h.example
│           └── secrets.h           # local only, not committed
|
├── docs/
├── report/
└── README.md
```

---

## Getting Started

### Dashboard

The dashboard is a plain HTML/CSS/JavaScript app.

```bash
open dashboard/index.html
```

For live Firebase data:

1. Copy `dashboard/js/secrets.example.js` to `dashboard/js/secrets.js`.
2. Paste the Firebase web app config into `secrets.js`.
3. Open `dashboard/index.html` in a browser.
4. Allow browser notifications if alert popups are required.

If Firebase is not configured, the dashboard falls back to mock readings from
`dashboard/js/mock.js`.

### Firmware

Requirements:

- VS Code
- PlatformIO extension
- FireBeetle ESP32-E connected by USB

```bash
cp firmware/blinking/src/secrets.h.example firmware/blinking/src/secrets.h
```

Then edit `firmware/blinking/src/secrets.h` with:

- Wi-Fi SSID
- Wi-Fi password
- Firebase Realtime Database host
- Firebase database secret

Build and upload from PlatformIO using the `firebeetle2_esp32e` environment.
Open the Serial Monitor at `115200` baud to view sensor readings, state
classification, Wi-Fi status, and Firebase upload logs.

---

## Firebase Data Shape

The ESP32 writes the latest reading to `leaksense/latest` and appends history
records to `leaksense/history`.

```json
{
  "ppm_compensated": 120.5,
  "ppm_raw": 130.0,
  "voltage": 0.682,
  "temperature": 23.4,
  "humidity": 48.2,
  "state": "SAFE",
  "fan": false,
  "buzzer": false,
  "thermal_risk": false,
  "timestamp": {
    ".sv": "timestamp"
  }
}
```

---

## Technology Stack

| Layer | Technology |
|---|---|
| Microcontroller | FireBeetle ESP32-E, Arduino framework, PlatformIO |
| Gas sensing | SEN0565 analog gas sensor |
| Environmental sensing | BME280 |
| Local display | SSD1306 OLED |
| Communication | Wi-Fi, HTTPS REST |
| Cloud | Firebase Realtime Database |
| Dashboard | HTML, CSS, JavaScript, Chart.js |
| Version control | Git + GitHub |

---

## Important Notes

- Do not commit `firmware/blinking/src/secrets.h`.
- Do not commit `dashboard/js/secrets.js`.
- Safety thresholds are defined in `firmware/blinking/src/main.cpp`.
- Dashboard display thresholds are defined in `dashboard/js/dashboard.js` and
  `dashboard/js/charts.js`.
- The SEN0565 is qualitative; formal calibration against certified LPG
  reference gas is required before production deployment.
