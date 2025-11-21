

🏎️ RC-VOLT: Telemetry Master Controller

Version: v0.19.2-BetaRC | Device: M5Stack AtomS3 Lite

    ⚠️ AI Disclosure: This firmware was architected by a human and implemented with AI assistance. The logic is pure driver intuition; the syntax is machine-generated. Bugs are possible, drift is guaranteed.

📖 Overview

This firmware transforms an M5Stack AtomS3 Lite into a high-performance telemetry logger and master controller for RC vehicles. Unlike standard loggers, RC-VOLT includes a physics engine to estimate drift angles and detect jumps, plus a battery simulator that tracks voltage sag and consumption in real-time.

🚀 Key Features

    Real-Time Web Dashboard: Host a racing dashboard directly on the ESP32 (Works on any phone/browser).

    Physics Engine: Estimates Drift Angle, detects Airtime/Jumps, and tracks Traction Loss.

    Smart Power: Accurate power consumption tracking with voltage sag simulation and motor profiling.

    High-Speed Logging: 10Hz+ Binary logging (auto-converts to CSV on download).

    GPS Validation: Automatically flags speed runs with low satellite count or high HDOP.

    Crash Recovery: Emergency storage management automatically creates space if the disk fills up during a run.

🛠️ Hardware & Pinout

Master Controller: M5Stack AtomS3 Lite
Interface	Pin	Function
I2C SDA	G2	Sensor Bus Data (GPS/IMU)
I2C SCL	G1	Sensor Bus Clock
PWM In	G5	RC Receiver Throttle
PWM In	G6	RC Receiver Steering
LED	G35	Status Indication

Peripheral Units (I2C):

    0x55: Secondary Sensor Unit (GPS + IMU Fusion)

    0x56: Audio Unit (Engine Sound Synthesis)

    0x57: LoRa Unit (Long-Range Ground Station)

⚡ Quick Start

    Flash: Compile using PlatformIO or Arduino IDE.

        Dependencies: M5Unified, Adafruit NeoPixel, AsyncTCP, ESPAsyncWebServer, ArduinoJson (v7.x).

        Partition Scheme: Large APP (No OTA) (Requires 2MB APP / 2MB FS).

    Wire: Connect your RC Receiver PWM pins to G5/G6 and your sensors to the I2C bus.

    Connect:

        SSID: RC-Telemetry-V0-19-Beta

        Pass: telemetry123

        URL: http://192.168.4.1

🎮 Controls & Status

Button A (Atom Lite Button):

    Single Click: Start / Stop Logging.

    Hold (1s): Zero IMU (Level Calibration).

LED Status:

    🟢 Solid Green: Ready (On-Road Mode)

    🟠 Solid Orange: Ready (Off-Road Mode)

    🔴 Solid Red: LOGGING ACTIVE

    🟣 Blinking Magenta: GPS Fix Poor (High HDOP)

    🟠 Blinking Orange: RC Signal Lost (Failsafe)

🤝 Contributing

This project is "Vibe Coded," meaning we move fast and break things.

    Found a bug? Open an issue with your log file.

    Want to add a feature? PRs are welcome, especially for new vehicle physics profiles.

License: Educational & Hobbyist Use.

