# 🏎️ RC-VOLT: Advanced RC Telemetry & Physics Engine

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20M5Stack-orange.svg)
![Status](https://img.shields.io/badge/status-Beta%20v0.19.2-green.svg)

**RC-VOLT** is a professional-grade telemetry and data logging system for Radio Control vehicles, running on the **ESP32-S3 (M5Stack AtomS3)** platform.

Unlike standard GPS loggers, RC-VOLT features a **real-time physics engine** that calculates drift angles, detects jumps, estimates tire traction, and simulates battery voltage sag using motor impedance modeling.

## 🚀 Key Features

### 📊 Real-Time Web Dashboard
Host a racing dashboard directly from the RC car via WiFi.
* **Live Gauges:** Speed, G-Force, Throttle/Steering input, and Battery %.
* **WebSocket Technology:** Ultra-low latency updates (~10Hz).
* **Universal:** Works on any smartphone, tablet, or laptop browser.

### 🧮 On-Board Physics Engine
* **Drift Analysis:** Uses IMU fusion to calculate slip angles and yaw rates.
* **Jump Detection:** Identifies airtime using accelerometer free-fall detection.
* **Battery Simulator:** Estimates voltage sag and remaining capacity based on motor current draw profiling (Brushed/Brushless) and calibration factors.

### 🛰️ Precision Data Logging
* **Dual Formats:** Records to optimized **Binary** (for speed) or standard **CSV** (for Excel/Overlay).
* **GPS Validation:** Automatically flags speed runs with "Valid" or "Invalid" based on satellite count and HDOP accuracy.
* **Smart Storage:** Auto-rotates logs and manages flash storage to prevent corruption.

## 🛠️ Hardware Architecture

The system uses two M5Atom communicating over I2C to distribute processing load.

| Unit | Device | Function |
| :--- | :--- | :--- |
| **Primary** | **M5Stack AtomS3 Lite** | WiFi Host, Physics Engine, Logger, RC PWM Input |
| **Secondary** | **M5Stack AtomS3** | GPS NMEA Parsing, IMU Sensor Fusion (MPU6886) |
| **Audio (Optional)** | **M5Stack Atom Echo** | Engine Sound Synthesis (RPM/Turbo sounds) |

### Wiring & Pinout
* **I2C Bus:** Pins `G2` (SDA) / `G1` (SCL)
* **RC Throttle:** Pin `G5` (Connect to Receiver CH2)
* **RC Steering:** Pin `G6` (Connect to Receiver CH1)
* **Status LED:** Pin `G35` (Built-in WS2812)

## 📦 Installation & Setup

### 1. Firmware Flashing
This project is built using **PlatformIO**.
1.  Clone this repository.
2.  Open in VS Code with the PlatformIO extension.
3.  **Important:** Ensure your partition scheme is set to `Large APP (No OTA)` to allow 2MB for the filesystem.
4.  Flash `Master` firmware to the Primary Unit and `Secondary` firmware to the Sensor Unit.

**Dependencies:** `M5Unified`, `Adafruit NeoPixel`, `AsyncTCP`, `ESPAsyncWebServer`, `ArduinoJson (v7.x)`.

### 2. Configuration
Connect to the WiFi Access Point:
* **SSID:** `RC-Telemetry-V0-19-Beta`
* **Pass:** `telemetry123`
* **Dashboard:** Navigate to `http://192.168.4.1`

### 3. Calibration
* **Steering:** Use the web dashboard to map your RC transmitter's PWM endpoints.
* **IMU:** Ensure vehicle is level and hold **Button A** for 1 second to Zero the IMU.
* **Battery:** Use the "Battery Config" tab to set your LiPo cells (2S-6S) and capacity.

## 🎮 Controls & Status

### Button A (Atom Lite)
* **Single Click:** Start / Stop Logging.
* **Hold (1s):** Zero IMU (Level Calibration).

### LED Status Codes
| Color | Pattern | Status | Meaning |
| :--- | :--- | :--- | :--- |
| 🟢 **Green** | Solid | **Ready** | On-Road Mode (Nominal). |
| 🟠 **Orange** | Solid | **Ready** | Off-Road Mode (Nominal). |
| 🔴 **Red** | Solid | **Logging** | Data recording is active. |
| 🟣 **Purple** | Blinking | **RC Error** | No Receiver Signal (Failsafe). |
| 🟠 **Orange** | Blinking | **I2C Error** | Secondary Unit disconnected. |
| 🟣 **Magenta** | Blinking | **GPS Poor** | High HDOP / Low Satellite count. |

## 📂 Documentation
For detailed technical guides, refer to the `docs/` folder:
* [**Primary Unit Guide**](./docs/PRIMARY_README.md) - Master controller architecture.
* [**Secondary Unit Guide**](./docs/SECONDARY_README.md) - GPS & IMU fusion details.
* [**Troubleshooting**](./docs/TROUBLESHOOTING.md) - Full error code reference.
* [**Release Notes**](./RC_VOLT_0.19.1_RELEASE_NOTES.md) - Version history.

## 🤝 Contributing
This project is "Vibe Coded" — we move fast and break things.
* Found a bug? Open an [Issue](https://github.com/Ian-cmd-ops/RemoteControl-VOLT/issues) with your log file.
* Want to add a physics profile? Submit a Pull Request.

## 📄 License
Distributed under the MIT License. See `LICENSE` for more information.

---
*Keywords: ESP32 RC Telemetry, M5Stack AtomS3, Arduino GPS Logger, RC Drift Gyro, Open Source RC, Data Logging, PlatformIO, ESP32-S3, Vehicle Dynamics*

