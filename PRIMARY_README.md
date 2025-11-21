Here is the comprehensive technical documentation for the **Primary Controller (Master)** of the RC-VOLT system. You can save this as `PRIMARY_README.md` or add it to your project wiki.

---

# RC-VOLT Primary Controller (Master)
**Firmware Version:** v0.19.2-BetaRC
**Hardware Platform:** M5Stack AtomS3 Lite (ESP32-S3)

## 1. System Overview
The **Primary Controller** is the central "brain" of the RC-VOLT telemetry system. It is responsible for orchestrating the entire vehicle network, managing data logging, calculating physics/battery simulation, and hosting the user interface via a WiFi Hotspot.

### Core Responsibilities
* **Bus Master:** Acts as the I2C Master, polling the Secondary unit (GPS/IMU) and sending commands to the Sound Unit.
* **RC Interface:** Reads raw PWM signals from the RC receiver (Throttle & Steering).
* **Physics Engine:** Calculates simulated battery voltage, fuel consumption, g-forces, and drift angles.
* **Data Logger:** Records high-frequency telemetry data to internal flash storage (Binary or CSV).
* **Web Server:** Hosts the `dashboard_html` interface over a dedicated WiFi Access Point.

---

## 2. Hardware Configuration

### Pinout & Connections
The firmware is configured for the **M5Stack AtomS3 Lite**.

| Interface | Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | `G2` | Grove Port | Connects to Secondary, Audio, LoRa |
| **I2C SCL** | `G1` | Grove Port | Connects to Secondary, Audio, LoRa |
| **PWM In 1** | `G5` | Throttle Input | Connect to RC Receiver CH2 |
| **PWM In 2** | `G6` | Steering Input | Connect to RC Receiver CH1 |
| **RGB LED** | `G35` | Status LED | Built-in WS2812 |
| **Button** | `BtnA` | Screen Button | Press to Start/Stop Log |



### System Topology
The Primary unit sits at the top of the I2C hierarchy:
* **Primary (Master):** Polls devices.
* **Address `0x55`:** Secondary Sensor Unit (GPS + IMU).
* **Address `0x56`:** Audio Synthesis Unit.
* **Address `0x57`:** LoRa Telemetry Module (Optional).

---

## 3. Features & Modules

### A. Battery Simulation Engine
Unlike standard voltage sensors, RC-VOLT calculates battery state based on energy consumption to simulate voltage sag and "fuel" remaining.
* **Configurable Cells:** Supports 1S (3.7V) to 6S (22.2V) setups.
* **Motor Modeling:** Simulates current draw based on motor type (Brushed/Brushless), KV/Turns, and throttle curve.
* **Calibration:** Includes a "Calibration Mode" to tune the simulation against real-world charger data.

### B. Data Logging
* **Format:** Configurable between **Binary** (compact, high speed) and **CSV** (human readable).
* **Storage Management:**
    * Auto-rotates files when storage is full.
    * Emergency cleanup if free space drops below 100KB.
    * Files named by timestamp (e.g., `/log_123456.bin`).
* **Auto-Conversion:** The web dashboard automatically converts Binary logs to CSV upon download.

### C. GPS & Physics Validation
* **GPS Quality:** Analyzes satellite count and HDOP to classify signal as `POOR`, `GOOD`, or `EXCELLENT`.
* **Performance Claims:** Flags high-speed runs as "Valid" only if GPS signal quality meets strict thresholds during the run.
* **Drift Detection:** Calculates slip angle and drift status based on IMU yaw rate vs. GPS heading.

### D. Wireless Dashboard
* **SSID:** `RC-Telemetry-V0-19-Beta`
* **Password:** `telemetry123`
* **IP Address:** `192.168.4.1`
* **Protocol:** WebSockets for real-time gauges (~10Hz update rate).

---

## 4. Status LED Codes
The single RGB LED on the AtomS3 indicates system state:

| Color | State | Meaning |
| :--- | :--- | :--- |
| **Green (Solid)** | `NOMINAL` | System idle, all sensors connected. |
| **Green (Blink)** | `NOMINAL` | Off-Road / Dynamic mode active. |
| **Red (Solid)** | `LOGGING` | Data recording is active. |
| **Orange (Blink)** | `ERROR` | I2C Bus Error (Secondary or Audio unit missing). |
| **Purple (Blink)** | `WARN` | RC Link Lost (No PWM signal detected). |
| **Magenta (Blink)** | `WARN` | GPS Signal Poor (Low accuracy). |

---

## 5. Setup & Calibration

### Initial Setup
1.  Flash `M5AtomS3_Master.ino` to the AtomS3 Lite.
2.  Connect the **Grove Cable** to the I2C hub.
3.  Connect **RC Receiver** signal pins to G5/G6 (ensure common ground).
4.  Power on. The LED should turn **Orange** (searching) then **Green** (connected).

### Steering Calibration
1.  Connect to WiFi and open `192.168.4.1`.
2.  Go to **Control Panel** > **Calibrate Steering**.
3.  Follow the prompts: Turn Left, Center, Right.
4.  This maps your RC radio's specific PWM endpoints to degrees (-30° to +30°).

### Battery Calibration
1.  Go to **Battery Config** tab on the dashboard.
2.  Set your **Cell Count** (e.g., 2S) and **Capacity** (e.g., 5000mAh).
3.  Click **Start Calibration** and drive until the battery is low.
4.  Charge the battery and note the mAh put back in by the charger.
5.  Enter this value in the dashboard to auto-calculate the efficiency factor.

---

## 6. File System (LittleFS)
The internal flash is used to store logs and settings.

* `/settings.json`: Stores volume, battery config, and WiFi passwords.
* `/steering_cal.json`: Stores PWM mapping data.
* `/log_*.bin`: Telemetry data files.
* `/health.txt`: Temporary file used to check storage integrity on boot.

**Emergency Recovery:**
If the filesystem corrupts, the firmware will attempt to auto-format on the next boot. To force a format, you can send the serial command: `FORMAT_FS`.

---

## 7. Library Dependencies
Ensure these libraries are installed in Arduino IDE:
1.  `M5Unified`
2.  `Adafruit_NeoPixel`
3.  `ArduinoJson` (v6 or v7)
4.  `ESPAsyncWebServer`
5.  `AsyncTCP`