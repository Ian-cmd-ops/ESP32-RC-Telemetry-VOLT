// =================================================================
// M5STACK ATOMS3 LITE - MASTER CONTROLLER v0.19.2-BetaRC
// =================================================================
//
// BETA-READY FIXES:
//    • GPS Quality Dashboard Integration
//    • Temperature Telemetry Validation
//    • Calibration Timeout Protection
//    • Emergency Storage Management
//    • Packet CRC Validation
//    • Auto Binary→CSV Conversion
//    • Battery Calibration Mode
//    • Corrupted Log Recovery
//
// =================================================================

#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <Wire.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <vector>
#include "dashboard_html.h"

// =================================================================
// STORAGE & LOGGING CONFIGURATION (OPTIMIZED)
// =================================================================
#define MAX_LOG_FILES 20              
#define MAX_FILE_SIZE_BYTES 450000    
#define LOG_FORMAT_BINARY true        
#define LOG_SAMPLE_RATE_HZ 10         
#define MIN_FREE_STORAGE_BYTES 100000  // NEW: Emergency threshold

// =================================================================
// GLOBAL MEMORY ALLOCATION
// =================================================================
StaticJsonDocument<4096> wsDataDoc;
StaticJsonDocument<4096> wsLogDoc;
StaticJsonDocument<512> wsCmdDoc;

#define LOG_WRITE_BUFFER_SIZE 4096
char logWriteBuffer[LOG_WRITE_BUFFER_SIZE];
size_t logWriteBufferOffset = 0;
const unsigned long LOG_FLUSH_INTERVAL_STABLE = 10000;
unsigned long lastLogFlush_STABLE = 0;

// =================================================================
// CIRCULAR LOG BUFFER GLOBALS
// =================================================================
#define LOG_BUFFER_SIZE 50
String logBuffer[LOG_BUFFER_SIZE];
int logBufferIndex = 0;

File logFile;
size_t currentFileSize = 0;
bool logFileOpen = false;

unsigned long lastSoundUpdate = 0;
const unsigned long SOUND_UPDATE_INTERVAL = 50; 
const unsigned long FORCE_AUDIO_UPDATE_INTERVAL = 2000;

// =================================================================
// FIRMWARE CONFIGURATION
// =================================================================
#define FW_VERSION "v0.19.2-BetaRC" 
#define STEERING_CAL_FILE "/steering_cal.json"
#define SETTINGS_FILE "/settings.json"

// =================================================================
// HARDWARE PIN DEFINITIONS
// =================================================================
const int GROVE_SDA_PIN = 2;
const int GROVE_SCL_PIN = 1;
const int THROTTLE_PIN = 5;
const int STEERING_PIN = 6;
const int LED_PIN = 35;
const int NUM_LEDS = 1;

// =================================================================
// STATUS LED CONFIGURATION
// =================================================================
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
unsigned long lastBlinkTime = 0;
bool ledState = false;

#define COLOR_LOGGING     0xFF0000  
#define COLOR_NOMINAL_ON  0x00FF00  
#define COLOR_NOMINAL_OFF 0xFF8000  
#define COLOR_WARNING_RC  0x800080  
#define COLOR_ERROR_I2C   0xFF4500  
#define COLOR_GPS_POOR    0xFF0080  // NEW: Magenta for poor GPS
#define COLOR_OFF         0x000000  

// =================================================================
// NETWORK CONFIGURATION
// =================================================================
const char* WIFI_SSID = "RC-Telemetry-V0-19-Beta";
String wifiPass = "telemetry123"; 
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =================================================================
// I2C DEVICE ADDRESSES & PROTOCOL
// =================================================================
#define ADDR_SECONDARY 0x55  
#define ADDR_SOUND     0x56  
#define ADDR_LORA      0x57  
#define I2C_FREQUENCY  100000
#define I2C_TIMEOUT    500
#define I2C_RETRY      3

// =================================================================
// NEW: ROBUST PACKET PROTOCOL WITH CRC
// =================================================================
#define PACKET_HEADER 0xA5
#define PACKET_VERSION 0x02

// CRC-8 lookup table
static const uint8_t crc8_table[256] PROGMEM = {
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
  0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
  0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
  0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
  0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
  0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
  0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
  0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
  0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
  0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
  0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
  0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
  0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
  0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
  0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
  0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
  0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc = pgm_read_byte(&crc8_table[crc ^ data[i]]);
  }
  return crc;
}

// =================================================================
// OPTIMIZED LOGGING STRUCTURES (BINARY)
// =================================================================

struct LogRecord {
  uint32_t timestamp;           
  float speed;                  
  float lat;                    
  float lon;                    
  int16_t gforce_total_x100;    
  int16_t gforce_lat_x100;      
  int16_t gforce_lon_x100;      
  int16_t yaw_x10;              
  uint8_t sats;                 
  uint8_t hdop_x10;             
  uint8_t flags;                
  int16_t temp_c_x10;           
  uint16_t volts_x100;          
  uint16_t mah_used;            
} __attribute__((packed));      

inline int16_t floatToInt16x100(float val) { 
  return (int16_t)(constrain(val * 100.0f, -32767, 32767)); 
}

inline int16_t floatToInt16x10(float val) { 
  return (int16_t)(constrain(val * 10.0f, -32767, 32767)); 
}

inline uint16_t floatToUInt16x100(float val) { 
  return (uint16_t)(constrain(val * 100.0f, 0, 65535)); 
}

inline uint8_t floatToUInt8x10(float val) { 
  return (uint8_t)(constrain(val * 10.0f, 0, 255)); 
}

// =================================================================
// LORA TELEMETRY PROTOCOL
// =================================================================
#define LORA_LIVE_INTERVAL 1000  

struct LoRaLivePacket {
  uint8_t type = 0x01;
  int32_t lat_scaled;       
  int32_t lon_scaled;       
  uint8_t speed_kph;        
  uint8_t sats;             
  uint8_t status_flags;     
} __attribute__((packed));

struct LoRaSummaryPacket {
  uint8_t type = 0x02;
  uint16_t max_speed_x10;   
  uint16_t max_g_x100;      
  uint32_t run_time_ms;     
  int32_t lat_end;          
  int32_t lon_end;          
} __attribute__((packed));

unsigned long lastLoRaUpdate = 0;
unsigned long sessionStartTime = 0;

// =================================================================
// NEW: ROBUST TELEMETRY PACKET WITH CRC
// =================================================================
struct CompactTelemetryPacket {
  uint8_t header;           // 0xA5
  uint8_t version;          // 0x02
  uint8_t length;           // sizeof(payload)
  
  // Payload
  float speed;
  float gforce_total;
  float lat;
  float lon;
  float yaw;
  float maxSpeed;
  float maxG;
  float gforce_lat;
  float gforce_lon;
  uint8_t sats;
  uint8_t flags;            
  uint32_t date;
  uint32_t time;
  int16_t temp_c_x10;       
  uint16_t hdop_x10;        // NEW: Add HDOP
  
  uint8_t crc;              // CRC-8 of payload
} __attribute__((packed));

struct TelemetryData {
  float latitude = 0.0;
  float longitude = 0.0;
  float speed_kph = 0.0;
  float g_force_total = 0.0;
  float g_force_lateral = 0.0;
  float g_force_longitudinal = 0.0;
  float yaw_rate = 0.0;
  float max_speed = 0.0;
  float max_g_force = 0.0;
  uint8_t satellites = 0;
  uint16_t hdop = 255;      // Changed to uint16_t
  uint16_t heading = 0;
  int16_t altitude = 0;
  bool gps_valid = false;
  bool imu_connected = false;
  uint32_t date = 0;
  uint32_t time = 0;
  int gps_quality = 0; 
  float temp_c = 0.0;
  bool temp_valid = false;  // NEW: Temperature validity flag
};
TelemetryData sensorData;

unsigned long lastAmbientTempUpdate = 0;
bool manualTempOverride = false;  // NEW: Track manual override

// =================================================================
// GPS QUALITY VALIDATION SYSTEM
// =================================================================
enum GPSQuality { 
  GPS_NO_FIX,       
  GPS_POOR,         
  GPS_ACCEPTABLE,   
  GPS_GOOD,         
  GPS_EXCELLENT     
};

struct GPSValidation {
  GPSQuality quality = GPS_NO_FIX;
  bool validForPerformanceClaims = false;
  uint32_t excellentQualityDuration_ms = 0;
  uint32_t lastExcellentTimestamp = 0;
  float maxSpeedDuringExcellentGPS_kph = 0.0f;
  uint8_t satellitesAtMaxSpeed = 0;
  uint16_t hdopAtMaxSpeed = 0;
  uint32_t totalSamplesExcellent = 0;
  uint32_t totalSamplesGood = 0;
  uint32_t totalSamplesAcceptable = 0;
  uint32_t totalSamplesPoor = 0;
  
  void reset() {
    quality = GPS_NO_FIX;
    validForPerformanceClaims = false;
    maxSpeedDuringExcellentGPS_kph = 0.0f;
  }
  
  float getExcellentPercentage() const {
    uint32_t total = totalSamplesExcellent + totalSamplesGood; 
    if(total == 0) return 0.0f;
    return (float)totalSamplesExcellent / (float)total * 100.0f;
  }
};
GPSValidation gpsValidation;

struct SensorLimits {
  static constexpr float MAX_SPEED_KPH = 150.0f; 
  static constexpr float MIN_SPEED_KPH = -5.0f;
  static constexpr float MAX_G_FORCE = 10.0f;
  static constexpr float MIN_G_FORCE = -10.0f;
  static constexpr float MAX_LATITUDE = 90.0f;
  static constexpr float MIN_LATITUDE = -90.0f;
  static constexpr float MAX_LONGITUDE = 180.0f;
  static constexpr float MIN_LONGITUDE = -180.0f;
  static constexpr int32_t MAX_ALTITUDE = 10000;
  static constexpr int32_t MIN_ALTITUDE = -500;
  static constexpr uint16_t MAX_HDOP = 2000;
  static constexpr uint8_t MAX_SATELLITES = 32;
};

struct ValidationStats {
  uint32_t totalValidations = 0;
  uint32_t speedViolations = 0;
  uint32_t gForceViolations = 0;
  uint32_t gpsCoordViolations = 0;
  uint32_t crcFailures = 0;         // NEW
  uint32_t headerFailures = 0;      // NEW
  
  uint32_t getTotalViolations() const {
    return speedViolations + gForceViolations + gpsCoordViolations + crcFailures + headerFailures;
  }
  
  float getValidationRate() const {
    if(totalValidations == 0) return 100.0f;
    return (float)(totalValidations - getTotalViolations()) / 
           (float)totalValidations * 100.0f;
  }
};
ValidationStats validationStats;

// =================================================================
// NON-BLOCKING I2C STATE MACHINE
// =================================================================
enum I2CReadState { 
  I2C_IDLE, 
  I2C_REQUEST_SENT, 
  I2C_AWAITING_DATA, 
  I2C_PROCESSING 
};

struct I2CAsyncContext {
  I2CReadState state = I2C_IDLE;
  uint32_t requestTime_us = 0;
  uint8_t retryCount = 0;
  bool dataReady = false;
  CompactTelemetryPacket tempPacket;  // Changed to packet
  
  static constexpr uint32_t SEC_RESPONSE_TIME_US = 2000;  
  static constexpr uint32_t TIMEOUT_US = 200000;            
  static constexpr uint8_t MAX_RETRIES = 3;
};
I2CAsyncContext i2cContext;

struct I2CStatistics {
  uint32_t totalAttempts = 0;
  uint32_t successfulReads = 0;
  uint32_t timeouts = 0;
  uint32_t partialReads = 0;
  uint32_t crcErrors = 0;
  
  void recordAttempt() { totalAttempts++; }
  void recordSuccess() { successfulReads++; }
  void recordTimeout() { timeouts++; }
  void recordPartialRead() { partialReads++; }
  void recordCrcError() { crcErrors++; }
  
  float getSuccessRate() const {
    if(totalAttempts == 0) return 100.0f;
    return (float)successfulReads / (float)totalAttempts * 100.0f;
  }
};
I2CStatistics i2cStats;

// =================================================================
// NEW: BATTERY CALIBRATION MODE
// =================================================================
enum BatteryCalMode {
  CAL_DISABLED = 0,
  CAL_RECORDING = 1,
  CAL_COMPLETE = 2
};

struct BatteryCalibration {
  BatteryCalMode mode = CAL_DISABLED;
  unsigned long startTime = 0;
  float startVoltage = 0.0;
  float predicted_mah = 0.0;
  float actual_mah = 0.0;      // User enters from charger
  float calculatedFactor = 1.0;
  
  void startCalibration(float voltage) {
    mode = CAL_RECORDING;
    startTime = millis();
    startVoltage = voltage;
    predicted_mah = 0.0;
    actual_mah = 0.0;
  }
  
  void finishCalibration(float predicted) {
    mode = CAL_COMPLETE;
    predicted_mah = predicted;
  }
  
  bool calculateFactor(float charger_reading) {
    if (predicted_mah < 100.0) return false;  // Too short run
    actual_mah = charger_reading;
    calculatedFactor = actual_mah / predicted_mah;
    return true;
  }
};
BatteryCalibration batteryCalibration;

// =================================================================
// BATTERY SIMULATION ENGINE
// =================================================================
enum MotorType { 
  MOTOR_BRUSHLESS = 0,  
  MOTOR_BRUSHED = 1      
};

struct BatteryState {
  float consumed_mah = 0.0;
  float current_amps = 0.0;
  float voltage_sim = 12.6;
  uint8_t percentage = 100;
  unsigned long last_calc_time = 0;
   
  uint16_t capacity_mah = 5000;
  uint8_t cells = 2; // NEW: Default to 2S (7.4V nominal)
   
  int motor_type = MOTOR_BRUSHLESS;
  float motor_turns = 13.5;         
  int esc_rating_amps = 60;
  float peak_amps = 60.0;
  float idle_amps = 0.5;            
  
  float static_load_amps = 0.40;
  float calibration_factor = 1.0;
};
BatteryState battery;

// =================================================================
// SYSTEM TIMING CONSTANTS
// =================================================================
const unsigned long SENSOR_READ_INTERVAL = 50;
const unsigned long WEBSOCKET_INTERVAL = 100;
const unsigned long LOG_INTERVAL = 100;
const unsigned long GPS_HIGH_SPEED_INTERVAL = 100;
const unsigned long RC_TIMEOUT = 2000;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;
const unsigned long CLIENT_PING_INTERVAL = 1000;
const unsigned long AUDIO_SCAN_INTERVAL = 30000;
const unsigned long MODE_CHANGE_HYSTERESIS = 5000;
const unsigned long CALIBRATION_TIMEOUT = 30000;  // NEW: 30s timeout
#define WDT_TIMEOUT 10000

// =================================================================
// AUDIO CONTROL
// =================================================================
uint8_t masterVolume = 80;
uint8_t engineVolume = 80;
uint8_t turboVolume = 50;
uint8_t mechanicalVolume = 30;
bool audioMuted = false;

int16_t user_lpf_override = 0;
int16_t user_hpf_override = 0;
int16_t user_distortion_k = 0;

// =================================================================
// WIFI & CLIENT MANAGEMENT
// =================================================================
unsigned long lastWifiCheck = 0;
unsigned long lastClientPing = 0;
int connectedClients = 0;

// =================================================================
// SYSTEM STATE
// =================================================================
bool sensorConnected = false;
bool soundUnitConnected = false;
bool loraConnected = false;  
bool sessionActive = false;
bool isLogging = false;
unsigned long sensorLastSeen = 0;
unsigned long soundLastSeen = 0;
unsigned long loraLastSeen = 0; 
unsigned long lastAudioScan = 0;
String currentUnits = "metric";
String logFileName = "";
unsigned long logEntryCount = 0;
float prevSpeed = 0.0;
int prevThrottle = 0;
float prevMaxSpeed = 0.0;
bool testMode = false;
unsigned long testModeStartTime = 0;
int simulatedThrottle = 0;
const unsigned long TEST_MODE_DURATION = 20000;

// =================================================================
// ATOMIC PWM INPUT STATE (ISR-Safe)
// =================================================================
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

volatile unsigned long throttlePwmRiseTime = 0;
volatile unsigned long throttlePwmPulseWidth = 1500;
volatile unsigned long throttlePwmFallTime = 0;  // NEW: For signal validation
volatile bool throttlePwmNewData = false;
int throttlePercent = 0;
bool throttleConnected = false;
unsigned long lastThrottleSignal = 0;

volatile unsigned long steeringPwmRiseTime = 0;
volatile unsigned long steeringPwmPulseWidth = 1500;
volatile unsigned long steeringPwmFallTime = 0;  // NEW: For signal validation
volatile bool steeringPwmNewData = false;
float steeringAngleDeg = 0.0;
bool steeringConnected = false;
unsigned long lastSteeringSignal = 0;

unsigned long readThrottlePwmSafe() {
  portENTER_CRITICAL(&mux);
  unsigned long value = throttlePwmPulseWidth;
  portEXIT_CRITICAL(&mux);
  return value;
}

unsigned long readSteeringPwmSafe() {
  portENTER_CRITICAL(&mux);
  unsigned long value = steeringPwmPulseWidth;
  portEXIT_CRITICAL(&mux);
  return value;
}

// =================================================================
// RC CALIBRATION SYSTEM
// =================================================================
const unsigned long RC_CENTER_DEFAULT = 1500;
const unsigned long RC_DEADBAND = 30;
unsigned long throttleMin = 1100;
unsigned long throttleMax = 1900;
unsigned long calibrationStartTime = 0;
bool calibrationMode = true;

unsigned long steeringMinPwm = 1100;
unsigned long steeringMaxPwm = 1900;
unsigned long steeringCenterPwm = 1500;
float steeringAngleMin = -30.0;
float steeringAngleCenter = 0.0;
float steeringAngleMax = 30.0;
bool steeringReversed = false;

enum CalState { IDLE, WAIT_LEFT, WAIT_CENTER, WAIT_RIGHT };
CalState steeringCalState = IDLE;
unsigned long tempCalMinPwm, tempCalCenterPwm, tempCalMaxPwm;
float tempCalAngleMin, tempCalAngleCenter, tempCalAngleMax;
unsigned long calStateStartTime = 0;  // NEW: Timeout tracking

#define PWM_MIN_VALID 800
#define PWM_MAX_VALID 2200
#define PWM_DEBOUNCE_TIME 500

// =================================================================
// DRIVING MODE SYSTEM
// =================================================================
enum DrivingMode { 
  MODE_ON_ROAD = 0,   
  MODE_OFF_ROAD = 1,  
  MODE_AUTO = 2       
};

DrivingMode currentMode = MODE_AUTO;
DrivingMode detectedMode = MODE_OFF_ROAD;
bool modeAutoDetect = true;
unsigned long lastModeChange = 0;
unsigned long modeChangeRequestTime = 0;

struct ModeThresholds {
  float jump_takeoff_g; 
  float jump_landing_g; 
  float jump_min_speed;
  float drift_slip_angle; 
  float drift_min_speed;
  float surface_rough_threshold; 
  float corner_yaw_threshold;
};

ModeThresholds onRoadThresholds = { 
  0.3, 2.0, 15.0,  
  10.0, 20.0,      
  0.15, 60.0       
};

ModeThresholds offRoadThresholds = { 
  0.5, 1.8, 5.0,   
  15.0, 10.0,      
  0.3, 40.0        
};

ModeThresholds* activeThresholds = &offRoadThresholds;

// =================================================================
// OFF-ROAD METRICS
// =================================================================
struct OffRoadMetrics {
  float roll_deg = 0.0;
  float pitch_deg = 0.0;
  float yaw_deg = 0.0;
  float yaw_rate_deg_s = 0.0;
  
  float slip_angle_deg = 0.0;
  bool is_drifting = false;
  float drift_angle_max = 0.0;
  unsigned long drift_duration_ms = 0;
  unsigned long drift_start = 0;
  
  bool is_airborne = false;
  unsigned long airtime_ms = 0;
  unsigned long airtime_max_ms = 0;
  unsigned long last_takeoff = 0;
  float landing_g_force = 0.0;
  float landing_g_max = 0.0;
  int jump_count = 0;
  
  enum SurfaceType { 
    SURFACE_TARMAC = 0, 
    SURFACE_GRAVEL = 1, 
    SURFACE_DIRT = 2, 
    SURFACE_ROUGH = 3, 
    SURFACE_UNKNOWN = 4 
  };
  SurfaceType surface = SURFACE_UNKNOWN;
  float terrain_roughness = 0.0;
  
  float traction_loss_pct = 0.0;
  float front_traction = 100.0;
  float rear_traction = 100.0;
  bool awd_active = false;
  
  float suspension_travel_front_mm = 0.0;
  float suspension_travel_rear_mm = 0.0;
  float suspension_compression_pct = 0.0;
  
  unsigned long stage_start_time = 0;
  unsigned long stage_elapsed_ms = 0;
  bool stage_active = false;
  int corner_count = 0;
  
  float max_roll_deg = 0.0;
  float max_pitch_deg = 0.0;
  float max_yaw_rate = 0.0;
  float max_lateral_g = 0.0;
  float max_longitudinal_g = 0.0;
};
OffRoadMetrics offroad;

// =================================================================
// ON-ROAD METRICS
// =================================================================
struct OnRoadMetrics {
  unsigned long lap_start_time = 0;
  unsigned long lap_elapsed_ms = 0;
  unsigned long best_lap_ms = 0;
  bool lap_active = false;
  int lap_count = 0;
  
  unsigned long sector_times[3] = {0, 0, 0};
  unsigned long best_sectors[3] = {0, 0, 0};
  int current_sector = 0;
  
  float avg_corner_speed = 0.0;
  float max_corner_speed = 0.0;
  float corner_g_force_avg = 0.0;
  int corner_count = 0;
  
  float max_brake_g = 0.0;
  float avg_brake_g = 0.0;
  int brake_count = 0;
  float max_accel_g = 0.0;
  float avg_accel_g = 0.0;
  int accel_count = 0;
  
  float consistency_index = 0.0;
};
OnRoadMetrics onroad;

float roll_angle = 0.0;
float pitch_angle = 0.0;
float yaw_angle = 0.0;
unsigned long last_imu_update = 0;
float accel_history[10] = {0};
int accel_history_idx = 0;
float last_yaw_rate = 0.0;
unsigned long last_corner_time = 0;

// =================================================================
// I2C COMMAND DEFINITIONS
// =================================================================
enum TelemetryCmd {
  CMD_START_LOG_TEL = 0x10,
  CMD_STOP_LOG_TEL = 0x11,
  CMD_TOGGLE_UNITS = 0x12,
  CMD_RESET_SESSION = 0x13,
  CMD_CLEAR_MAX = 0x14,
  CMD_ZERO_IMU = 0x15,
  CMD_GET_TELEMETRY = 0x30
};

// =================================================================
// AUDIO DATA PACKET PROTOCOL
// =================================================================
#define AUDIO_PACKET_CMD 0xA0

struct AudioDataPacket {
  int16_t rpm;
  int16_t throttle_percent;
  int16_t speed_kph_x10;
  int16_t g_force_lon_x100;
  int16_t boost_psi_x100;
  uint8_t gear;
  uint8_t master_volume;
  uint8_t engine_volume;
  uint8_t turbo_volume;
  uint8_t mech_volume;
  uint8_t muted;
  int16_t lpf_override_hz;
  int16_t hpf_override_hz;
  int16_t distortion_override_k;
  uint8_t event_triggers;
} __attribute__((packed));

#define EVT_TRIGGER_START   (1 << 0)
#define EVT_TRIGGER_STOP    (1 << 1)
#define EVT_TRIGGER_MAX     (1 << 2)
#define EVT_TRIGGER_SHIFT   (1 << 3)
#define EVT_TRIGGER_JUMP    (1 << 4)
#define EVT_TRIGGER_CRACKLE (1 << 5)

enum SoundCmd { 
  SND_EVENT_START = 0x30, 
  SND_EVENT_STOP = 0x31 
};

static uint8_t one_shot_event_mask = 0;

// =================================================================
// FUNCTION PROTOTYPES
// =================================================================
void initializeI2C();
void recoverI2CBus();
void scanI2CBus();
void setupWebServer();
void checkFS(); 
void updateSystemStatus();
void updateRCInputs();
void updateSoundUnit();
void updateBatterySimulation();
void printBatteryReport();
void updateWiFiStatus();
void updateLedStatus();
void scanForAudioUnit();
void updateOffRoadTelemetry();
void updateOnRoadTelemetry();
void autoDetectMode();
void switchMode(DrivingMode newMode);
const char* getModeString(DrivingMode mode);
const char* getSurfaceString(OffRoadMetrics::SurfaceType surface);
void updateOrientationAndFusion(float dt);
void estimateSlipAngle();
void detectJump();
void detectSurface();
void estimateAWDTraction();
void estimateSuspensionTravel();
void detectCorners();
void updateRallyStage();
float calculateSteeringAngle(unsigned long pwmValue);
void handleButtonInput();
void handleSerialCommands();
void processSteeringCalibration(unsigned long currentPwm); 
void IRAM_ATTR throttleISR();
void IRAM_ATTR steeringISR();
bool sendI2CCommand(uint8_t address, uint8_t command);
bool sendSoundDataPacket(const AudioDataPacket& packet);
void unpackTelemetryData(const CompactTelemetryPacket& packet);
bool updateSensorDataAsync(); 
void assessGPSQuality(); 
bool validateTelemetryPacket(const CompactTelemetryPacket& packet);
const char* getGPSQualityString(GPSQuality quality); 
void printGPSValidationSummary(); 
void onWebSocketEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);
void handleWebCommand(const char* json);
void sendWebSocketData();
void sendNotification(const char* status, const String& message);
void sendCalStep(String message, String color, bool done = false);
void startNewLog();
void stopLog();
void cleanupOldLogs();
void deleteAllLogFiles(); 
void logData();
void logDataBinary();
void flushLogBuffer();
void deleteFile(const String& filename);
void saveSteeringCalibration(unsigned long, unsigned long, unsigned long, float, float, float, bool);
void loadSteeringCalibration();
void saveSettings();
void loadSettings();
void triggerSoundEvent(uint8_t event_bit);
void emergencyFlushHandler();
void updateLoRaTelemetry(); 
void sendLoRaSummary();     
String getLatestLogFile();
void convertBinaryToCSV(String binFilename);
void recoverCorruptedLog(String filename);
// Added missing prototypes
void addToLogBuffer(const String& message);
void log_m(const char* level, const String& message);
size_t getFreeStorage();
int countLogFiles();
String getOldestLogFile();
bool deleteOldestLog();
void closeCurrentLog();
void checkStorageAndRotate();

// =================================================================
// FILE MANAGEMENT & LOGGING IMPLEMENTATION
// =================================================================

void addToLogBuffer(const String& message) {
  logBuffer[logBufferIndex] = message;
  logBufferIndex = (logBufferIndex + 1) % LOG_BUFFER_SIZE;
}

void log_m(const char* level, const String& message) {
  String logLine = "[" + String(level) + "][" + String(millis()) + "ms] " + message;
  Serial.println(logLine);
  addToLogBuffer(logLine);
}

size_t getFreeStorage() {
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

int countLogFiles() {
  int count = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    String name = String(file.name());
    if (name.startsWith("/log_") && 
        (name.endsWith(".bin") || name.endsWith(".csv"))) {
      count++;
    }
    file = root.openNextFile();
  }
  return count;
}

String getOldestLogFile() {
  String oldest = "";
  uint32_t oldestID = 0xFFFFFFFF;
  
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    String name = String(file.name());
    if (name.startsWith("/log_")) {
      int startIdx = name.indexOf("_") + 1;
      int endIdx = name.indexOf(".");
      if (startIdx > 0 && endIdx > startIdx) {
        if (oldest == "" || name < oldest) {
            oldest = name;
        }
      }
    }
    file = root.openNextFile();
  }
  return oldest;
}

bool deleteOldestLog() {
  String oldest = getOldestLogFile();
  if (oldest.length() > 0) {
    log_m("FILE", "Deleting oldest: " + oldest);
    return LittleFS.remove(oldest);
  }
  return false;
}

void closeCurrentLog() {
  if (logFileOpen && logFile) {
    if (!LOG_FORMAT_BINARY && logWriteBufferOffset > 0) {
        logFile.write((uint8_t*)logWriteBuffer, logWriteBufferOffset);
        logFile.flush();
        logWriteBufferOffset = 0;
    }
    logFile.close();
    logFileOpen = false;
    log_m("FILE", "Closed log file, size: " + String(currentFileSize));
  }
}

void checkStorageAndRotate() {
  size_t freeSpace = getFreeStorage();
  
  // CRITICAL: Emergency cleanup if <100KB free
  if (freeSpace < MIN_FREE_STORAGE_BYTES) {
    log_m("STORAGE", "⚠️ CRITICAL: <100KB free, emergency cleanup!");
    
    // Delete oldest files aggressively until we have breathing room
    int deletedCount = 0;
    while (freeSpace < (MIN_FREE_STORAGE_BYTES * 2) && deletedCount < 5) {
      if (deleteOldestLog()) {
        deletedCount++;
        freeSpace = getFreeStorage();
      } else {
        break;
      }
    }
    
    if (deletedCount > 0) {
      log_m("STORAGE", "Emergency deleted " + String(deletedCount) + " files");
      sendNotification("warning", "Low storage - deleted " + String(deletedCount) + " old logs");
    }
  }
  
  // Normal rotation by file count
  int fileCount = countLogFiles();
  while (fileCount >= MAX_LOG_FILES) {
    if (deleteOldestLog()) {
      fileCount--;
      log_m("FILE", "Deleted old file, count now: " + String(fileCount));
    } else {
      break;
    }
  }
  
  // Check current file size
  if (logFileOpen && currentFileSize > MAX_FILE_SIZE_BYTES) {
    log_m("FILE", "Current file size limit reached, rotating...");
    closeCurrentLog();
    delay(100);
  }
}

// =================================================================
// SETUP
// =================================================================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  Wire.end(); 
  delay(10); 
  
  pinMode(GROVE_SDA_PIN, OPEN_DRAIN); 
  pinMode(GROVE_SCL_PIN, OUTPUT);     
  digitalWrite(GROVE_SCL_PIN, HIGH);  
  digitalWrite(GROVE_SDA_PIN, HIGH);  
  delay(10); 
  
  Serial.begin(115200);
  
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  
  delay(500);
  
  log_m("INFO", "============================================");
  log_m("INFO", "AtomS3 Lite Master " FW_VERSION);
  log_m("INFO", "============================================");

  strip.begin();
  strip.setBrightness(10);
  strip.setPixelColor(0, COLOR_ERROR_I2C);
  strip.show();

  bool safeMode = M5.BtnA.isPressed();
  if (safeMode) {
    log_m("WARN", "!!! SAFE MODE BOOT - Settings not loaded !!!");
  }

  pinMode(THROTTLE_PIN, INPUT_PULLUP);
  pinMode(STEERING_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_PIN), throttleISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(STEERING_PIN), steeringISR, CHANGE);

  initializeI2C(); 
  scanI2CBus();
  
  delay(3000);
  
  log_m("INFO", "Waiting 5s for audio unit boot...");
  for (int i = 5; i > 0; i--) {
    log_m("WAIT", String(i) + "s");
    delay(1000);
  }
  
  scanI2CBus();
  delay(500);
  scanForAudioUnit();
  lastAudioScan = millis();

  if (!LittleFS.begin(true)) {
    log_m("ERROR", "LittleFS Mount Failed! Formatting...");
    LittleFS.format();
    if(!LittleFS.begin(true)) {
        log_m("FATAL", "LITTLEFS FAILED. RESTARTING.");
        ESP.restart();
    }
  }

  log_m("INFO", "LittleFS Mounted Successfully.");
  
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  log_m("STORAGE", "Total: " + String(total) + ", Used: " + String(used) + ", Free: " + String(total - used));
  
  int fileCount = countLogFiles();
  log_m("STORAGE", "Found " + String(fileCount) + " log files");
  while (fileCount >= MAX_LOG_FILES) {
    deleteOldestLog();
    fileCount--;
  }

  checkFS();
  
  if (!safeMode) {
    loadSettings();
    loadSteeringCalibration();
  } else {
    saveSettings();
    saveSteeringCalibration(steeringMinPwm, steeringCenterPwm, steeringMaxPwm, 
                            steeringAngleMin, steeringAngleCenter, steeringAngleMax, 
                            steeringReversed);
  }
  
  WiFi.softAP(WIFI_SSID, wifiPass.c_str());
  log_m("WIFI", "AP " + WiFi.softAPIP().toString());

  setupWebServer();
  
  calibrationStartTime = millis();
  switchMode(currentMode);
  
  log_m("INFO", "Initializing Shutdown Handler");
  esp_register_shutdown_handler(emergencyFlushHandler);

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL); 
  
  log_m("INFO", "BETA READY - v0.19.2 Validation Mode");
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
  esp_task_wdt_reset();

  static unsigned long lastStackCheck = 0;
  if (millis() - lastStackCheck > 10000) {
    UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(NULL);
    if (stackLeft < 512) {
      Serial.print(F("[ERROR] Low stack: "));
      Serial.print(stackLeft);
      Serial.println(F(" bytes remaining"));
    }
    lastStackCheck = millis();
  }
  
  M5.update();
  updateSystemStatus();
  updateRCInputs();
  updateBatterySimulation();
  updateWiFiStatus();
  handleButtonInput();
  handleSerialCommands();
  updateLedStatus();
  
  unsigned long now = millis();
  
  if (now - lastAudioScan >= AUDIO_SCAN_INTERVAL) {
    scanForAudioUnit();
    lastAudioScan = now;
  }

  if (now - lastLoRaUpdate >= LORA_LIVE_INTERVAL) {
    updateLoRaTelemetry();
    lastLoRaUpdate = now;
  }
  
  if (currentMode == MODE_OFF_ROAD || 
      (currentMode == MODE_AUTO && detectedMode == MODE_OFF_ROAD)) {
    updateOffRoadTelemetry();
  } else {
    updateOnRoadTelemetry();
  }
  
  autoDetectMode();
  
  if (isLogging) {
    static unsigned long lastCheck = 0;
    if (now - lastCheck > 30000) { 
      checkStorageAndRotate();
      lastCheck = now;
    }

    static unsigned long lastLogUpdate = 0;
    static unsigned long lastHighSpeedLog = 0;
    static float lastLoggedSpeed = 0.0;
    
    bool speedChanged = abs(sensorData.speed_kph - lastLoggedSpeed) > 0.5;
    bool throttleActive = abs(throttlePercent) > 5;
    bool highSpeed = sensorData.speed_kph > 10.0;
    
    if ((speedChanged || throttleActive || highSpeed) && 
        (now - lastHighSpeedLog >= GPS_HIGH_SPEED_INTERVAL)) {
      logData();
      lastHighSpeedLog = now;
      lastLoggedSpeed = sensorData.speed_kph;
    }
    else if (now - lastLogUpdate >= LOG_INTERVAL) {
      logData();
      lastLogUpdate = now;
    }
  } else {
    if (logFileOpen && (now - sessionStartTime > 20000)) {
      closeCurrentLog();
    }
  }

  if (!LOG_FORMAT_BINARY && isLogging && (now - lastLogFlush_STABLE >= LOG_FLUSH_INTERVAL_STABLE)) {
    flushLogBuffer();
  }

  static unsigned long lastWsUpdate = 0;
  if (now - lastWsUpdate >= WEBSOCKET_INTERVAL) {
    if (ws.count() > 0) sendWebSocketData();
    lastWsUpdate = now;
  }
  
  if (now - lastSoundUpdate >= SOUND_UPDATE_INTERVAL) {
    updateSoundUnit();
    lastSoundUpdate = now;
  }
  
  delay(5);
}

// =================================================================
// BATTERY SIMULATION ENGINE
// =================================================================
void updateBatterySimulation() {
  unsigned long now = millis();
  unsigned long dt_ms = now - battery.last_calc_time;
  
  if (battery.last_calc_time == 0) {
    battery.last_calc_time = now;
    return;
  }
  
  if (throttleConnected) {
    float throttle_decimal = abs(throttlePercent) / 100.0;
    float motor_draw = 0.0;
    
    if (battery.motor_type == MOTOR_BRUSHLESS) {
      motor_draw = (battery.peak_amps - battery.idle_amps) * pow(throttle_decimal, 1.5);
    } else {
      motor_draw = (battery.peak_amps - battery.idle_amps) * throttle_decimal;
    }
    
    motor_draw *= battery.calibration_factor;
    battery.current_amps = battery.idle_amps + motor_draw + battery.static_load_amps;
    
    float mah_slice = (battery.current_amps * dt_ms) / 3600.0; 
    battery.consumed_mah += mah_slice;
    
    // Battery calibration tracking
    if (batteryCalibration.mode == CAL_RECORDING) {
      batteryCalibration.predicted_mah = battery.consumed_mah;
    }
  } else {
    battery.current_amps = 0.0;
  }
  
  float remaining = battery.capacity_mah - battery.consumed_mah;
  battery.percentage = constrain((int)((remaining / battery.capacity_mah) * 100), 0, 100);
  
// Calculate per-cell voltage (3.5V empty to 4.2V full)
  float v_cell = map(battery.percentage, 0, 100, 350, 420) / 100.0;
  
  // Multiply by cell count (e.g., 2S, 3S, 4S)
  float v_rest = v_cell * battery.cells;
  
  // Voltage sag simulation (simplified resistance model)
  float v_sag = battery.current_amps * 0.02; 
  battery.voltage_sim = v_rest - v_sag;
   
  battery.last_calc_time = now;
}

void printBatteryReport() {
  log_m("REPORT", "=======================================");
  log_m("REPORT", "      BATTERY VALIDATION REPORT        ");
  log_m("REPORT", "=======================================");
  
  Serial.println("\n--- POWER VALIDATION ---");
  Serial.printf("Consumed:        %.2f mAh\n", battery.consumed_mah);
  Serial.printf("Capacity:        %d mAh\n", battery.capacity_mah);
  Serial.printf("Remaining:       %d %%\n", battery.percentage);
  Serial.printf("Final Voltage:   %.2f V\n", battery.voltage_sim);
  Serial.printf("Final Current:   %.2f A\n", battery.current_amps);
  
  Serial.println("\n--- CONFIG USED ---");
  Serial.printf("Motor Type:      %s\n", 
    battery.motor_type == MOTOR_BRUSHLESS ? "Brushless" : "Brushed");
  Serial.printf("Motor Turns:     %.1fT\n", battery.motor_turns);
  Serial.printf("Peak Amps:       %.1f A\n", battery.peak_amps);
  Serial.printf("ESC Rating:      %d A\n", battery.esc_rating_amps);
  Serial.printf("Idle Draw:       %.2f A\n", battery.idle_amps);
  Serial.printf("Static Load:     %.2f A\n", battery.static_load_amps);
  Serial.printf("Cal Factor:      %.2f\n", battery.calibration_factor);
  
  // Calibration mode report
  if (batteryCalibration.mode == CAL_COMPLETE && batteryCalibration.actual_mah > 0) {
    Serial.println("\n--- CALIBRATION RESULT ---");
    Serial.printf("Predicted:       %.2f mAh\n", batteryCalibration.predicted_mah);
    Serial.printf("Actual (Charger):%.2f mAh\n", batteryCalibration.actual_mah);
    Serial.printf("Error:           %.1f%%\n", 
      abs(batteryCalibration.predicted_mah - batteryCalibration.actual_mah) / 
      batteryCalibration.actual_mah * 100.0);
    Serial.printf("Suggested Factor:%.3f\n", batteryCalibration.calculatedFactor);
  }
  
  Serial.println("\n--- VALIDATION STEPS ---");
  Serial.println("1. Charge battery fully.");
  Serial.println("2. Note 'Charged Capacity' from charger.");
  Serial.println("3. Compare with 'Consumed' above.");
  Serial.println("   - Diff > 10%? Adjust Calibration Factor.");
  Serial.println("   - Factor > 1.0: System uses MORE than calculated.");
  Serial.println("   - Factor < 1.0: System uses LESS than calculated.");
  Serial.println("=======================================\n");
}

// =================================================================
// ENHANCED: SERIAL COMMAND HANDLER
// =================================================================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("AP_PASS=")) {
      String newPass = cmd.substring(8);
      newPass.trim();
      if (newPass.length() >= 8) {
        wifiPass = newPass;
        saveSettings();
        log_m("WIFI", "Password changed to: " + newPass);
        log_m("WIFI", "Restarting to apply...");
        delay(1000);
        ESP.restart();
      } else {
        log_m("WIFI", "Error: Password must be at least 8 chars");
      }
    }
    else if (cmd.equalsIgnoreCase("AP_RESET")) {
      wifiPass = "telemetry123";
      saveSettings();
      log_m("WIFI", "Password reset to default");
      log_m("WIFI", "Restarting to apply...");
      delay(1000);
      ESP.restart();
    }
    
    else if (cmd.startsWith("TEMP=")) {
      float ambient_temp = cmd.substring(5).toFloat();
      if (ambient_temp >= -20.0 && ambient_temp <= 60.0) {
        sensorData.temp_c = ambient_temp;
        lastAmbientTempUpdate = millis();
        manualTempOverride = true;  // Mark as manual
        log_m("TEMP", "Ambient set to " + String(ambient_temp, 1) + "°C (MANUAL)");
        saveSettings();
      } else {
        log_m("ERROR", "Temperature out of range (-20 to 60°C)");
      }
    }
    
    else if (cmd.startsWith("SET_BATTERY=")) {
      int cap = cmd.substring(12).toInt();
      if (cap > 0) {
        battery.capacity_mah = cap;
        battery.consumed_mah = 0;
        log_m("BATT", "Capacity set to " + String(cap) + " mAh");
        saveSettings();
      }
    }
    else if (cmd.startsWith("SET_STATIC=")) {
      float amps = cmd.substring(11).toFloat();
      if (amps >= 0) {
        battery.static_load_amps = amps;
        log_m("BATT", "Static Load set to " + String(amps) + " A");
        saveSettings();
      }
    }
    else if (cmd.startsWith("SET_FACTOR=")) {
      float factor = cmd.substring(11).toFloat();
      if (factor > 0) {
        battery.calibration_factor = factor;
        log_m("BATT", "Tuning Factor set to " + String(factor));
        saveSettings();
      }
    }
    else if (cmd.startsWith("SET_MOTOR=")) {
      String args = cmd.substring(10);
      int comma1 = args.indexOf(',');
      int comma2 = args.lastIndexOf(',');
      if (comma1 > 0 && comma2 > comma1) {
        battery.motor_type = args.substring(0, comma1).toInt();
        battery.peak_amps = args.substring(comma1 + 1, comma2).toFloat();
        battery.motor_turns = args.substring(comma2 + 1).toFloat();
        log_m("BATT", "Motor profile updated");
        saveSettings();
      }
    }
    else if (cmd.equalsIgnoreCase("RESET_BATTERY")) {
      battery.consumed_mah = 0;
      battery.percentage = 100;
      log_m("BATT", "Fuel gauge reset");
    }
    
    // NEW: Battery calibration commands
    else if (cmd.equalsIgnoreCase("CAL_START")) {
      batteryCalibration.startCalibration(battery.voltage_sim);
      battery.consumed_mah = 0;  // Reset counter
      log_m("CAL", "Calibration started - run vehicle until low battery");
      sendNotification("info", "Calibration mode started");
    }
    else if (cmd.startsWith("CAL_FINISH=")) {
      float charger_mah = cmd.substring(11).toFloat();
      if (charger_mah > 100.0 && batteryCalibration.mode == CAL_RECORDING) {
        batteryCalibration.finishCalibration(battery.consumed_mah);
        if (batteryCalibration.calculateFactor(charger_mah)) {
          log_m("CAL", "Calibration complete!");
          log_m("CAL", "Predicted: " + String(batteryCalibration.predicted_mah, 1) + " mAh");
          log_m("CAL", "Actual: " + String(batteryCalibration.actual_mah, 1) + " mAh");
          log_m("CAL", "Suggested factor: " + String(batteryCalibration.calculatedFactor, 3));
          log_m("CAL", "Type: SET_FACTOR=" + String(batteryCalibration.calculatedFactor, 3));
          sendNotification("success", "Calibration complete - see serial");
        }
      } else {
        log_m("ERROR", "Invalid calibration data or mode not active");
      }
    }
    
    else if (cmd.equalsIgnoreCase("LORA_TEST")) {
      log_m("LORA", "Forcing Summary Packet...");
      sendLoRaSummary();
    }
    
    else if (cmd.equalsIgnoreCase("BATTERY_REPORT")) {
      printBatteryReport();
    }
    
    else if (cmd.startsWith("CONVERT=")) {
        String binFile = cmd.substring(8);
        convertBinaryToCSV(binFile);
    }
    
    // NEW: Corrupted log recovery
    else if (cmd.startsWith("RECOVER=")) {
        String logFile = cmd.substring(8);
        recoverCorruptedLog(logFile);
    }
    
    // NEW: I2C statistics
    else if (cmd.equalsIgnoreCase("I2C_STATS")) {
      log_m("I2C", "========== I2C STATISTICS ==========");
      log_m("I2C", "Total Attempts: " + String(i2cStats.totalAttempts));
      log_m("I2C", "Successful:     " + String(i2cStats.successfulReads));
      log_m("I2C", "Timeouts:       " + String(i2cStats.timeouts));
      log_m("I2C", "Partial Reads:  " + String(i2cStats.partialReads));
      log_m("I2C", "CRC Errors:     " + String(i2cStats.crcErrors));
      log_m("I2C", "Success Rate:   " + String(i2cStats.getSuccessRate(), 1) + "%");
      log_m("I2C", "====================================");
    }
    
    // NEW: Validation statistics
    else if (cmd.equalsIgnoreCase("VAL_STATS")) {
      log_m("VAL", "======== VALIDATION STATISTICS =======");
      log_m("VAL", "Total Validations: " + String(validationStats.totalValidations));
      log_m("VAL", "Speed Violations:  " + String(validationStats.speedViolations));
      log_m("VAL", "G-Force Violations:" + String(validationStats.gForceViolations));
      log_m("VAL", "GPS Violations:    " + String(validationStats.gpsCoordViolations));
      log_m("VAL", "CRC Failures:      " + String(validationStats.crcFailures));
      log_m("VAL", "Header Failures:   " + String(validationStats.headerFailures));
      log_m("VAL", "Validation Rate:   " + String(validationStats.getValidationRate(), 1) + "%");
      log_m("VAL", "======================================");
    }
  }
}

// =================================================================
// LORA TELEMETRY FUNCTIONS
// =================================================================
void updateLoRaTelemetry() {
  if(!sensorConnected && !soundUnitConnected) return;

  LoRaLivePacket packet;
  
  packet.lat_scaled = (int32_t)(sensorData.latitude * 1000000.0);
  packet.lon_scaled = (int32_t)(sensorData.longitude * 1000000.0);
  packet.speed_kph = (uint8_t)constrain(sensorData.speed_kph, 0, 255);
  packet.sats = sensorData.satellites;
  
  packet.status_flags = 0;
  if (sensorData.gps_valid) packet.status_flags |= (1 << 0);
  if (isLogging)            packet.status_flags |= (1 << 1);
  
  Wire.beginTransmission(ADDR_LORA);
  Wire.write((uint8_t*)&packet, sizeof(packet));
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    if (!loraConnected) {
        log_m("LORA", "Connected");
        sendNotification("success", "LoRa module online");
    }
    loraConnected = true;
    loraLastSeen = millis();
  } else {
    if (loraConnected) {
        log_m("LORA", "Disconnected");
        sendNotification("warning", "LoRa module offline");
    }
    loraConnected = false;
  }
}

void sendLoRaSummary() {
  if (!loraConnected) {
    log_m("LORA", "Cannot send summary - module offline");
    return;
  }
  
  LoRaSummaryPacket packet;
  
  packet.max_speed_x10 = (uint16_t)(sensorData.max_speed * 10.0);
  packet.max_g_x100 = (uint16_t)(sensorData.max_g_force * 100.0);
  packet.run_time_ms = millis() - sessionStartTime;
  packet.lat_end = (int32_t)(sensorData.latitude * 1000000.0);
  packet.lon_end = (int32_t)(sensorData.longitude * 1000000.0);
  
  for(int i=0; i<3; i++) {
    Wire.beginTransmission(ADDR_LORA);
    Wire.write((uint8_t*)&packet, sizeof(packet));
    if(Wire.endTransmission() == 0) {
      log_m("LORA", "Summary Sent!");
      sendNotification("success", "LoRa summary transmitted");
      break;
    }
    delay(50);
  }
}

// =================================================================
// I2C INITIALIZATION & RECOVERY
// =================================================================
void initializeI2C() {
  log_m("I2C", "Stopping existing Wire instance...");
  Wire.end(); 
  delay(50); 

  log_m("I2C", "Attempting bus recovery...");
  recoverI2CBus(); 
  delay(50);      

  log_m("I2C", "Initializing Grove I2C...");
  Wire.begin(GROVE_SDA_PIN, GROVE_SCL_PIN); 
  Wire.setClock(I2C_FREQUENCY);             
  Wire.setTimeout(I2C_TIMEOUT);             
  Wire.setBufferSize(128);  // Increased for packet protocol

  log_m("I2C", "Initialized " + String(I2C_FREQUENCY/1000) + 
        "kHz on SDA=" + String(GROVE_SDA_PIN) + 
        " SCL=" + String(GROVE_SCL_PIN));
  delay(100); 
}

void recoverI2CBus() {
  pinMode(GROVE_SDA_PIN, INPUT_PULLUP); 
  
  if (digitalRead(GROVE_SDA_PIN) == LOW) {
    log_m("WARN", "I2C SDA stuck low - attempting clock pulse recovery");
    
    pinMode(GROVE_SCL_PIN, OUTPUT);
    for (int i = 0; i < 9; i++) {
      digitalWrite(GROVE_SCL_PIN, LOW);
      delayMicroseconds(5);
      digitalWrite(GROVE_SCL_PIN, HIGH);
      delayMicroseconds(5);
    }
    pinMode(GROVE_SDA_PIN, INPUT_PULLUP); 
  } else {
    log_m("I2C", "SDA line appears free");
  }
}

void scanI2CBus() {
  byte error, address;
  int nDevices = 0;
  log_m("I2C", "===== SCAN START =====");
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      log_m("I2C", "Device 0x" + String(address, HEX));
      nDevices++;
      if (address == ADDR_SECONDARY) log_m("I2C", "  -> Secondary");
      else if (address == ADDR_SOUND) log_m("I2C", "  -> Audio");
      else if (address == ADDR_LORA) {
          log_m("I2C", "  -> LoRa");
          loraConnected = true; 
          loraLastSeen = millis();
      }
    }
  }
  log_m("I2C", "Found " + String(nDevices) + " devices");
}

void scanForAudioUnit() {
  Wire.flush();
  delay(10); 
  
  uint8_t bytesRead = Wire.requestFrom((uint8_t)ADDR_SOUND, (uint8_t)2, true);
  
  if (bytesRead >= 2) {
    uint8_t marker = Wire.read();
    uint8_t msgCount = Wire.read();
    if (marker == 0xA5) {  
      if (!soundUnitConnected) {
        soundUnitConnected = true;
        soundLastSeen = millis();
        log_m("AUDIO", "Connected!");
        sendNotification("success", "Audio online");
      }
      soundLastSeen = millis(); 
    } 
  } else {
    if (soundUnitConnected) { 
      soundUnitConnected = false;
      log_m("AUDIO", "Disconnected");
      sendNotification("error", "Audio offline");
    }
  }
  delay(5); 
}

bool sendI2CCommand(uint8_t addr, uint8_t command) {
  Wire.beginTransmission(addr);
  Wire.write(command);
  return (Wire.endTransmission() == 0);
}

// =================================================================
// ENHANCED: NON-BLOCKING I2C WITH CRC VALIDATION
// =================================================================
bool updateSensorDataAsync() {
  uint32_t now_us = micros();
  
  switch(i2cContext.state) {
    
    case I2C_IDLE: {
      Wire.beginTransmission(ADDR_SECONDARY);
      Wire.write(CMD_GET_TELEMETRY);
      byte error = Wire.endTransmission();
      
      if (error != 0) {
        i2cStats.recordAttempt();
        static unsigned long lastError = 0;
        if (millis() - lastError > 1000) {
          log_m("ERROR", "I2C TX failed: " + String(error));
          lastError = millis();
        }
        
        if(i2cContext.retryCount < I2CAsyncContext::MAX_RETRIES) {
          i2cContext.retryCount++;
          return false;
        } else {
          i2cContext.retryCount = 0;
          return false;
        }
      }
      
      i2cContext.state = I2C_REQUEST_SENT;
      i2cContext.requestTime_us = now_us;
      i2cContext.retryCount = 0; 
      return false;
    }
    
    case I2C_REQUEST_SENT: {
      if(now_us - i2cContext.requestTime_us >= I2CAsyncContext::SEC_RESPONSE_TIME_US) {
        i2cContext.state = I2C_AWAITING_DATA;
      }
      return false;
    }
    
    case I2C_AWAITING_DATA: {
      if(now_us - i2cContext.requestTime_us >= I2CAsyncContext::TIMEOUT_US) {
        i2cStats.recordAttempt();
        i2cStats.recordTimeout();
        
        static unsigned long lastTimeoutLog = 0;
        if(millis() - lastTimeoutLog > 1000) {
          log_m("ERROR", "I2C timeout after " + 
                String((now_us - i2cContext.requestTime_us)/1000) + "ms");
          lastTimeoutLog = millis();
        }
        
        Wire.flush(); 
        i2cContext.state = I2C_IDLE;
        return false;
      }
      
      uint8_t bytesAvailable = Wire.available();
      if(bytesAvailable == 0) {
        static bool requestSent = false;
        if(!requestSent) {
          Wire.requestFrom(ADDR_SECONDARY, (uint8_t)sizeof(CompactTelemetryPacket), (uint8_t)true);
          requestSent = true;
        }
        else {
            requestSent = false; 
        }
        return false;
      }
      
      if(bytesAvailable < (int)sizeof(CompactTelemetryPacket)) {
        return false;
      }
      
      i2cContext.state = I2C_PROCESSING;
      return false; 
    }
    
    case I2C_PROCESSING: {
      i2cStats.recordAttempt();
      
      size_t bytesRead = Wire.readBytes((uint8_t*)&i2cContext.tempPacket, sizeof(CompactTelemetryPacket));
      
      if (bytesRead != sizeof(CompactTelemetryPacket)) {
        i2cStats.recordPartialRead();
        log_m("ERROR", "Partial read: " + String(bytesRead) + "/" + String(sizeof(CompactTelemetryPacket)));
        Wire.flush();
        i2cContext.state = I2C_IDLE;
        return false;
      }
      
      if(!validateTelemetryPacket(i2cContext.tempPacket)) {
        i2cStats.recordCrcError();
        static unsigned long lastValidationError = 0;
        if(millis() - lastValidationError > 1000) {
          log_m("ERROR", "Packet validation failed");
          lastValidationError = millis();
        }
        i2cContext.state = I2C_IDLE;
        return false;
      }
      
      unpackTelemetryData(i2cContext.tempPacket);
      i2cStats.recordSuccess();
      i2cContext.state = I2C_IDLE;
      i2cContext.dataReady = true;
      
      return true; 
    }
  }
  
  return false;
}

void unpackTelemetryData(const CompactTelemetryPacket& packet) {
  sensorData.speed_kph = packet.speed;
  sensorData.g_force_total = packet.gforce_total;
  sensorData.g_force_lateral = packet.gforce_lat;
  sensorData.g_force_longitudinal = packet.gforce_lon;
  sensorData.yaw_rate = packet.yaw;
  sensorData.latitude = packet.lat;
  sensorData.longitude = packet.lon;
  sensorData.max_speed = packet.maxSpeed;
  sensorData.max_g_force = packet.maxG;
  sensorData.satellites = packet.sats;
  sensorData.date = packet.date;
  sensorData.time = packet.time;
  sensorData.hdop = packet.hdop_x10;  // Now using uint16_t

  // Temperature handling with validation
  if (packet.temp_c_x10 != -999 && !manualTempOverride) {  // -999 = invalid sensor
    sensorData.temp_c = packet.temp_c_x10 / 10.0;
    sensorData.temp_valid = true;
  } else if (manualTempOverride) {
    sensorData.temp_valid = true;  // Manual override is always valid
  } else {
    sensorData.temp_valid = false;
  }

  sensorData.gps_valid = (packet.flags & 0x02);
  sessionActive = (packet.flags & 0x04);
  currentUnits = (packet.flags & 0x08) ? "imperial" : "metric";
  sensorData.imu_connected = (packet.flags & 0x10);
  
  sensorData.heading = 0;
  sensorData.altitude = 0;
}

// NEW: Robust packet validation with CRC
bool validateTelemetryPacket(const CompactTelemetryPacket& packet) {
  validationStats.totalValidations++;
  
  // Check header
  if (packet.header != PACKET_HEADER) {
    validationStats.headerFailures++;
    return false;
  }
  
  // Check version
  if (packet.version != PACKET_VERSION) {
    log_m("WARN", "Packet version mismatch: " + String(packet.version));
  }
  
  // Verify CRC
  uint8_t payload_size = sizeof(CompactTelemetryPacket) - 4;  // Exclude header, version, length, crc
  uint8_t* payload_start = ((uint8_t*)&packet) + 3;  // Skip to payload
  uint8_t calculated_crc = crc8(payload_start, payload_size);
  
  if (calculated_crc != packet.crc) {
    validationStats.crcFailures++;
    return false;
  }
  
  // Validate payload ranges
  bool isValid = true;
  
  if(packet.speed < SensorLimits::MIN_SPEED_KPH || packet.speed > SensorLimits::MAX_SPEED_KPH) {
    validationStats.speedViolations++;
    isValid = false;
  }
  
  if(packet.gforce_lat < SensorLimits::MIN_G_FORCE || packet.gforce_lat > SensorLimits::MAX_G_FORCE ||
     packet.gforce_lon < SensorLimits::MIN_G_FORCE || packet.gforce_lon > SensorLimits::MAX_G_FORCE) {
    validationStats.gForceViolations++;
    isValid = false;
  }
  
  if((packet.flags & 0x02)) { 
      if(packet.lat < SensorLimits::MIN_LATITUDE || packet.lat > SensorLimits::MAX_LATITUDE ||
         packet.lon < SensorLimits::MIN_LONGITUDE || packet.lon > SensorLimits::MAX_LONGITUDE) {
        validationStats.gpsCoordViolations++;
        isValid = false;
      }
  }
  
  return isValid;
}

void assessGPSQuality() {
  float hdop_actual = (float)sensorData.hdop / 10.0f;  // Updated scaling
  uint8_t sats = sensorData.satellites;
  
  gpsValidation.validForPerformanceClaims = false;
  
  if(!sensorData.gps_valid || sats == 0) {
    gpsValidation.quality = GPS_NO_FIX;
  }
  else if(sats >= 10 && hdop_actual < 1.5f) {
    gpsValidation.quality = GPS_EXCELLENT;
    gpsValidation.validForPerformanceClaims = true;
    gpsValidation.totalSamplesExcellent++;
    
    uint32_t now = millis();
    if(now - gpsValidation.lastExcellentTimestamp < 2000) {
      gpsValidation.excellentQualityDuration_ms += (now - gpsValidation.lastExcellentTimestamp);
    } else {
      gpsValidation.excellentQualityDuration_ms = 0;
    }
    gpsValidation.lastExcellentTimestamp = now;
    
    if(sensorData.speed_kph > gpsValidation.maxSpeedDuringExcellentGPS_kph) {
      gpsValidation.maxSpeedDuringExcellentGPS_kph = sensorData.speed_kph;
      gpsValidation.satellitesAtMaxSpeed = sats;
      gpsValidation.hdopAtMaxSpeed = sensorData.hdop;
    }
  }
  else if(sats >= 8 && hdop_actual < 2.0f) {
    gpsValidation.quality = GPS_GOOD;
    gpsValidation.validForPerformanceClaims = true; 
    gpsValidation.totalSamplesGood++;
  }
  else if(sats >= 6 && hdop_actual < 3.0f) {
    gpsValidation.quality = GPS_ACCEPTABLE;
    gpsValidation.totalSamplesAcceptable++;
  }
  else {
    gpsValidation.quality = GPS_POOR;
    gpsValidation.totalSamplesPoor++;
  }
  
  sensorData.gps_quality = gpsValidation.quality;
}

const char* getGPSQualityString(GPSQuality quality) {
  switch(quality) {
    case GPS_NO_FIX:      return "NO_FIX";
    case GPS_POOR:        return "POOR";
    case GPS_ACCEPTABLE:  return "ACCEPTABLE";
    case GPS_GOOD:        return "GOOD";
    case GPS_EXCELLENT:   return "EXCELLENT";
    default:              return "UNKNOWN";
  }
}

void printGPSValidationSummary() {
  log_m("GPS-SUMMARY", "========================================");
  log_m("GPS-SUMMARY", "GPS Quality Session Statistics:");
  log_m("GPS-SUMMARY", "  Excellent: " + String(gpsValidation.totalSamplesExcellent) + " samples");
  log_m("GPS-SUMMARY", "  Good:      " + String(gpsValidation.totalSamplesGood) + " samples");
  log_m("GPS-SUMMARY", "  Acceptable:" + String(gpsValidation.totalSamplesAcceptable) + " samples");
  log_m("GPS-SUMMARY", "  Poor:      " + String(gpsValidation.totalSamplesPoor) + " samples");
  
  if(gpsValidation.validForPerformanceClaims) {
    log_m("GPS-SUMMARY", "");
    log_m("GPS-SUMMARY", "GPS QUALITY SUFFICIENT FOR CLAIMS");
    log_m("GPS-SUMMARY", "Max Speed: " + String(gpsValidation.maxSpeedDuringExcellentGPS_kph, 2) + " km/h");
    log_m("GPS-SUMMARY", "  @ " + String(gpsValidation.satellitesAtMaxSpeed) + " sats, " +
          "HDOP " + String(gpsValidation.hdopAtMaxSpeed / 10.0, 2));  // Updated scaling
  }
  
  log_m("GPS-SUMMARY", "========================================");
}

bool sendSoundDataPacket(const AudioDataPacket& packet) {
  for (int retry = 0; retry < I2C_RETRY; retry++) {
    Wire.beginTransmission(ADDR_SOUND);
    Wire.write(AUDIO_PACKET_CMD);
    int bytesWritten = Wire.write((uint8_t*)&packet, sizeof(packet));
    
    if (bytesWritten != sizeof(packet)) continue;
    
    byte error = Wire.endTransmission();
    if (error == 0) {
      soundLastSeen = millis();
      if (!soundUnitConnected) {
        log_m("AUDIO", "Re-connected");
        sendNotification("success", "Audio online");
      }
      soundUnitConnected = true;
      return true;
    }
    delay(10);
  }
  soundUnitConnected = false;
  return false;
}

// =================================================================
// WEB SERVER SETUP
// =================================================================
extern const char dashboard_html[] PROGMEM; 

void setupWebServer() {
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", dashboard_html);
  });

  server.on("/api/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      String fileName = String(file.name());
      if (!file.isDirectory() && (fileName.endsWith(".csv") || fileName.endsWith(".bin"))) { 
        if (!first) json += ',';
        String cleanName = fileName;
        if (cleanName.startsWith("/")) cleanName = cleanName.substring(1);
        json += "{\"name\":\"" + cleanName + "\",\"size\":" + String(file.size()) + "}";
        first = false;
      }
      file.close(); 
      file = root.openNextFile();
    }
    root.close();
    json += "]";
    request->send(200, "application/json", json);
  });

  // ENHANCED: Smart Sync with auto-conversion
  server.on("/api/latestlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    String latest = getLatestLogFile();
    if (latest == "") {
      request->send(404, "text/plain", "No logs found");
      return;
    }
    
    // Auto-convert binary to CSV
    if (latest.endsWith(".bin")) {
      String csvFile = latest;
      csvFile.replace(".bin", ".csv");
      
      if (!LittleFS.exists(csvFile)) {
        convertBinaryToCSV(latest);
      }
      
      if (LittleFS.exists(csvFile)) {
        latest = csvFile;
      }
    }
    
    request->redirect("/api/download?file=" + latest);
  });

  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String fileName = request->getParam("file")->value();
    if (!fileName.startsWith("/")) fileName = "/" + fileName;

    if (!LittleFS.exists(fileName)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    
    String contentType = fileName.endsWith(".csv") ? "text/csv" : "application/octet-stream";
    
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, fileName, contentType, true);
    response->addHeader("Content-Disposition", "attachment; filename=\"" + fileName.substring(1) + "\"");
    request->send(response);
  });

  server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("file", true)) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String fileName = request->getParam("file", true)->value();
    if (!fileName.startsWith("/")) fileName = "/" + fileName;

    if (LittleFS.remove(fileName)) {
      sendNotification("success", "Deleted " + fileName.substring(1));
      request->send(200, "text/plain", "Deleted");
    } else {
      request->send(500, "text/plain", "Delete failed");
    }
  });

  server.begin();
  log_m("WEB", "Web Server Started");
}

void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      log_m("WS", "Client connected");
      break;
    case WS_EVT_DISCONNECT:
      log_m("WS", "Client disconnected");
      break;
    case WS_EVT_DATA:
      { 
        AwsFrameInfo* info = (AwsFrameInfo*)arg; 
        if (info->opcode == WS_TEXT) {
          data[len] = 0; 
          handleWebCommand((const char*)data);
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebCommand(const char* json) {
  DeserializationError error = deserializeJson(wsCmdDoc, json);
  if (error) return;
  
  const char* command = wsCmdDoc["command"];
  if (command == nullptr) return;
  
  if (strcmp(command, "toggleLog") == 0) {
    if (isLogging) stopLog(); 
    else startNewLog();
  } 
  else if (strcmp(command, "getSettings") == 0) {
    sendWebSocketData(); 
  }
  else if (strcmp(command, "getLogs") == 0) {
    String json;
    wsLogDoc.clear();
    JsonArray logArray = wsLogDoc.to<JsonArray>();
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      logArray.add(logBuffer[(logBufferIndex + i) % LOG_BUFFER_SIZE]);
    }
    serializeJson(wsLogDoc, json);
    ws.textAll(json);
  }
  else if (strcmp(command, "startSteeringCal") == 0) {
    steeringCalState = WAIT_LEFT;
    calStateStartTime = millis();  // Start timeout
    tempCalAngleMin = wsCmdDoc["minAngle"] | -30.0f;
    tempCalAngleCenter = wsCmdDoc["centerAngle"] | 0.0f;
    tempCalAngleMax = wsCmdDoc["maxAngle"] | 30.0f;
    steeringReversed = wsCmdDoc["reversed"] | false;
    sendCalStep("Turn full LEFT and click again...", "var(--warning)");
  } 
  else if (strcmp(command, "switchMode") == 0) {
    int newModeInt = wsCmdDoc["mode"] | -1;
    if (newModeInt >= 0 && newModeInt <= 2) {
      switchMode((DrivingMode)newModeInt);
      saveSettings();
    }
  } 
  else if (strcmp(command, "toggleAutoMode") == 0) {
    modeAutoDetect = !modeAutoDetect;
    saveSettings();
  } 
  else if (strcmp(command, "deleteAllLogs") == 0) {
    deleteAllLogFiles();
  } 
  else if (strcmp(command, "setAudio") == 0) {
    masterVolume = wsCmdDoc["master"] | masterVolume;
    engineVolume = wsCmdDoc["engine"] | engineVolume;
    turboVolume = wsCmdDoc["turbo"] | turboVolume;
    mechanicalVolume = wsCmdDoc["mech"] | mechanicalVolume;
    audioMuted = wsCmdDoc["mute"] | audioMuted;
    user_lpf_override = wsCmdDoc["lpf"] | user_lpf_override;
    user_hpf_override = wsCmdDoc["hpf"] | user_hpf_override;
    user_distortion_k = wsCmdDoc["dist"] | user_distortion_k;
    saveSettings();
    updateSoundUnit();
  }
  else if (strcmp(command, "setBatteryConfig") == 0) {
    battery.capacity_mah = wsCmdDoc["capacity"] | battery.capacity_mah;
    battery.cells = wsCmdDoc["cells"] | battery.cells; // NEW: Handle cell update
    battery.static_load_amps = wsCmdDoc["static"] | battery.static_load_amps;
    battery.calibration_factor = wsCmdDoc["factor"] | battery.calibration_factor;
    saveSettings();
    log_m("BATT", "Configuration updated via dashboard (Cells: " + String(battery.cells) + "S)");
  }
  else if (strcmp(command, "setMotorConfig") == 0) {
    battery.motor_type = wsCmdDoc["type"] | battery.motor_type;
    battery.peak_amps = wsCmdDoc["amps"] | battery.peak_amps;
    battery.motor_turns = wsCmdDoc["turns"] | battery.motor_turns;
    battery.esc_rating_amps = wsCmdDoc["esc"] | battery.esc_rating_amps;
    saveSettings();
    log_m("BATT", "Motor profile updated via dashboard");
  }
  else if (strcmp(command, "setAmbientTemp") == 0) {
    float temp = wsCmdDoc["temp"];
    if (temp >= -20.0 && temp <= 60.0) {
      sensorData.temp_c = temp;
      lastAmbientTempUpdate = millis();
      manualTempOverride = true;  // Mark as manual
      saveSettings();
      log_m("TEMP", "Updated via Dashboard: " + String(temp, 1) + "C (MANUAL)");
    }
  }
  else if (strcmp(command, "resetBattery") == 0) {
    battery.consumed_mah = 0;
    battery.percentage = 100;
    log_m("BATT", "Fuel gauge reset via dashboard");
  }
  // NEW: Battery calibration dashboard commands
  else if (strcmp(command, "startCalibration") == 0) {
    batteryCalibration.startCalibration(battery.voltage_sim);
    battery.consumed_mah = 0;
    log_m("CAL", "Calibration started via dashboard");
    sendNotification("info", "Battery calibration mode started");
  }
  else if (strcmp(command, "finishCalibration") == 0) {
    float charger_mah = wsCmdDoc["charger_mah"];
    if (charger_mah > 100.0 && batteryCalibration.mode == CAL_RECORDING) {
      batteryCalibration.finishCalibration(battery.consumed_mah);
      if (batteryCalibration.calculateFactor(charger_mah)) {
        sendNotification("success", "Factor: " + String(batteryCalibration.calculatedFactor, 3));
      }
    }
  }
}

void sendWebSocketData() {
  if (ws.count() == 0) return;
  
  wsDataDoc.clear();
  
  wsDataDoc["fw"] = FW_VERSION;
  wsDataDoc["speed"] = sensorData.speed_kph;
  wsDataDoc["units"] = currentUnits;
  wsDataDoc["mode"] = (int)currentMode;
  wsDataDoc["detectedMode"] = (int)detectedMode;
  wsDataDoc["log_active"] = isLogging;
  
  wsDataDoc["rc_thr"] = throttlePercent;
  wsDataDoc["rc_steer"] = steeringAngleDeg;
  wsDataDoc["rc_thr_conn"] = throttleConnected;
  wsDataDoc["rc_steer_conn"] = steeringConnected;
  
  wsDataDoc["sensor_conn"] = sensorConnected;
  wsDataDoc["audio_conn"] = soundUnitConnected;
  wsDataDoc["lora_conn"] = loraConnected;
  wsDataDoc["lora_last"] = lastLoRaUpdate;
  
  wsDataDoc["throttlePwm"] = throttlePwmPulseWidth;
  wsDataDoc["steeringPwm"] = steeringPwmPulseWidth;
  
  wsDataDoc["lapCount"] = onroad.lap_count;
  wsDataDoc["lapTime"] = onroad.lap_elapsed_ms;
  wsDataDoc["bestLap"] = onroad.best_lap_ms;

  // Enhanced battery telemetry
  JsonObject batt = wsDataDoc.createNestedObject("batt");
  batt["volts"] = battery.voltage_sim;
  batt["amps"] = battery.current_amps;
  batt["mah_used"] = battery.consumed_mah;
  batt["pct"] = battery.percentage;
  batt["cap"] = battery.capacity_mah;
  batt["static"] = battery.static_load_amps;
  batt["factor"] = battery.calibration_factor;
  batt["m_type"] = battery.motor_type;
  batt["m_amps"] = battery.peak_amps;
  batt["m_turns"] = battery.motor_turns;
  batt["esc"] = battery.esc_rating_amps;
  batt["cal_mode"] = batteryCalibration.mode;
  batt["cal_predicted"] = batteryCalibration.predicted_mah;
  batt["cal_factor"] = batteryCalibration.calculatedFactor;

  JsonObject tel = wsDataDoc.createNestedObject("tel");
  tel["max_speed"] = sensorData.max_speed;
  tel["max_g"] = sensorData.max_g_force;
  tel["sats"] = sensorData.satellites;
  tel["gps_valid"] = sensorData.gps_valid;
  tel["g_lat"] = sensorData.g_force_lateral;
  tel["g_lon"] = sensorData.g_force_longitudinal;
  tel["temp"] = sensorData.temp_c;
  tel["temp_valid"] = sensorData.temp_valid;  // NEW
  tel["temp_manual"] = manualTempOverride;    // NEW
  
  // NEW: GPS quality telemetry
  tel["gps_quality"] = (int)gpsValidation.quality;
  tel["gps_quality_str"] = getGPSQualityString(gpsValidation.quality);
  tel["gps_valid_claims"] = gpsValidation.validForPerformanceClaims;
  tel["gps_hdop"] = sensorData.hdop / 10.0;  // Display as float
  tel["gps_excellent_pct"] = gpsValidation.getExcellentPercentage();

  JsonObject off = wsDataDoc.createNestedObject("off");
  off["roll"] = offroad.roll_deg;
  off["pitch"] = offroad.pitch_deg;
  off["slip_angle"] = offroad.slip_angle_deg;
  off["drift"] = offroad.is_drifting;
  off["airborne"] = offroad.is_airborne;
  off["surface"] = getSurfaceString(offroad.surface);
  off["front_traction"] = offroad.front_traction;
  off["rear_traction"] = offroad.rear_traction;
  off["awd_active"] = offroad.awd_active;
  off["max_roll"] = offroad.max_roll_deg;
  off["max_pitch"] = offroad.max_pitch_deg;
  off["max_drift"] = offroad.drift_angle_max;
  off["max_landing_g"] = offroad.landing_g_max;
  
  JsonObject settings = wsDataDoc.createNestedObject("settings");
  settings["masterVolume"] = masterVolume;
  settings["engineVolume"] = engineVolume;
  settings["turboVolume"] = turboVolume;
  settings["mechanicalVolume"] = mechanicalVolume;
  settings["audioMuted"] = audioMuted;
  settings["user_lpf"] = user_lpf_override;
  settings["user_hpf"] = user_hpf_override;
  settings["user_dist"] = user_distortion_k;
  settings["steeringMinPwm"] = steeringMinPwm;
  settings["steeringCenterPwm"] = steeringCenterPwm;
  settings["steeringMaxPwm"] = steeringMaxPwm;
  settings["steeringReversed"] = steeringReversed;
  settings["steeringAngleMax"] = steeringAngleMax;

  wsDataDoc["drive"]["rpm"] = 0; 
  wsDataDoc["drive"]["gear"] = 1;
  wsDataDoc["drive"]["boost"] = 0;

  JsonObject store = wsDataDoc.createNestedObject("storage");
  store["total"] = LittleFS.totalBytes();
  store["used"] = LittleFS.usedBytes();
  store["free"] = getFreeStorage();
  store["files"] = countLogFiles();
  store["current_size"] = currentFileSize;
  store["format"] = LOG_FORMAT_BINARY ? "binary" : "csv";
  
  size_t freeSpace = getFreeStorage();
  size_t recordSize = LOG_FORMAT_BINARY ? sizeof(LogRecord) : 100;
  uint32_t remainingRecords = freeSpace / recordSize;
  uint32_t remainingMinutes = remainingRecords / (LOG_SAMPLE_RATE_HZ * 60);
  
  store["remaining_records"] = remainingRecords;
  store["remaining_minutes"] = remainingMinutes;
  
  // NEW: I2C and validation statistics
  JsonObject i2c = wsDataDoc.createNestedObject("i2c");
  i2c["success_rate"] = i2cStats.getSuccessRate();
  i2c["attempts"] = i2cStats.totalAttempts;
  i2c["timeouts"] = i2cStats.timeouts;
  i2c["crc_errors"] = i2cStats.crcErrors;
  
  JsonObject val = wsDataDoc.createNestedObject("validation");
  val["rate"] = validationStats.getValidationRate();
  val["violations"] = validationStats.getTotalViolations();

  String json;
  serializeJson(wsDataDoc, json);
  ws.textAll(json);
}

void sendNotification(const char* status, const String& message) {
  StaticJsonDocument<256> doc;
  doc["type"] = "notification";
  doc["status"] = status;
  doc["message"] = message;
  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

// =================================================================
// SYSTEM STATUS MANAGEMENT
// =================================================================
void updateSystemStatus() {
  unsigned long now = millis();
  
  if (sensorConnected && (now - sensorLastSeen > 2000)) {
    sensorConnected = false;
    log_m("I2C", "Secondary disconnected");
  }
  
  bool newSensorData = updateSensorDataAsync();
  if(newSensorData) {
    sensorLastSeen = millis();
    sensorConnected = true;
    assessGPSQuality();
  }
}

void updateWiFiStatus() {
  unsigned long now = millis();
  if (now - lastWifiCheck < WIFI_RECONNECT_INTERVAL) return;
  lastWifiCheck = now;
  
  if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
    WiFi.softAP(WIFI_SSID, wifiPass.c_str());
  }
  
  int clients = WiFi.softAPgetStationNum();
  if (clients != connectedClients) {
    connectedClients = clients;
  }
  
  if (now - lastClientPing >= CLIENT_PING_INTERVAL) {
    ws.cleanupClients();
    lastClientPing = now;
  }
}

// ENHANCED: LED Status with GPS quality indication
void updateLedStatus() {
  unsigned long now = millis();
  bool blink = false;
  uint32_t color = COLOR_NOMINAL_ON;
  static uint32_t lastColor = (uint32_t)-1;

  if (isLogging) {
    color = COLOR_LOGGING;
  }
  else if (!sensorConnected || !soundUnitConnected) {
    color = COLOR_ERROR_I2C;
    blink = true;
  }
  else if (!throttleConnected || !steeringConnected) {
    color = COLOR_WARNING_RC;
    blink = true;
  }
  // NEW: GPS quality indication
  else if (sensorData.gps_valid && gpsValidation.quality <= GPS_POOR) {
    color = COLOR_GPS_POOR;
    blink = true;
  }
  else {
    color = (currentMode == MODE_OFF_ROAD || 
             (currentMode == MODE_AUTO && detectedMode == MODE_OFF_ROAD)) 
             ? COLOR_NOMINAL_OFF : COLOR_NOMINAL_ON;
  }

  if (blink) {
    if (now - lastBlinkTime > 500) {
      ledState = !ledState;
      lastBlinkTime = now;
    }
  } else {
    ledState = true;
  }

  uint32_t finalColor = ledState ? color : COLOR_OFF;
  if (finalColor != lastColor) {
    strip.setPixelColor(0, finalColor);
    strip.show();
    lastColor = finalColor;
  }
}

// =================================================================
// INTERRUPT SERVICE ROUTINES (ATOMIC)
// =================================================================
void IRAM_ATTR throttleISR() {
  portENTER_CRITICAL_ISR(&mux);
  
  unsigned long now = micros();
  
  if (digitalRead(THROTTLE_PIN) == HIGH) {
    throttlePwmRiseTime = now;
  } else if (throttlePwmRiseTime > 0) {
    unsigned long width = now - throttlePwmRiseTime;
    if (width >= PWM_MIN_VALID && width <= PWM_MAX_VALID) {
      throttlePwmPulseWidth = width;
      throttlePwmFallTime = now;
      throttlePwmNewData = true;
    }
    throttlePwmRiseTime = 0;
  }
  
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR steeringISR() {
  portENTER_CRITICAL_ISR(&mux);
  
  unsigned long now = micros();
  
  if (digitalRead(STEERING_PIN) == HIGH) {
    steeringPwmRiseTime = now;
  } else if (steeringPwmRiseTime > 0) {
    unsigned long width = now - steeringPwmRiseTime;
    if (width >= PWM_MIN_VALID && width <= PWM_MAX_VALID) {
      steeringPwmPulseWidth = width;
      steeringPwmFallTime = now;
      steeringPwmNewData = true;
    }
    steeringPwmRiseTime = 0;
  }
  
  portEXIT_CRITICAL_ISR(&mux);
}

void updateRCInputs() {
  unsigned long now = millis();
  
  unsigned long currentThrottlePwm = 1500;
  unsigned long currentSteeringPwm = 1500;
  bool newThrottle = false;
  bool newSteering = false;

  portENTER_CRITICAL(&mux);
  currentThrottlePwm = throttlePwmPulseWidth;
  if (throttlePwmNewData) {
    throttlePwmNewData = false;
    newThrottle = true;
  }
  currentSteeringPwm = steeringPwmPulseWidth;
  if (steeringPwmNewData) {
    steeringPwmNewData = false;
    newSteering = true;
  }
  portEXIT_CRITICAL(&mux);

  if (newThrottle) {
    if (!throttleConnected) throttleConnected = true;
    
    if (calibrationMode) {
      if (currentThrottlePwm < throttleMin) throttleMin = currentThrottlePwm;
      if (currentThrottlePwm > throttleMax) throttleMax = currentThrottlePwm;
    }
    
    if (currentThrottlePwm > (RC_CENTER_DEFAULT + RC_DEADBAND)) {
      throttlePercent = map(currentThrottlePwm, 1500, throttleMax, 0, 100);
    } else if (currentThrottlePwm < (RC_CENTER_DEFAULT - RC_DEADBAND)) {
      throttlePercent = map(currentThrottlePwm, throttleMin, 1500, -100, 0);
    } else {
      throttlePercent = 0;
    }
    throttlePercent = constrain(throttlePercent, -100, 100);
    
    lastThrottleSignal = now;
  }
  
  if (newSteering) {
    if (!steeringConnected) steeringConnected = true;
    
    // ENHANCED: Calibration with timeout
    if (steeringCalState != IDLE) {
      // Check timeout
      if (now - calStateStartTime > CALIBRATION_TIMEOUT) {
        log_m("CAL", "Calibration timeout - reverting to defaults");
        steeringCalState = IDLE;
        sendCalStep("Timeout - using defaults", "var(--error)", true);
      } else {
        processSteeringCalibration(currentSteeringPwm);
      }
    }
    
    steeringAngleDeg = calculateSteeringAngle(currentSteeringPwm);
    lastSteeringSignal = now;
  }
  
  if (throttleConnected && (now - lastThrottleSignal > RC_TIMEOUT)) {
    throttleConnected = false;
    throttlePercent = 0;
  }
  if (steeringConnected && (now - lastSteeringSignal > RC_TIMEOUT)) {
    steeringConnected = false;
    steeringAngleDeg = steeringAngleCenter;
  }
}

float calculateSteeringAngle(unsigned long pwmValue) {
  pwmValue = constrain(pwmValue, steeringMinPwm, steeringMaxPwm);
  
  if (abs((long)pwmValue - (long)steeringCenterPwm) <= RC_DEADBAND) {
    return steeringAngleCenter;
  }
  
  float angle;
  long mapAngleMin = steeringReversed ? (long)(steeringAngleMax * 10) : (long)(steeringAngleMin * 10);
  long mapAngleMax = steeringReversed ? (long)(steeringAngleMin * 10) : (long)(steeringAngleMax * 10);
  
  if (pwmValue > steeringCenterPwm) {
    angle = map(pwmValue, steeringCenterPwm, steeringMaxPwm, 
                (long)(steeringAngleCenter * 10), mapAngleMax) / 10.0;
  } else {
    angle = map(pwmValue, steeringMinPwm, steeringCenterPwm, 
                mapAngleMin, (long)(steeringAngleCenter * 10)) / 10.0;
  }
  
  return constrain(angle, steeringAngleMin, steeringAngleMax);
}

void sendCalStep(String message, String color, bool done) {
  StaticJsonDocument<256> doc;
  doc["type"] = "cal_step";
  doc["message"] = message;
  doc["color"] = color;
  if (done) doc["done"] = true;
  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

void processSteeringCalibration(unsigned long currentPwm) {
  if (steeringCalState == IDLE) return;
  
  switch(steeringCalState) {
    case WAIT_LEFT:
      tempCalMinPwm = currentPwm; 
      steeringCalState = WAIT_CENTER;
      calStateStartTime = millis();  // Reset timeout
      sendCalStep("OK! Now CENTER", "var(--info)");
      break;
      
    case WAIT_CENTER:
      tempCalCenterPwm = currentPwm; 
      steeringCalState = WAIT_RIGHT;
      calStateStartTime = millis();  // Reset timeout
      sendCalStep("OK! Now full RIGHT", "var(--info)");
      break;
      
    case WAIT_RIGHT:
      tempCalMaxPwm = currentPwm; 
      saveSteeringCalibration(tempCalMinPwm, tempCalCenterPwm, tempCalMaxPwm, 
                              tempCalAngleMin, tempCalAngleCenter, tempCalAngleMax, 
                              steeringReversed);
      loadSteeringCalibration();
      steeringCalState = IDLE;
      sendCalStep("Complete!", "var(--success)", true);
      break;
      
    default:
      steeringCalState = IDLE;
  }
}

void handleButtonInput() {
  static bool longPressHandled = false;
  
  if (M5.BtnA.wasClicked()) {
    if (!longPressHandled) {
      if (isLogging) stopLog();
      else startNewLog();
    }
  }
  
  if (M5.BtnA.pressedFor(1000)) {
    if (!longPressHandled) {
      sendI2CCommand(ADDR_SECONDARY, CMD_ZERO_IMU);
      sendNotification("info", "IMU zeroing");
      longPressHandled = true;
    }
  }
  
  if (M5.BtnA.wasReleased()) {
    longPressHandled = false;
  }
}

// =================================================================
// AUDIO SYNTHESIS UPDATE
// =================================================================
void updateSoundUnit() {
  AudioDataPacket packet;

  int mappedRpm = map(abs(throttlePercent), 0, 100, 800, 7000);
  
  packet.rpm = (int16_t)mappedRpm;
  packet.throttle_percent = (int16_t)throttlePercent;
  packet.speed_kph_x10 = (int16_t)(sensorData.speed_kph * 10.0);
  packet.g_force_lon_x100 = (int16_t)(sensorData.g_force_longitudinal * 100.0);
  packet.boost_psi_x100 = 0; 
  packet.gear = 1; 
  
  packet.master_volume = masterVolume;
  packet.engine_volume = engineVolume;
  packet.turbo_volume = turboVolume;
  packet.mech_volume = mechanicalVolume;
  packet.muted = audioMuted ? 1 : 0;

  packet.lpf_override_hz = user_lpf_override;
  packet.hpf_override_hz = user_hpf_override;
  packet.distortion_override_k = user_distortion_k;

  packet.event_triggers = one_shot_event_mask;
  one_shot_event_mask = 0;

  sendSoundDataPacket(packet);  
}

void triggerSoundEvent(uint8_t event_bit) {
  one_shot_event_mask |= event_bit;
}

// =================================================================
// DATA LOGGING SYSTEM (OPTIMIZED)
// =================================================================
void startNewLog() {
  if (isLogging) return;
  
  checkStorageAndRotate();
  
  closeCurrentLog();
  
  unsigned long timestamp = millis();
  
  if (LOG_FORMAT_BINARY) {
    logFileName = "/log_" + String(timestamp) + ".bin";
  } else {
    logFileName = "/log_" + String(timestamp) + ".csv";
  }
  
  logFile = LittleFS.open(logFileName, FILE_WRITE);
  if (!logFile) {
    sendNotification("error", "Cannot create log");
    return;
  }
  
  if (!LOG_FORMAT_BINARY) {
      String header = "timestamp_ms,elapsed_sec,mode,kph,mph,g_total,g_lat,g_lon,throttle_pct,steering_deg,";
      header += "roll_deg,pitch_deg,yaw_deg,yaw_rate,slip_angle,drift,jump,surface,front_traction,rear_traction,";
      header += "susp_front_mm,susp_rear_mm,pwm_thr,pwm_str,sats,hdop,heading,lat,lon,alt,gps_datetime,";
      header += "batt_volts,batt_amps,batt_mah,temp_c,gps_quality";  // Added gps_quality
      logFile.println(header);
      currentFileSize += header.length();
  }
  
  logFileOpen = true;
  isLogging = true;
  logEntryCount = 0;
  logWriteBufferOffset = 0;
  lastLogFlush_STABLE = millis();
  sessionStartTime = millis();
  currentFileSize = 0;
  
  log_m("LOG", "Started " + logFileName + (LOG_FORMAT_BINARY ? " (BIN)" : " (CSV)"));
  sendNotification("success", "Recording");
  triggerSoundEvent(EVT_TRIGGER_START);
}

void stopLog() {
  if (!isLogging) return;

  closeCurrentLog();

  isLogging = false;
  log_m("LOG", "Stopped " + String(logEntryCount) + " entries");
  sendNotification("info", "Saved " + String(logEntryCount) + " entries to " + logFileName.substring(1));
  
  printBatteryReport();
  printGPSValidationSummary();
  
  sendLoRaSummary();
  
  triggerSoundEvent(EVT_TRIGGER_STOP);
}

void deleteAllLogFiles() {
  log_m("LOG-DEL", "Attempting to delete all log files...");
  File root = LittleFS.open("/");
  if (!root) {
    log_m("ERROR", "Failed to open LittleFS root for deletion.");
    sendNotification("error", "Failed to access storage.");
    return;
  }

  File file = root.openNextFile();
  int deletedCount = 0;
  std::vector<String> filesToDelete;

  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".csv") || fileName.endsWith(".bin")) {
      filesToDelete.push_back(fileName);
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  for (const String& fileName : filesToDelete) {
    if (LittleFS.remove(fileName)) {
      log_m("LOG-DEL", "Deleted: " + fileName);
      deletedCount++;
    } else {
      log_m("ERROR", "Failed to delete: " + fileName);
    }
  }
  
  sendNotification("info", "Deleted " + String(deletedCount) + " log files.");
  log_m("LOG-DEL", "Finished deleting. Count: " + String(deletedCount));
}

void cleanupOldLogs() {
    checkStorageAndRotate();
}

void logData() {
  if (!isLogging || !logFileOpen) return;
  
  if (LOG_FORMAT_BINARY) {
      logDataBinary();
  } else {
      unsigned long now = millis();
      float elapsed_sec = (float)(now - sessionStartTime) / 1000.0f;
      
      int requiredSize = snprintf(NULL, 0,
        "%lu,%.2f,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%s,%.2f,%.2f,%.2f,%.2f,%lu,%lu,%u,%u,%u,%.6f,%.6f,%d,%lu-%lu,%.2f,%.2f,%.1f,%.1f,%s\n",
        now,
        elapsed_sec,
        getModeString(currentMode),
        sensorData.speed_kph,
        sensorData.speed_kph * 0.621371,
        sensorData.g_force_total,
        sensorData.g_force_lateral,
        sensorData.g_force_longitudinal,
        throttlePercent,
        steeringAngleDeg,
        offroad.roll_deg,
        offroad.pitch_deg,
        offroad.yaw_deg,
        offroad.yaw_rate_deg_s,
        offroad.slip_angle_deg,
        offroad.is_drifting ? 1 : 0,
        offroad.is_airborne ? 1 : 0,
        getSurfaceString(offroad.surface),
        offroad.front_traction,
        offroad.rear_traction,
        offroad.suspension_travel_front_mm,
        offroad.suspension_travel_rear_mm,
        throttlePwmPulseWidth,
        steeringPwmPulseWidth,
        sensorData.satellites,
        sensorData.hdop,
        sensorData.heading,
        sensorData.latitude,
        sensorData.longitude,
        sensorData.altitude,
        sensorData.date,
        sensorData.time,
        battery.voltage_sim,
        battery.current_amps,
        battery.consumed_mah,
        sensorData.temp_c,
        getGPSQualityString(gpsValidation.quality)  // Added
      );
      
      if (requiredSize < 0) {
        log_m("ERROR", "snprintf failed to calculate size");
        return;
      }
      
      size_t totalRequired = (size_t)(requiredSize + 1);
      
      if (LOG_WRITE_BUFFER_SIZE - logWriteBufferOffset < totalRequired) {
        flushLogBuffer();
        
        if (LOG_WRITE_BUFFER_SIZE - logWriteBufferOffset < totalRequired) {
          log_m("WARN", "Log entry too large. Entry dropped.");
          return;
        }
      }
      
      int written = snprintf(logWriteBuffer + logWriteBufferOffset, 
                            LOG_WRITE_BUFFER_SIZE - logWriteBufferOffset,
        "%lu,%.2f,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%s,%.2f,%.2f,%.2f,%.2f,%lu,%lu,%u,%u,%u,%.6f,%.6f,%d,%lu-%lu,%.2f,%.2f,%.1f,%.1f,%s\n",
        now,
        elapsed_sec,
        getModeString(currentMode),
        sensorData.speed_kph,
        sensorData.speed_kph * 0.621371,
        sensorData.g_force_total,
        sensorData.g_force_lateral,
        sensorData.g_force_longitudinal,
        throttlePercent,
        steeringAngleDeg,
        offroad.roll_deg,
        offroad.pitch_deg,
        offroad.yaw_deg,
        offroad.yaw_rate_deg_s,
        offroad.slip_angle_deg,
        offroad.is_drifting ? 1 : 0,
        offroad.is_airborne ? 1 : 0,
        getSurfaceString(offroad.surface),
        offroad.front_traction,
        offroad.rear_traction,
        offroad.suspension_travel_front_mm,
        offroad.suspension_travel_rear_mm,
        throttlePwmPulseWidth,
        steeringPwmPulseWidth,
        sensorData.satellites,
        sensorData.hdop,
        sensorData.heading,
        sensorData.latitude,
        sensorData.longitude,
        sensorData.altitude,
        sensorData.date,
        sensorData.time,
        battery.voltage_sim,
        battery.current_amps,
        battery.consumed_mah,
        sensorData.temp_c,
        getGPSQualityString(gpsValidation.quality)
      );
      
      if (written > 0 && (size_t)written < LOG_WRITE_BUFFER_SIZE - logWriteBufferOffset) {
        logWriteBufferOffset += written;
        logEntryCount++;
      }
  }
}

void logDataBinary() {
  if (!logFileOpen || !logFile) return;
  
  LogRecord record;
  record.timestamp = millis() - sessionStartTime;
  record.speed = sensorData.speed_kph;
  record.lat = sensorData.latitude;
  record.lon = sensorData.longitude;
  record.gforce_total_x100 = floatToInt16x100(sensorData.g_force_total);
  record.gforce_lat_x100 = floatToInt16x100(sensorData.g_force_lateral);
  record.gforce_lon_x100 = floatToInt16x100(sensorData.g_force_longitudinal);
  record.yaw_x10 = floatToInt16x10(sensorData.yaw_rate);
  record.sats = sensorData.satellites;
  record.hdop_x10 = floatToUInt8x10((float)sensorData.hdop / 10.0f);
  
  record.flags = 0;
  if(sensorData.gps_valid) record.flags |= (1 << 0);
  if(currentMode == MODE_OFF_ROAD) record.flags |= (1 << 1);
  if(gpsValidation.validForPerformanceClaims) record.flags |= (1 << 2);  // Store validation flag
  
  record.temp_c_x10 = (int16_t)(sensorData.temp_c * 10.0);
  record.volts_x100 = floatToUInt16x100(battery.voltage_sim);
  record.mah_used = (uint16_t)battery.consumed_mah;
  
  size_t written = logFile.write((uint8_t*)&record, sizeof(LogRecord));
  if (written == sizeof(LogRecord)) {
      currentFileSize += written;
      logEntryCount++;
  } else {
      log_m("ERROR", "Binary write failed");
  }
}

void flushLogBuffer() {
  if (logWriteBufferOffset > 0 && logFile) {
    size_t written = logFile.write((uint8_t*)logWriteBuffer, logWriteBufferOffset);
    if (written != logWriteBufferOffset) {
      log_m("ERROR", "Partial log flush: " + String(written) + "/" + String(logWriteBufferOffset));
    }
    logFile.flush();
    currentFileSize += written;
    logWriteBufferOffset = 0;
    lastLogFlush_STABLE = millis();
  }
}

String getLatestLogFile() {
  File root = LittleFS.open("/");
  if (!root) return "";
  
  String latestFile = "";
  unsigned long latestTime = 0;
  
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".csv") || fileName.endsWith(".bin")) {
      int firstUnderscore = fileName.indexOf('_');
      int secondUnderscore = fileName.lastIndexOf('.');
      
      if (firstUnderscore != -1 && secondUnderscore > firstUnderscore) {
        String tsStr = fileName.substring(firstUnderscore + 1, secondUnderscore);
        
        bool isNum = true;
        for (unsigned int i = 0; i < tsStr.length(); i++) {
          if (!isDigit(tsStr.charAt(i))) {
            isNum = false;
            break;
          }
        }
        
        if (isNum && tsStr.length() > 0) {
          unsigned long fileTime = tsStr.toInt();
          if (fileTime > latestTime) {
            latestTime = fileTime;
            latestFile = fileName;
          }
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return latestFile;
}

void convertBinaryToCSV(String binFilename) {
  if (!binFilename.startsWith("/")) binFilename = "/" + binFilename;
  
  if (!LittleFS.exists(binFilename)) {
      log_m("CONVERT", "File not found: " + binFilename);
      return;
  }
  
  String csvFilename = binFilename;
  csvFilename.replace(".bin", ".csv");
  
  File bin = LittleFS.open(binFilename, FILE_READ);
  File csv = LittleFS.open(csvFilename, FILE_WRITE);
  
  if (!bin || !csv) {
      log_m("CONVERT", "Failed to open files");
      return;
  }
  
  csv.println("timestamp,speed,lat,lon,gforce_total,gforce_lat,gforce_lon,yaw,sats,hdop,temp,volts,mah,flags,gps_valid,mode,claims_valid");
  
  LogRecord record;
  int count = 0;
  while (bin.read((uint8_t*)&record, sizeof(LogRecord)) == sizeof(LogRecord)) {
      char line[300];
      sprintf(line, "%lu,%.1f,%.6f,%.6f,%.2f,%.2f,%.2f,%.1f,%d,%.1f,%.1f,%.2f,%u,%d,%d,%d,%d",
            record.timestamp,
            record.speed,
            record.lat,
            record.lon,
            record.gforce_total_x100 / 100.0f,
            record.gforce_lat_x100 / 100.0f,
            record.gforce_lon_x100 / 100.0f,
            record.yaw_x10 / 10.0f,
            record.sats,
            record.hdop_x10 / 10.0f,
            record.temp_c_x10 / 10.0f,
            record.volts_x100 / 100.0f,
            record.mah_used,
            record.flags,
            (record.flags & 0x01) ? 1 : 0,  // GPS valid
            (record.flags & 0x02) ? 1 : 0,  // Off-road mode
            (record.flags & 0x04) ? 1 : 0); // Claims valid
      csv.println(line);
      count++;
  }
  
  bin.close();
  csv.close();
  log_m("CONVERT", "Done! Converted " + String(count) + " records.");
  sendNotification("success", "Converted to CSV: " + String(count) + " records");
}

// NEW: Corrupted log recovery
void recoverCorruptedLog(String filename) {
  if (!filename.startsWith("/")) filename = "/" + filename;
  
  if (!LittleFS.exists(filename)) {
    log_m("RECOVER", "File not found: " + filename);
    return;
  }
  
  String recoveredFilename = filename;
  recoveredFilename.replace(".bin", "_recovered.csv");
  recoveredFilename.replace(".csv", "_recovered.csv");
  
  File src = LittleFS.open(filename, FILE_READ);
  File dst = LittleFS.open(recoveredFilename, FILE_WRITE);
  
  if (!src || !dst) {
    log_m("RECOVER", "Failed to open files");
    return;
  }
  
  dst.println("timestamp,speed,lat,lon,gforce_total,sats,flags");
  
  LogRecord record;
  int recoveredCount = 0;
  int skippedCount = 0;
  
  while (src.available() >= sizeof(LogRecord)) {
    size_t bytesRead = src.read((uint8_t*)&record, sizeof(LogRecord));
    
    if (bytesRead != sizeof(LogRecord)) {
      skippedCount++;
      continue;
    }
    
    // Basic sanity checks
    if (record.speed < -5.0 || record.speed > 200.0) {
      skippedCount++;
      continue;
    }
    
    if (record.lat < -90.0 || record.lat > 90.0 || 
        record.lon < -180.0 || record.lon > 180.0) {
      skippedCount++;
      continue;
    }
    
    // Record appears valid
    char line[200];
    sprintf(line, "%lu,%.1f,%.6f,%.6f,%.2f,%d,%d",
          record.timestamp,
          record.speed,
          record.lat,
          record.lon,
          record.gforce_total_x100 / 100.0f,
          record.sats,
          record.flags);
    dst.println(line);
    recoveredCount++;
  }
  
  src.close();
  dst.close();
  
  log_m("RECOVER", "Recovery complete!");
  log_m("RECOVER", "Recovered: " + String(recoveredCount) + " records");
  log_m("RECOVER", "Skipped: " + String(skippedCount) + " corrupted records");
  sendNotification("success", "Recovered " + String(recoveredCount) + " records");
}

// =================================================================
// PHYSICS & METRICS SYSTEMS
// =================================================================
void updateOffRoadTelemetry() {
  if (abs(offroad.roll_deg) > offroad.max_roll_deg) {
    offroad.max_roll_deg = abs(offroad.roll_deg);
  }
  if (abs(offroad.pitch_deg) > offroad.max_pitch_deg) {
    offroad.max_pitch_deg = abs(offroad.pitch_deg);
  }
  if (abs(sensorData.g_force_lateral) > offroad.max_lateral_g) {
    offroad.max_lateral_g = abs(sensorData.g_force_lateral);
  }
  if (abs(sensorData.g_force_longitudinal) > offroad.max_longitudinal_g) {
    offroad.max_longitudinal_g = abs(sensorData.g_force_longitudinal);
  }
}

void updateOnRoadTelemetry() {
  // Placeholder
}

void autoDetectMode() {
  // Placeholder
}

void switchMode(DrivingMode newMode) {
  if (currentMode != newMode) {
    currentMode = newMode;
    log_m("MODE", "Switched to " + String(getModeString(newMode)));
    
    if (newMode == MODE_OFF_ROAD) {
      activeThresholds = &offRoadThresholds;
    } else {
      activeThresholds = &onRoadThresholds;
    }
  }
}

const char* getModeString(DrivingMode mode) {
  switch(mode) {
    case MODE_ON_ROAD:  return "ON-ROAD";
    case MODE_OFF_ROAD: return "OFF-ROAD";
    case MODE_AUTO:     return "AUTO";
    default:            return "UNKNOWN";
  }
}

const char* getSurfaceString(OffRoadMetrics::SurfaceType surface) {
  switch(surface) {
    case OffRoadMetrics::SURFACE_TARMAC:  return "TARMAC";
    case OffRoadMetrics::SURFACE_GRAVEL:  return "GRAVEL";
    case OffRoadMetrics::SURFACE_DIRT:    return "DIRT";
    case OffRoadMetrics::SURFACE_ROUGH:   return "ROUGH";
    default:                              return "UNKNOWN";
  }
}

// =================================================================
// SETTINGS PERSISTENCE
// =================================================================

void saveSettings() {
  File settingsFile = LittleFS.open(SETTINGS_FILE, "w");
  if (!settingsFile) return;
  
  StaticJsonDocument<768> doc;
  
  doc["masterVolume"] = masterVolume;
  doc["engineVolume"] = engineVolume;
  doc["turboVolume"] = turboVolume;
  doc["mechanicalVolume"] = mechanicalVolume;
  doc["audioMuted"] = audioMuted;
  doc["user_lpf"] = user_lpf_override;
  doc["user_hpf"] = user_hpf_override;
  doc["user_dist"] = user_distortion_k;
  
  doc["currentMode"] = currentMode;
  doc["modeAutoDetect"] = modeAutoDetect;
  
  doc["wifiPass"] = wifiPass;
  
  doc["battCap"] = battery.capacity_mah;
  doc["battCells"] = battery.cells; // NEW: Save cell count
  doc["battStatic"] = battery.static_load_amps;
  doc["battFactor"] = battery.calibration_factor;
  doc["motorType"] = battery.motor_type;
  doc["motorAmps"] = battery.peak_amps;
  doc["motorTurns"] = battery.motor_turns;
  doc["escRating"] = battery.esc_rating_amps;
  doc["idleAmps"] = battery.idle_amps;
  
  doc["ambientTemp"] = sensorData.temp_c;
  doc["tempManual"] = manualTempOverride;  // Save override state
  
  serializeJson(doc, settingsFile);
  settingsFile.close();
  log_m("SETTINGS", "Saved to flash");
}

void loadSettings() {
  if (!LittleFS.exists(SETTINGS_FILE)) return;
  File settingsFile = LittleFS.open(SETTINGS_FILE, "r");
  if (!settingsFile) return;
  
  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, settingsFile) == DeserializationError::Ok) {
    masterVolume = doc["masterVolume"] | 80;
    engineVolume = doc["engineVolume"] | 80;
    turboVolume = doc["turboVolume"] | 50;
    mechanicalVolume = doc["mechanicalVolume"] | 30;
    audioMuted = doc["audioMuted"] | false;
    user_lpf_override = doc["user_lpf"] | 0;
    user_hpf_override = doc["user_hpf"] | 0;
    user_distortion_k = doc["user_dist"] | 0;
    
    currentMode = (DrivingMode)(doc["currentMode"] | MODE_AUTO);
    modeAutoDetect = doc["modeAutoDetect"] | true;
    
    const char* pass = doc["wifiPass"];
    if(pass) wifiPass = String(pass);
    
    battery.capacity_mah = doc["battCap"] | 5000;
    battery.cells = doc["battCells"] | 2; // NEW: Load cell count (default 2)
    battery.static_load_amps = doc["battStatic"] | 0.4;
    battery.calibration_factor = doc["battFactor"] | 1.0;
    battery.motor_type = doc["motorType"] | MOTOR_BRUSHLESS;
    battery.peak_amps = doc["motorAmps"] | 60.0;
    battery.motor_turns = doc["motorTurns"] | 13.5;
    battery.esc_rating_amps = doc["escRating"] | 60;
    battery.idle_amps = doc["idleAmps"] | 0.5;
    
    sensorData.temp_c = doc["ambientTemp"] | 25.0;
    manualTempOverride = doc["tempManual"] | false;  // Load override state
    
    log_m("SETTINGS", "Loaded from flash");
  }
  settingsFile.close();
}

void saveSteeringCalibration(unsigned long minPwm, unsigned long centerPwm, unsigned long maxPwm,
                             float angleMin, float angleCenter, float angleMax, bool reversed) {
  File calFile = LittleFS.open(STEERING_CAL_FILE, "w");
  if (!calFile) return;
  
  StaticJsonDocument<256> doc;
  doc["minPwm"] = minPwm;
  doc["centerPwm"] = centerPwm;
  doc["maxPwm"] = maxPwm;
  doc["angleMin"] = angleMin;
  doc["angleCenter"] = angleCenter;
  doc["angleMax"] = angleMax;
  doc["reversed"] = reversed;
  
  serializeJson(doc, calFile);
  calFile.close();
  log_m("CAL", "Steering calibration saved");
}

void loadSteeringCalibration() {
  if (!LittleFS.exists(STEERING_CAL_FILE)) return;
  File calFile = LittleFS.open(STEERING_CAL_FILE, "r");
  if (!calFile) return;
  
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, calFile) == DeserializationError::Ok) {
    steeringMinPwm = doc["minPwm"] | 1100;
    steeringCenterPwm = doc["centerPwm"] | 1500;
    steeringMaxPwm = doc["maxPwm"] | 1900;
    steeringAngleMin = doc["angleMin"] | -30.0;
    steeringAngleCenter = doc["angleCenter"] | 0.0;
    steeringAngleMax = doc["angleMax"] | 30.0;
    steeringReversed = doc["reversed"] | false;
    
    log_m("CAL", "Steering calibration loaded");
  }
  calFile.close();
}

// =================================================================
// FILESYSTEM HEALTH CHECK
// =================================================================
void checkFS() {
  File testFile = LittleFS.open("/health.txt", FILE_WRITE);
  if (!testFile) {
    log_m("ERROR", "LittleFS corrupted! Formatting...");
    LittleFS.format();
    if(!LittleFS.begin(true)) {
      log_m("FATAL", "LittleFS format failed. Restarting...");
      ESP.restart();
    }
    saveSettings();
  } else {
    testFile.println("OK");
    testFile.close();
    LittleFS.remove("/health.txt");
  }
}

// =================================================================
// EMERGENCY SHUTDOWN HANDLER
// =================================================================
void emergencyFlushHandler() {
  if (isLogging) {
    if (!LOG_FORMAT_BINARY && logWriteBufferOffset > 0) {
        if (logFile) {
            logFile.write((uint8_t*)logWriteBuffer, logWriteBufferOffset);
            logFile.flush();
        }
    }
    if (logFile) {
      logFile.close();
    }
    log_m("EMERGENCY", "Log flushed during shutdown");
  }
}