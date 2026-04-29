# LeakSense – Smart LPG Gas Leakage Detection and Automatic Safety System

## Overview

LeakSense is an IoT-based smart safety system designed to detect LPG gas leakage in real time and automatically respond to dangerous situations.

The system uses an ESP32 microcontroller, MQ-6 I2C LPG gas sensor, BME280 environmental sensor, relay-controlled exhaust fan, buzzer alarm, OLED display, and cloud communication to provide both local and remote monitoring.

This project is developed as part of the CITS5506 Internet of Things unit at The University of Western Australia.

---

## Problem Statement

LPG gas leakage is a serious household safety issue that can lead to fire, explosion, and health hazards.

Traditional gas alarms usually provide only local sound alerts and do not support:
- remote monitoring
- historical logging
- automatic response systems
- cloud integration

LeakSense aims to provide an affordable and intelligent IoT-based solution for early gas leak detection and automated safety response.

---

## Features

- Real-time LPG gas detection
- Three-state safety logic:
  - SAFE
  - WARNING
  - DANGER
- Automatic buzzer activation
- Relay-controlled exhaust fan
- OLED local status display
- Temperature and humidity compensation using BME280
- Cloud communication using WiFi + MQTT/Firebase
- Web dashboard for remote monitoring
- Historical alert logging
- Real-time gas concentration display

---

## Hardware Components

| Component | Purpose |
|---|---|
| FireBeetle ESP32-E | Main IoT controller |
| MQ-6 I2C Gas Sensor | LPG/Butane detection |
| BME280 Sensor | Temperature & humidity compensation |
| OLED SSD1306 Display | Local system monitoring |
| Relay Module | Fan control |
| 5V Exhaust Fan | Ventilation simulation |
| Digital Buzzer | Audible alerts |
| LEDs | Status indication |
| Breadboard & Jumper Wires | Prototyping |

---

## System Architecture

The system contains five major IoT components:

1. Sensors  
   MQ-6 I2C gas sensor and BME280 environmental sensor

2. Computing Node  
   ESP32 microcontroller

3. Communication Technology  
   WiFi + MQTT / Firebase cloud communication

4. Cloud & Dashboard  
   Remote monitoring and historical logging

5. Actions  
   Buzzer alarm, fan activation, visual alerts

---

## Technology Stack

### Embedded System
- ESP32
- Arduino IDE
- C/C++

### Cloud
- Firebase Realtime Database / MQTT

### Dashboard
- HTML
- CSS
- JavaScript

---

## Repository Structure

```bash
LeakSense-IoT/
│
├── firmware/          # ESP32 source code
├── dashboard/         # Web dashboard files
├── docs/              # Proposal, diagrams, references
├── hardware/          # Circuit diagrams and images
├── testing/           # Testing results and logs
└── README.md