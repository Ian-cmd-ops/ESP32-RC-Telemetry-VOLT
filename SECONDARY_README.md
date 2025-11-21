Here is the technical documentation for the **Secondary Sensor Unit** of the RC-VOLT system.

-----

# RC-VOLT Secondary Sensor Unit (Slave)

**Firmware Version:** v0.19.2-BetaRC
**Hardware Platform:** M5Stack AtomS3 / Atom Lite
**I2C Address:** `0x55`

## 1\. System Overview

The **Secondary Unit** acts as the "Sensory Cortex" of the vehicle. It offloads intensive sensor processing from the Master controller. Its primary job is to fuse raw GPS NMEA data with high-frequency IMU (Accelerometer/Gyroscope) readings to create a unified telemetry packet.

### Core Responsibilities

  * **I2C Slave:** Responds to polling requests from the Master Unit.
  * **GNSS Processing:** Parses NMEA streams (10Hz) for location, speed, altitude, and accuracy metrics (HDOP).
  * **Inertial Fusion:** Reads internal IMU (MPU6886/SH200Q) to calculate G-Forces, Yaw Rate, and Pitch/Roll.
  * **CRC Generation:** Calculates checksums for all data packets to ensure signal integrity over the wire.

-----

## 2\. Hardware Configuration

### Connections

The Secondary Unit connects to the Master via the Grove I2C bus, and to a GPS Module via UART.

| Interface | Pin | Function | Connection |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | `G2` | Grove Port | Connect to Master Unit |
| **I2C SCL** | `G1` | Grove Port | Connect to Master Unit |
| **UART RX** | `G5` | GPS RX | Connect to GPS TX |
| **UART TX** | `G6` | GPS TX | Connect to GPS RX (Configuration) |
| **IMU** | *Internal* | MPU6886 | Internal I2C Bus |

### Supported Hardware

  * **Controller:** M5Stack AtomS3 or Atom Lite.
  * **GPS Module:** M5Stack GPS Unit (AT6558 / M8N) or BN-880.
      * *Note:* GPS baud rate is typically configured to **38400** or **115200** bps in firmware.

-----

## 3\. I2C Protocol API

The Secondary Unit operates as an I2C Slave at address `0x55`.

### Command Set

The Master sends a 1-byte command to trigger specific actions:

| Command | Hex | Description |
| :--- | :--- | :--- |
| `CMD_GET_TELEMETRY` | `0x30` | Prepares the main telemetry packet for reading. |
| `CMD_ZERO_IMU` | `0x15` | Recalibrates the IMU offsets (vehicle must be stationary). |
| `CMD_TOGGLE_UNITS` | `0x12` | Toggles internal unit calculations (Metric/Imperial). |

### Data Packet Structure

When the Master reads from `0x55`, the Secondary sends a **CompactTelemetryPacket** (Binary, Packed).

**Total Size:** \~48 Bytes (Variable based on struct alignment)

```cpp
struct CompactTelemetryPacket {
  uint8_t header;       // 0xA5 (Sync Byte)
  uint8_t version;      // 0x02 (Protocol Version)
  uint8_t length;       // Payload Length
  
  // --- Payload ---
  float speed;          // Speed in km/h
  float gforce_total;   // Combined G-Force vector
  float lat;            // Latitude (Decimal Degrees)
  float lon;            // Longitude (Decimal Degrees)
  float yaw;            // Yaw Rate (deg/s)
  float maxSpeed;       // Session Max Speed
  float maxG;           // Session Max G
  float gforce_lat;     // Lateral G (Cornering)
  float gforce_lon;     // Longitudinal G (Accel/Brake)
  uint8_t sats;         // Satellite Count
  uint8_t flags;        // Bitmask (GPS Valid, Logging Active, etc)
  uint32_t date;        // DDMMYY
  uint32_t time;        // HHMMSS
  int16_t temp_c_x10;   // Temperature * 10 (e.g. 255 = 25.5°C)
  uint16_t hdop_x10;    // HDOP * 10 (e.g. 12 = 1.2 HDOP)
  
  uint8_t crc;          // CRC-8 Checksum of payload
} __attribute__((packed));
```

-----

## 4\. Sensor Logic & Filters

### GPS Logic

  * **Cold Start:** LED flashes **Blue** while searching for satellites.
  * **Lock:** LED turns **Green** when 3D Fix is acquired (Sats \> 4).
  * **HDOP Filtering:** The system reports HDOP (Horizontal Dilution of Precision). The Master uses this to validate speed runs.
      * HDOP \< 1.5 = Excellent
      * HDOP \> 2.0 = Good
      * HDOP \> 5.0 = Poor

### IMU Logic

  * **G-Force Smoothing:** Raw accelerometer data is passed through a Low Pass Filter (LPF) to remove road vibration noise.
  * **Orientation:** Uses a Mahony or Madgwick filter to determine Pitch and Roll for off-road telemetry.
  * **Zeroing:** When `CMD_ZERO_IMU` is received, the current accelerometer readings are stored as the "flat" reference offset.

-----

## 5\. Status LED Codes

The Atom's LED indicates sensor health:

| Color | Pattern | Meaning |
| :--- | :--- | :--- |
| **Blue** | Blink (1Hz) | **Searching:** No GPS Fix yet. |
| **Green** | Blink (1Hz) | **GPS Lock:** 3D Fix acquired. |
| **Cyan** | Rapid Flash | **I2C Activity:** Data being sent to Master. |
| **Red** | Solid | **Error:** IMU initialization failed. |
| **White** | Flash | **Calibrating:** IMU Zeroing in progress. |

-----

## 6\. Installation Guide

1.  **Mounting:** The unit must be mounted **flat** and **square** to the chassis.
      * *Arrow on AtomS3 should point toward the front of the vehicle.*
2.  **Isolation:** Use double-sided foam tape to isolate the IMU from high-frequency motor vibrations.
3.  **GPS Placement:** Ensure the GPS antenna has a clear view of the sky. Do not mount directly under carbon fiber or metal body panels.
4.  **Wiring:**
      * Connect Grove cable to the Master Unit.
      * Ensure the cable is not strained during suspension travel.

-----

## 7\. Troubleshooting

**Master reports "Sensors: NO"**

  * Check the Grove cable connection.
  * Ensure Secondary unit has power (LED is on).
  * Verify I2C addresses match (`0x55`).

**GPS Sats = 0**

  * Move vehicle outdoors (GPS does not work indoors).
  * Check UART wiring (RX to TX, TX to RX).
  * Verify GPS module baud rate matches firmware (default 38400).

**G-Force reading is high when stopped**

  * Vehicle is not level.
  * Perform **Zero IMU** via the Web Dashboard (Master).