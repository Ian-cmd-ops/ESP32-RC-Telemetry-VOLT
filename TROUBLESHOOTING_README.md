Here is the comprehensive **Troubleshooting Guide** for the RC-VOLT system. This document is designed to help you diagnose hardware connection issues, firmware errors, and calibration problems.

Save this as `TROUBLESHOOTING.md` in your project root.

---

# RC-VOLT System Troubleshooting Guide
**Applicable Firmware:** v0.19.2-BetaRC+
**Hardware:** M5Stack AtomS3 (Master) / AtomS3 or Lite (Slave)

## 1. Quick Diagnostic (LED Codes)
The fastest way to identify an issue is to look at the status LEDs on the Master and Secondary units.

### Master Unit (Primary)
| LED Color | Pattern | Status | Action Required |
| :--- | :--- | :--- | :--- |
| 🟢 **Green** | Solid | **Nominal** | System is ready. No action needed. |
| 🟢 **Green** | Blinking | **Off-Road Mode** | Dynamic mode active. Normal operation. |
| 🔴 **Red** | Solid | **Logging** | Data is being recorded to flash. |
| 🟠 **Orange** | Blinking | **I2C Error** | **Check Grove Cable.** Secondary or Audio unit is offline. |
| 🟣 **Purple** | Blinking | **RC Link Lost** | **Check Receiver.** No PWM signal on pins G5/G6. |
| 🟣 **Magenta**| Blinking | **GPS Poor** | **Wait for Lock.** GPS accuracy is too low for logging. |
| ⚪ **White** | Fast Blink| **Booting** | System initialization. |

### Secondary Unit (Sensor)
| LED Color | Pattern | Status | Action Required |
| :--- | :--- | :--- | :--- |
| 🔵 **Blue** | Blinking | **Searching** | Searching for Satellites. Move outdoors. |
| 🟢 **Green** | Blinking | **GPS Lock** | 3D Fix acquired. System ready. |
| 🔴 **Red** | Solid | **IMU Error** | **Hardware Fail.** MPU6886 init failed. Check power. |

---

## 2. Connectivity Issues (WiFi & Web)

### ❌ Can't find WiFi Network "RC-Telemetry..."
* **Cause:** The ESP32 radio may be off, or the SSID is hidden.
* **Fix:**
    1.  Ensure the unit is powered via USB-C or the 5V/GND header.
    2.  Open the Serial Monitor (`115200` baud). If it prints "AP Started," try a different device (phone/laptop).
    3.  **Reset WiFi:** Send command `AP_RESET` via Serial Monitor to restore default password (`telemetry123`).

### ❌ Dashboard Loads but "Disconnected"
* **Cause:** WebSocket connection blocked or interference.
* **Fix:**
    1.  Ensure you are close to the vehicle (range is ~10-20 meters).
    2.  Turn off mobile data on your phone (some phones prefer LTE over a WiFi with no internet).
    3.  Refresh the page.

---

## 3. Sensor & I2C Bus Issues

### ❌ "Sensor: NO" or "I2C Error" (Orange LED)
* **Cause:** The Master cannot talk to the Secondary Unit (Address `0x55`) or Audio Unit (`0x56`).
* **Fix:**
    1.  **Check Cable:** The Grove cable carries I2C data. Ensure it is clicked in fully on both ends.
    2.  **Check Power:** Does the Secondary unit have a light on? If not, it isn't getting 5V.
    3.  **Bus Recovery:** The system attempts to auto-recover. If it fails, reboot both units simultaneously.
    4.  **Check Stats:** Open Serial Monitor and send `I2C_STATS`. High "CRC Errors" indicate electrical noise or a bad cable.

### ❌ GPS Shows "NO FIX" (0 Sats)
* **Cause:** GPS antenna does not have a clear view of the sky.
* **Fix:**
    1.  **Go Outside:** GPS signals cannot penetrate concrete roofs.
    2.  **Interference:** Do not mount the GPS module directly under the ESC or Motor wires.
    3.  **Cold Start:** A "Cold Start" (first power up in a new location) can take up to 5 minutes.

---

## 4. RC Control & Input Issues

### ❌ Throttle/Steering shows 0% or doesn't move
* **Cause:** No PWM signal detected on pins G5 (Throttle) or G6 (Steering).
* **Fix:**
    1.  **Wiring:** Ensure RC Receiver Signal pins are connected to AtomS3 G5/G6.
    2.  **Ground:** **CRITICAL:** You must have a common ground wire between the RC Receiver and the AtomS3.
    3.  **Power:** Ensure the RC Receiver is powered (usually by the ESC BEC).

### ❌ Steering is reversed or limited
* **Cause:** Calibration does not match your specific radio.
* **Fix:**
    1.  Connect to Dashboard.
    2.  Click **Calibrate Steering**.
    3.  Follow the steps: Full Left -> Center -> Full Right.
    4.  This saves the new PWM endpoints to flash memory.

---

## 5. Battery & Power Issues

### ❌ Voltage reads incorrect (e.g., 11.1V battery reads as 7.4V)
* **Cause:** Wrong Cell Count (S) configured.
* **Fix:**
    1.  Go to **Battery Config** tab on Dashboard.
    2.  Change **Cell Count** to match your battery (e.g., 3S).
    3.  Click **Save Config**.

### ❌ mAh Consumed is inaccurate
* **Cause:** The physics model isn't tuned for your specific motor/efficiency.
* **Fix:**
    1.  Perform a **Battery Calibration** via the dashboard.
    2.  Drive a full pack.
    3.  Input the actual mAh put back in by your charger.
    4.  The system will generate a **Factor** (e.g., 0.85 or 1.15). Save this factor.

---

## 6. Storage & Logging Issues

### ❌ "Logging Failed" or System crashes when logging
* **Cause:** Flash storage is full or corrupted.
* **Fix:**
    1.  **Check Space:** Look at the Storage tab on the dashboard. If >95% full, delete files.
    2.  **Emergency Clear:** Send `deleteAllLogs` via the dashboard or Serial Monitor.
    3.  **Format:** If filesystem is corrupt, hold **Button A** while booting. This enters Safe Mode. Open Serial Monitor and observe. If it fails to mount, it will auto-format.

---

## 7. Advanced Serial Commands
You can troubleshoot via the USB Serial Monitor (115200 baud) using these commands:

| Command | Description |
| :--- | :--- |
| `I2C_STATS` | Shows packet success rates, CRC errors, and timeouts. |
| `VAL_STATS` | Shows how many packets failed physics validation (impossible speeds/Gs). |
| `BATTERY_REPORT` | detailed printout of current battery simulation state. |
| `LORA_TEST` | Forces a LoRa summary packet transmission. |
| `AP_RESET` | Resets WiFi password to default. |
| `RECOVER=/log_X.bin` | Attempts to repair a corrupted binary log file. |

---

## 8. Safe Mode (Recovery)
If you flashed a bad setting (e.g., invalid volume or battery config) that causes a boot loop:

1.  Unplug the Master Unit.
2.  **Hold Button A** (the screen button).
3.  Plug in power while holding the button.
4.  Release after 2 seconds.
5.  **Result:** The system boots *without* loading `settings.json` or `steering_cal.json`. You can now connect and save new, valid settings.