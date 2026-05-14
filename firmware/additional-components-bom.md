# Additional Components BOM

Project: LeakSense IoT LPG Gas Detection System

Purpose: Extra components requested for GPIO36 analog-input protection and optional signal conditioning. The 5V fan will remain controlled by the relay module, so no external flyback diode is requested for the fan circuit.

| Item | Qty | Suggested part / value | Purpose | Notes |
|---|---:|---|---|---|
| Logic-level N-MOSFET | 1 | AO3400, IRLZ44N, IRLZ34N, IRL540N, or equivalent | Optional low-side switch for future 5V DC load control | Must fully turn on with 3.3V gate drive. |
| Voltage divider upper resistor | 4 | 20k ohm | Protects ESP32 GPIO36 from 5V analog output | Connect from MiCS5524 AO to GPIO36. |
| Voltage divider lower resistor | 4 | 33k ohm | Protects ESP32 GPIO36 from 5V analog output | Connect from GPIO36 to GND. 5V input becomes about 3.11V. |
| Ceramic capacitor | 2 | 100nF | Optional ADC noise filtering / supply decoupling | Can be placed near sensor/module power pins or ADC input if readings are noisy. |
| Electrolytic capacitor | 2 | 100uF, 6.3V or higher | Optional 5V rail smoothing for fan/buzzer startup | Useful if the fan causes voltage dips. |
