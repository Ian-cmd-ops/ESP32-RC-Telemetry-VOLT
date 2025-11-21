RC Telemetry Master Controller (M5AtomS3 Lite)

Version: v0.19.2-BetaRC

Device: M5Stack AtomS3 Lite

This firmware transforms an M5Stack AtomS3 Lite into a high-performance telemetry logger and master controller for RC vehicles. It aggregates data from external sensors (GPS, IMU), reads RC PWM signals, manages audio synthesis, and hosts a real-time web dashboard for visualization and configuration.

🚀 Key Features

Real-Time Web Dashboard: Hosted directly on the ESP32 (WiFi Access Point). View speed, G-forces, battery status, and configure settings.

High-Speed Logging: Records telemetry at 10Hz+ to internal storage.

Supports efficient Binary format or standard CSV.

Smart Sync: Auto-converts binary logs to CSV upon download.

GPS Quality Validation: Tracks HDOP and Satellite count to validate speed runs.

Physics Engine: Estimates drift angle, detects jumps (airtime), and tracks traction loss.

Battery Simulation: accurate power consumption tracking based on motor profiles (Brushless/Brushed) and voltage sag simulation. Includes a calibration mode.

Robust Communication:

Async I2C with CRC validation for sensor data.

LoRa Telemetry support (for long-range ground station).

Crash Recovery: Emergency storage management and corrupted log recovery tools.

🛠️ Hardware Requirements

Master Controller: M5Stack AtomS3 Lite

Secondary Sensor Unit (I2C Address 0x55): Handles GPS & IMU raw data fusion.

Audio Unit (I2C Address 0x56): (Optional) Generates engine sounds based on RPM/Throttle.

LoRa Unit (I2C Address 0x57): (Optional) For long-range telemetry.

Pinout Configuration

Interface

AtomS3 Pin

Function

I2C SDA

G2

Sensor Bus Data

I2C SCL

G1

Sensor Bus Clock

PWM In

G5

RC Receiver Throttle Channel

PWM In

G6

RC Receiver Steering Channel

LED

G35

Status RGB LED

📦 Software Dependencies

Compile using Arduino IDE or PlatformIO. Ensure the following libraries are installed:

M5Unified & M5GFX

Adafruit NeoPixel

AsyncTCP

ESPAsyncWebServer

ArduinoJson (v7.x)

Note: This firmware uses LittleFS for storage. Select a partition scheme with sufficient SPIFFS/LittleFS space (e.g., "No OTA (Large APP), 2MB APP/2MB FS" or similar).

⚡ Installation

Connect Hardware: Wire the Grove I2C sensors and connect RC receiver PWM pins to G5/G6.

Configure IDE: Select board M5Stack AtomS3.

Compile & Upload: Flash the M5AtomS3_Master.ino to the device.

First Boot:

The LED will blink Orange (waiting for sensors) or Green (Ready).

If LittleFS is unformatted, the system will format it automatically (LED may stay red/orange for a few seconds).

🖥️ Usage

1. Connecting to Dashboard

The device creates a WiFi Access Point on boot:

SSID: RC-Telemetry-V0-19-Beta

Password: telemetry123

URL: http://192.168.4.1

2. Controls

Button A (Single Click): Start / Stop Logging.

Button A (Hold 1s): Zero IMU (Calibrate level).

3. LED Status Codes

🟢 Solid Green: System Ready (On-Road Mode).

🟠 Solid Orange: System Ready (Off-Road Mode).

🔴 Solid Red: Logging Active.

🟣 Blinking Magenta: GPS Fix acquired but accuracy is poor (High HDOP).

🟠 Blinking Orange: RC Signal Lost (Throttle/Steering disconnected).

🔴 Blinking Red: I2C Sensor Error.

4. Calibration

Steering: Open the Web Dashboard settings to run the Steering Calibration wizard (Left -> Center -> Right).

Battery: 1. Fully charge battery.
2. Start "Calibration Mode" in Dashboard.
3. Run car until empty.
4. Charge battery and input the "mAh charged" into the Dashboard to calculate the efficiency factor.

📂 Log Management

Logs are saved to internal flash memory.

Access logs via the Storage tab in the Web Dashboard.

Clicking a .bin file will automatically convert it to .csv and download it.

Emergency Cleanup: If storage drops below 100KB, the oldest logs are automatically deleted to preserve the current session.

📝 License

This project is released for educational and hobbyist use.