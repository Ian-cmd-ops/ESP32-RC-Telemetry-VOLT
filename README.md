# RemoteControl-VOLT
A real-time (RTOS) telemetry and data logging system for RC cars. Based on M5Atom/ESP32 Hardware
# RC-VOLT (Vehicle Operations & Live Telemetry)
> A real-time (RTOS) telemetry firmware for the M5Stack ecosystem, designed for high-performance RC vehicles.

## ⚠️ Project Status: In-Progress
This is a **work-in-progress** and is currently in the **proof-of-concept validation phase**. It is being tested with a small group of friends before any public release. The code is provided AS-IS, is not production-ready, and has no warranty.

---

## Software Philosophy
This project was born from a preventable failure: losing a race to an ebike after the RC car's 3S LiPo battery died unexpectedly at 10% charge. The root cause was a complete lack of data. **RC-VOLT is the software-first solution to that problem.**

The goal is to create a lightweight, modular, and real-time (RTOS) firmware that provides a comprehensive data acquisition and logging suite for any high-performance RC vehicle.

## Why the M5Stack Atom Ecosystem?
The hardware choice was driven by critical software and performance requirements.

1.  **Mass Reduction:** The previous, discrete ESP32-C3 system was 80-130g. The M5Stack-based architecture is only **33-48g** (a 40-65% weight reduction). In RC, weight is a primary performance killer.
2.  **Integration & Reliability:** The **Atom S3 OLED** integrates the ESP32-S3 and display into a single 8-10g package. This eliminates a major source of wiring failure, I2C bus complexity, and vibration-related issues.
3.  **Distributed Architecture:** The platform's small size and low weight make a multi-unit RTOS architecture feasible, allowing for dedicated microcontrollers for dedicated tasks.
4.  **Expandability:** The Grove connector ecosystem provides a simple, standardized interface for adding future sensors without a rat's nest of wires.

## Current Software Features
The system currently runs a dual-unit RTOS architecture:

* **Primary Unit (M5Stack Atom S3 OLED):**
    * Manages the I2C bus.
    * Polls the **GPS Module** (10Hz parsing).
    * Polls the **MPU6050 IMU** (20Hz sensor sampling).
    * Runs the local OLED display.
    * Serves the WebSocket dashboard.

* **Secondary Unit (M5Stack Atom S3R):**
    * Acts as a dedicated RC input processor.
    * Captures high-resolution throttle and steering PWM signals.

**Core System Features:**
* **"Vibe Coding" Protocol:** A custom, lightweight wireless protocol that synchronizes data between the primary and secondary units in real-time.
* **Live Web Dashboard:** Pushes all sensor data (GPS, IMU, RC Inputs) over WebSocket (5Hz) to any connected browser.
* **Data Logging:** Session-based logging with auto-start (on throttle input) and auto-stop (after 20 seconds of inactivity).
* **CSV Data Export:** Provides a simple way to download a complete, timestamped log of your run for post-session analysis.

## Future Software Roadmap
The current system is a stable proof-of-concept. The next development phase is focused on expanding data integration and telemetry range.

* **Full Powertrain Integration (Priority 1):**
    * Implement high-accuracy **battery voltage and current monitoring** (e.g., INA219/INA226 sensor). This is the key to solving the project's original "10% battery failure" problem.
    * Integrate with ESC bidirectional telemetry (e.g., Hobbywing) to get live RPM, FET temp, and ESC-reported current.

* **Long-Range Telemetry (LoRa):**
    * Integrate LoRa radio modules (e.g., RA-02, EByte) for a robust, long-range telemetry link.
    * Develop a packetized data protocol to transmit key metrics (Speed, Voltage, Current, GPS) from the car to a separate base station or handheld device.

* **Predictive Analysis:**
    * Use the new voltage/current data to add real-time power (Watt) calculations to the dashboard.
    * Develop a predictive runtime algorithm ("Time to 10% battery").

## The Testbed
This firmware is being developed and validated on an exceptionally-engineered 1:7.4 scale, 3D-printed AWD rally car. While RC-VOLT is a standalone software project, you can find the incredible vehicle platform it runs on here:

* **[IMPR3ZA: Tribute/Replica to Subaru Impreza 22B](https://cults3d.com/en/3d-model/game/impr3za-tribute-replica-to-subaru-impreza-22b-full-3d-printed-kit)**

## License
This project is licensed under the MIT License. See the `LICENSE` file for more information.

**Copyright (c) 2025 Ian-cmd-ops**

## Development & Acknowledgments
The core concept, project architecture, failure analysis, problem-solving, and troubleshooting for RC-VOLT were developed by the project author.

In the spirit of transparency, portions of the boilerplate code and specific functions were generated with the assistance of AI pair-programming tools, including Google's Gemini and Anthropic's Claude.
