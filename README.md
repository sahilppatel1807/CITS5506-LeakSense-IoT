# LeakSense – Smart LPG Gas Leakage Detection and Automatic Safety System

**CITS5506: Internet of Things — Group 27**
The University of Western Australia

| Member | Student ID | Role |
|---|---|---|
| Yuxuan Xi | 24319908 | Hardware Assembly & Circuit Design |
| Yiming Zhao | 24139958 | ESP32 Firmware Development |
| Sahil Pankajbhai Patel | 24562775 | Web Dashboard Development |
| Sohaib Ahmed Khan | 24756957 | Cloud Integration & System Testing |

---

## Overview

LeakSense is an IoT-based smart safety system that continuously monitors LPG gas concentration and automatically responds when dangerous levels are detected. The system provides both local physical response (buzzer, fan, LEDs, OLED display) and remote monitoring through a cloud-connected web dashboard.

---

## Problem

LPG gas leaks in homes are a leading cause of fires, explosions, and carbon monoxide poisoning. Existing home detectors only produce a local alarm — they do not activate ventilation, log historical data, or allow remote monitoring. LeakSense addresses this gap with an integrated, low-cost IoT solution.

---

## System Architecture

```
Gas Sensor (I2C) ─┐
                  ├──► ESP32-E ──► Relay ──► Exhaust Fan
BME280 (I2C) ─────┘      │
                          ├──► Buzzer
                          ├──► LEDs (Green / Red)
                          ├──► OLED Display
                          └──► WiFi ──► Firebase ──► Web Dashboard
```

### Five IoT Components

| # | Component | Implementation |
|---|---|---|
| 1 | Sensors | DFRobot Calibrated Gas Sensor (I2C) + BME280 (temp/humidity) |
| 2 | Computing Node | FireBeetle ESP32-E |
| 3 | Communication | WiFi + Firebase Realtime Database |
| 4 | Cloud & Dashboard | Firebase + HTML/CSS/JS web dashboard |
| 5 | Actions | Buzzer, relay-controlled fan, LEDs, OLED |

---

## Three-State Safety Logic

| State | Threshold | Local Response | Cloud |
|---|---|---|---|
| SAFE | < 300 ppm | Green LED on, fan off, buzzer silent | Data logged |
| WARNING | 300–500 ppm | Red LED blink, fan on, buzzer intermittent | Alert sent |
| DANGER | > 500 ppm | Red LED solid, fan on, buzzer continuous | Alert sent |

> Thresholds are defined in `firmware/src/config.h` and `dashboard/js/charts.js`. Adjust after hardware calibration.

> Actuation logic runs entirely on-device — fan and buzzer activate even without WiFi.

---

## Hardware Components

| Component | Purpose | Status |
|---|---|---|
| FireBeetle ESP32-E | Main IoT controller | ✅ Available at UWA |
| DFRobot Calibrated Gas Sensor (I2C) | LPG detection — factory calibrated, direct ppm over I2C | ⏳ On order (1–3 weeks) |
| BME280 Atmospheric Sensor | Temperature and humidity display | ⏳ To be confirmed with Andy |
| OLED SSD1306 (I2C) | Local status display | ✅ Available at UWA |
| 5V Relay Module | Fan control with electrical isolation | ✅ Available at UWA |
| Delta 5V 40mm Exhaust Fan | Ventilation simulation | ⏳ To be confirmed with Andy |
| Gravity Digital Buzzer | Audible alert | ✅ Available at UWA |
| Green + Red LEDs | Visual status indicators | ✅ Available at UWA |
| Breadboard + Jumper Wires | Prototyping | ✅ Available at UWA |

> **Note:** The original MQ-6 I2C Qwiic sensor was replaced following advice from the UWA lab technician (Andrew Burrell). The DFRobot calibrated gas sensor outputs factory-calibrated ppm values directly over I2C, removing the need for manual Rs/R0 conversion and environmental compensation calculations.

### Wiring (I2C Bus)

All I2C devices share the same SDA/SCL pins — wired in parallel.

| Component | SDA | SCL | I2C Address |
|---|---|---|---|
| DFRobot Gas Sensor | GPIO 21 | GPIO 22 | 0x74 |
| BME280 | GPIO 21 | GPIO 22 | 0x76 |
| OLED SSD1306 | GPIO 21 | GPIO 22 | 0x3C |

| Actuator | ESP32 Pin |
|---|---|
| Relay (Fan) | GPIO 26 |
| Buzzer | GPIO 25 |
| Green LED | GPIO 32 |
| Red LED | GPIO 33 |

---

## Repository Structure

```
CITS5506-LEAKSENSE-IOT/
│
├── firmware/                  # ESP32 Arduino source code (Yiming)
│   ├── src/
│   │   ├── main.ino           # Main firmware loop
│   │   ├── config.h           # Pin definitions and thresholds
│   │   └── secrets.h          # WiFi + Firebase credentials (NOT committed)
│   └── platformio.ini         # PlatformIO board and library config
│
├── dashboard/                 # Web dashboard (Sahil)
│   ├── index.html             # Dashboard HTML structure
│   ├── css/
│   │   └── style.css          # All dashboard styles
│   └── js/
│       ├── mock.js            # Simulated sensor data (development only)
│       ├── charts.js          # Chart.js trend graph
│       ├── dashboard.js       # Main UI logic and state management
│       └── firebase.js        # Firebase realtime listener
│
├── docs/                      # Project documentation
│   └── firebeetle-esp32-e/    # Hardware datasheets and references
│
├── hardware/                  # Circuit diagrams and wiring images
├── testing/                   # Test results and logs
└── README.md
```

---

## Current Status

| Task | Owner | Status |
|---|---|---|
| Project proposal submitted | All | ✅ Done |
| Hardware collection from UWA lab | Yuxuan | ✅ Done |
| DFRobot gas sensor | Yuxuan | ⏳ Awaiting delivery (1–3 weeks) |
| Web dashboard (mock data) | Sahil | ✅ Done |
| Firebase setup | Sohaib | ⏳ Pending |
| ESP32 firmware | Yiming | ⏳ Waiting for hardware |
| Hardware assembly and wiring | Yuxuan | ⏳ Waiting for sensor delivery |
| End-to-end integration | All | ⏳ Pending |

---

## Getting Started

### Dashboard (Sahil)

No installation needed — plain HTML/CSS/JS.

```bash
# Just open in browser
open dashboard/index.html
```

Use the simulator slider to test Safe / Warning / Danger states with mock data.

**To connect to Firebase** once Sohaib has it set up:
1. Paste the Firebase config into `dashboard/js/firebase.js`
2. In `dashboard/js/dashboard.js`, comment out the mock interval and uncomment the Firebase listener
3. Uncomment the Firebase script tags in `index.html`

---

### Firmware (Yiming)

> ⚠️ Wait for the DFRobot gas sensor to arrive before writing sensor reading code. The library and I2C address depend on the exact model Andy orders.

**Requirements:** VS Code + PlatformIO extension

```bash
# 1. Open the firmware/ folder in VS Code
# 2. Create your secrets file — never commit this
cp firmware/src/secrets.h.example firmware/src/secrets.h

# 3. Fill in WiFi credentials and Firebase details in secrets.h
# 4. Click Upload in PlatformIO bottom toolbar
# 5. Open Serial Monitor at 115200 baud to see live readings
```

**DFRobot gas sensor — reading ppm is one line, no formula needed:**

```cpp
#include "DFRobot_GasSensor.h"
DFRobot_GasSensor_I2C sensor(&Wire, 0x74);
float ppm = sensor.readGasConcentrationPPM();
```

---

### Firebase Setup (Sohaib)

1. Go to [firebase.google.com](https://firebase.google.com) → Create project
2. Enable **Realtime Database** → Start in test mode
3. Set database rules for testing:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

4. Share the config object with Sahil → paste into `dashboard/js/firebase.js`
5. Share the database secret with Yiming → paste into `firmware/src/secrets.h`

**Expected data structure written by ESP32 at `leaksense/latest`:**

```json
{
  "ppm_compensated": 145.3,
  "ppm_raw": 145.3,
  "temperature": 23.4,
  "humidity": 48.2,
  "state": "safe",
  "fan": false,
  "buzzer": false,
  "timestamp": 1234567890
}
```

---

## Technology Stack

| Layer | Technology |
|---|---|
| Microcontroller | ESP32-E, Arduino C, PlatformIO |
| Gas Sensor | DFRobot Calibrated I2C Gas Sensor |
| Environmental Sensor | BME280 (I2C) |
| Communication | WiFi, Firebase Realtime Database |
| Dashboard | HTML, CSS, JavaScript, Chart.js |
| Version Control | Git + GitHub |

---

## Important Notes

- `firmware/src/secrets.h` is in `.gitignore` — **never commit WiFi or Firebase credentials**
- Thresholds (300/500 ppm) must match in both `firmware/src/config.h` and `dashboard/js/charts.js`
- Dashboard runs fully with mock data — Firebase only needed for live hardware connection
- Do not write MQ-6 specific firmware — the new DFRobot sensor uses a different library