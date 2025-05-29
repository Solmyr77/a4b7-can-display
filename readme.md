# CANBUS Display – Audi A4 B7

## 🚗 Project Goal
A real-time display system for the Audi A4 B7 built on **ESP32 + MCP2515-M**, capable of reading engine-diagnostic data from the CAN bus and handling extra sensors.

---

## 📚 Parts List

| Component | Function |
|-----------|----------|
| **ESP32** | Main controller: CAN read-out, PWM, display driving |
| **MCP2515-M** | CAN controller + TJA1050 transceiver (SPI) |
| **150 Ω resistor** | Voltage divider for the oil-pressure sender |
| **1 kΩ resistor** | Voltage divider for the oil-temperature sender |
| **Step-down module (CR-4030S-05)** | 12 V → 5 V power supply for the ESP32 |
| **Capacitors** | Supply-voltage stabilisation and noise filtering |
| **Piezo buzzer** | Audible warnings |
| **RGB LED** | Visual status indicator |
| **DEPO Racing oil-pressure sender S27** | Additional, non-OEM sensor |
| **DEPO Racing oil-temperature sender S3747 (APD)** | Additional, non-OEM sensor |
| **MSP2807 – ILI9341 2.8″ SPI display** | Real-time data display |

---

## 📊 Available CAN Data (via OBD2, 500 kbps)

### Engine / Drivetrain

- **Turbo boost (MAP)** – ID `0x588`
- **Coolant temperature** – ID `0x288`
- **IAT (Intake-Air Temp)** – ID `0x380`
- **RPM** – ID `0x280`
- **Wheel speed** – ID `0x320`

---

## ✨ Extras (custom sensors)

### Oil Pressure (DEPO 3–160 Ω sender)

- Measured on an ESP32 ADC pin through a voltage divider (150 Ω + sensor)
- Convert resistance → pressure via linear interpolation

### Oil Temperature

- Measured on an ESP32 ADC pin through a voltage divider (1 kΩ + sensor)
- Convert resistance → temperature via linear interpolation

---

## 🎮 Outputs / Indicators

| Indicator        | Operation                                         |
|------------------|---------------------------------------------------|
| **Piezo buzzer** | Critical alerts – oil, temperature, boost, etc.   |
| **RGB LED**      | Shift-light (?) or general status                 |
| **Display**      | Real-time data visualisation                      |

---

## ⚒️ Potential Enhancements

- On-screen menu with paging
- Data logging to SD card
- Wi-Fi dashboard (e.g. phone view)
- Alert/event logging

---

## 📆 Project Status

- [x] Hardware selected  
- [x] CAN ID decoding  
- [x] Pressure & temperature tables loaded  
- [x] Display / LED control  
- [ ] Alert logic  
- [x] In-car testing
