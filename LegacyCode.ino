#include <Wire.h>
#include <U8g2lib.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <vector>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SoftWire.h>

// ========== DEBUG CONFIGURATION ==========
#define DEBUG_MODE true
#define DEBUG_PRINTLN(x) if(DEBUG_MODE) Serial.println(x)
#define DEBUG_PRINT(x) if(DEBUG_MODE) Serial.print(x)
#define DEBUG_PRINTF(...) if(DEBUG_MODE) Serial.printf(__VA_ARGS__)

// ========== PIN CONFIGURATION ==========
#define OLED_SDA_PIN 5      // Software I2C for OLED
#define OLED_SCL_PIN 6
#define MPU_SDA_PIN 8       // Hardware I2C for MPU6050
#define MPU_SCL_PIN 9
#define THROTTLE_PIN 2      // RC throttle PWM input
#define STEERING_PIN 4      // Steering potentiometer
#define BUTTON_PIN 0        // Boot button
#define LED_PIN 10          // Status LED
#define GPS_RX_PIN 20       // GPS module
#define GPS_TX_PIN 21

// ========== SENSOR CONFIGURATION ==========
SoftWire swI2C(OLED_SDA_PIN, OLED_SCL_PIN);
// CORRECTED: Constructor for the 0.42" 72x40 SH1106 OLED display
U8G2_SH1106_72X40_WISE_F_SW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);
Adafruit_MPU6050 mpu;
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// ========== NETWORK CONFIGURATION ==========
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
const char* WIFI_SSID = "RC-Telemetry-V3";
const char* WIFI_PASS = "telemetry123";

// ========== TIMING CONFIGURATION ==========
const unsigned long DISPLAY_UPDATE_INTERVAL = 100;      // 10Hz
const unsigned long SENSOR_READ_INTERVAL = 50;          // 20Hz
const unsigned long WEBSOCKET_INTERVAL = 200;           // 5Hz
const unsigned long GPS_PARSE_INTERVAL = 100;           // 10Hz
const unsigned long SESSION_TIMEOUT = 20000;            // 20 seconds
const unsigned long SENSOR_RECONNECT_INTERVAL = 5000;   // 5 seconds
const unsigned long HEALTH_CHECK_INTERVAL = 30000;      // 30 seconds
const unsigned long MARQUEE_SCROLL_INTERVAL = 200;      // Scroll speed for marquee text

// ========== GLOBAL STATE ==========
unsigned long lastDisplayUpdate = 0, lastSensorRead = 0, lastWebSocketUpdate = 0, lastGPSParse = 0, lastMemCheck = 0, lastSensorReconnectAttempt = 0;
volatile unsigned long pwmRiseTime = 0;
volatile unsigned long throttlePWM = 1500;
bool throttleConnected = false;
bool isRecording = false;
enum UnitSystem { METRIC, IMPERIAL } currentUnits = METRIC;

struct SensorStatus {
  bool connected = false;
  bool autoReconnect = true;
  String errorMsg = "N/A";
};
SensorStatus oledStatus, mpuStatus, gpsStatus;

enum DisplayMode { MODE_COMBINED, MODE_SPEED, MODE_THROTTLE, MODE_STEERING, MODE_MPU, MODE_GPS, MODE_WIFI, MODE_SESSION_SUMMARY, MODE_MARQUEE_DATA };
DisplayMode currentMode = MODE_COMBINED;
const int NUM_MODES = 9; // Updated number of modes
bool webDisplayOverride = false;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_DEBOUNCE = 200;

struct SessionData {
  bool isActive = false;
  unsigned long startTime = 0;
  unsigned long lastThrottleTime = 0;
  int sessionNumber = 1;
  float maxSpeed = 0.0, maxGTotal = 0.0, maxDeceleration = 0.0;
  int maxThrottle = 0, minThrottle = 100;
  bool showingSummary = false;
  unsigned long summaryStartTime = 0;
};
SessionData currentSession;

struct GyroCalibration {
  float offsetX = 0.0, offsetY = 0.0, offsetZ = 0.0;
  bool isCalibrated = false;
  bool isCalibrating = false;
  int calibrationSamples = 0;
  float sumX = 0.0, sumY = 0.0, sumZ = 0.0;
  const int CALIBRATION_SAMPLES = 100;
} gyroCalibration;

String marqueeText = "";
int marqueePosition = 0;
unsigned long lastMarqueeUpdate = 0;

enum BrakingType : uint8_t { BRAKE_NONE, BRAKE_GENTLE, BRAKE_MODERATE, BRAKE_HARD, BRAKE_EMERGENCY };
struct BrakingData {
  float lastSpeed = 0.0, deceleration = 0.0;
  bool isBraking = false;
  BrakingType brakingType = BRAKE_NONE;
  unsigned long lastSpeedTime = 0;
  const float GENTLE_THRESHOLD = 0.3, MODERATE_THRESHOLD = 0.6, HARD_THRESHOLD = 1.0, EMERGENCY_THRESHOLD = 1.5;
};
BrakingData brakingData;

#define FLAG_SESSION_ACTIVE 0b00000001
#define FLAG_MPU_CALIBRATED 0b00000010
#define FLAG_GPS_FIX        0b00000100
#define FLAG_THROTTLE_CONN  0b00001000

struct OptimizedDataPoint {
  uint32_t timestamp;
  float lat, lng;
  uint16_t speed_kph_x10;
  int16_t altitude_m;
  uint8_t satellites, hdop_x10;
  uint8_t throttle_percent;
  int8_t steering_angle;
  int16_t accel_x_mg, accel_y_mg, accel_z_mg;
  int16_t gyro_x_dps, gyro_y_dps, gyro_z_dps;
  int8_t temperature_c;
  uint16_t g_total_x100, deceleration_x100;
  BrakingType braking_type;
  uint8_t flags;
};
const uint16_t MAX_DATA_POINTS = 2500;
OptimizedDataPoint dataRingBuffer[MAX_DATA_POINTS];
uint16_t dataWriteIndex = 0;
uint16_t dataCount = 0;

// ========== FUNCTION PROTOTYPES ==========
void drawCombinedMode();
void drawSpeedMode();
void drawThrottleMode();
void drawSteeringMode();
void drawMPUMode();
void drawGPSMode();
void drawWiFiMode();
void drawSessionSummaryMode();
void drawMarqueeMode();
void generateSessionSummary();
void updateMarqueeText();
String getMarqueeDisplay();
void resetMarquee();
String generateCSV();
void calculateBasicBraking();
void startGyroCalibration();
void updateGyroCalibration();
void handleButton();
void updateStatusLED();
void checkSystemHealth();
void handleSerialCommands();
void printSystemInfo();
void performSelfTest();
String formatUptime();

// ========== HTML & JAVASCRIPT FOR WEB UI ==========
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>RC Telemetry Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root { --bg-grad-start: #667eea; --bg-grad-end: #764ba2; --card-bg: rgba(255, 255, 255, 0.1); --card-border: rgba(255, 255, 255, 0.2); --text-color: #fff; --text-muted: rgba(255, 255, 255, 0.7); --status-ok: #4ade80; --status-warn: #f87171; }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: linear-gradient(135deg, var(--bg-grad-start), var(--bg-grad-end)); min-height: 100vh; color: var(--text-color); }
        .container { padding: 20px; max-width: 1200px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 20px; padding: 20px; background: var(--card-bg); border-radius: 20px; border: 1px solid var(--card-border); backdrop-filter: blur(15px); }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 20px; padding: 25px; backdrop-filter: blur(15px); box-shadow: 0 8px 32px rgba(0,0,0,0.1); }
        .card h3 { margin-bottom: 20px; font-weight: 500; border-bottom: 1px solid var(--card-border); padding-bottom: 10px; }
        .metric { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; font-size: 1.1em; }
        .metric span:first-child { color: var(--text-muted); }
        .value { font-weight: 600; text-align: right; }
        .status-text { font-size: 0.9em; opacity: 0.8; }
        .status-dot { width: 12px; height: 12px; border-radius: 50%; display: inline-block; margin-right: 8px; flex-shrink: 0; }
        .online .status-dot { background: var(--status-ok); box-shadow: 0 0 8px var(--status-ok); }
        .offline .status-dot { background: var(--status-warn); }
        .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 15px; }
        .button { background: rgba(255,255,255,0.15); color: var(--text-color); border: 1px solid var(--card-border); padding: 10px 15px; border-radius: 10px; cursor: pointer; flex-grow: 1; text-align: center; transition: background 0.2s, color 0.2s; }
        .button:hover { background: rgba(255,255,255,0.25); }
        .button:disabled { opacity: 0.5; cursor: not-allowed; }
        .button.active { background: var(--status-ok); color: #111; }
        .danger { background: rgba(248, 113, 113, 0.2); }
        .danger:hover { background: rgba(248, 113, 113, 0.4); }
        .danger.active { background: var(--status-warn); color: #111; }
        .throttle-bar-wrapper { background: rgba(0,0,0,0.2); height: 30px; border-radius: 15px; position: relative; overflow: hidden; }
        .throttle-bar { height: 100%; width: 0; border-radius: 15px; transition: width 0.1s linear; position: absolute; }
        .throttle-bar.throttle { background: linear-gradient(90deg, #4ade80, #22d3ee); left: 50%; }
        .throttle-bar.braking { background: linear-gradient(90deg, #f87171, #fb923c); right: 50%; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header"><h1>RC Telemetry Dashboard</h1></div>
        <div class="grid">
            <div class="card">
                <h3>System Status</h3>
                <div class="metric" id="status-oled"><span>OLED Display</span><span class="value offline"><span class="status-dot"></span><span class="status-text">Offline</span></span></div>
                <div class="metric" id="status-mpu"><span>MPU6050</span><span class="value offline"><span class="status-dot"></span><span class="status-text">Offline</span></span></div>
                <div class="metric" id="status-gps"><span>GPS</span><span class="value offline"><span class="status-dot"></span><span class="status-text">Offline</span></span></div>
                <div class="metric"><span>Web Clients</span><span class="value" id="ws-clients">0</span></div>
            </div>
            <div class="card">
                <h3>Core Telemetry</h3>
                <div class="metric"><span>Speed</span><span class="value" id="speed">-- kph</span></div>
                <div class="metric"><span>G-Force</span><span class="value" id="gforce">-- g</span></div>
                <div class="metric"><span>Satellites</span><span class="value" id="satellites">--</span></div>
            </div>
            <div class="card">
                <h3>Controls</h3>
                <div class="metric"><span>Throttle</span><span class="value" id="throttle">-- %</span></div>
                <div class="throttle-bar-wrapper"><div id="throttleBar" class="throttle-bar"></div></div>
                <div class="metric" style="margin-top: 15px;"><span>Steering</span><span class="value" id="steering">-- &deg;</span></div>
            </div>
            <div class="card">
                <h3>Session</h3>
                <div class="metric"><span>Status</span><span class="value" id="sessionStatus">Inactive</span></div>
                <div class="metric"><span>Max Speed</span><span class="value" id="maxSpeed">-- kph</span></div>
                <div class="metric"><span>Data Points</span><span class="value" id="dataPoints">0</span></div>
            </div>
            <div class="card">
                <h3>System Control</h3>
                <div class="button-group">
                    <button class="button danger" id="recordBtn" onclick="sendCommand({command:'toggleRecording'})">Start Recording</button>
                    <button class="button" id="unitsBtn" onclick="sendCommand({command:'toggleUnits'})">Switch to MPH</button>
                </div>
                <div class="button-group">
                    <button class="button" id="calibrateBtn" onclick="sendCommand({command:'calibrateGyro'})" disabled>Calibrate Gyro</button>
                    <button class="button" onclick="sendCommand({command:'reconnectAll'})">Reconnect All</button>
                </div>
                <div class="button-group">
                    <button class="button" onclick="window.location.href='/downloadCSV'">Download CSV</button>
                    <button class="button danger" onclick="if(confirm('Clear all data?')) sendCommand({command:'clearData'})">Clear Data</button>
                </div>
            </div>
            <div class="card">
                <h3>OLED Display Control</h3>
                <div class="button-group">
                    <button class="button" onclick="setDisplayMode(0)">Combined</button>
                    <button class="button" onclick="setDisplayMode(1)">Speed</button>
                    <button class="button" onclick="setDisplayMode(2)">Throttle</button>
                    <button class="button" onclick="setDisplayMode(3)">Steering</button>
                </div>
                 <div class="button-group">
                    <button class="button" onclick="setDisplayMode(4)">MPU</button>
                    <button class="button" onclick="setDisplayMode(5)">GPS</button>
                    <button class="button" onclick="setDisplayMode(6)">WiFi</button>
                    <button class="button" onclick="setDisplayMode(7)">Session</button>
                </div>
                <div class="button-group">
                    <button class="button" onclick="setDisplayMode(8)">Marquee</button>
                    <button class="button danger" onclick="sendCommand({command:'releaseDisplay'})">Release Control</button>
                </div>
            </div>
        </div>
    </div>
    <script>
        const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
        let currentUnits = 'metric';

        ws.onmessage = event => {
            const data = JSON.parse(event.data);
            currentUnits = data.units || 'metric';

            const speedKph = parseFloat(data.speed);
            const maxSpeedKph = parseFloat(data.maxSpeed);
            if (currentUnits === 'imperial') {
                updateMetric('speed', (speedKph * 0.621371).toFixed(1), ' mph');
                updateMetric('maxSpeed', (maxSpeedKph * 0.621371).toFixed(1), ' mph');
            } else {
                updateMetric('speed', speedKph.toFixed(1), ' kph');
                updateMetric('maxSpeed', maxSpeedKph.toFixed(1), ' kph');
            }

            updateMetric('gforce', data.gforce, ' g');
            updateMetric('satellites', data.satellites);
            updateMetric('throttle', data.throttle, ' %');
            updateMetric('steering', data.steering, ' &deg;');
            updateMetric('ws-clients', data.clients);
            updateMetric('sessionStatus', data.sessionActive ? `Active (#${data.sessionNumber})` : 'Inactive');
            updateMetric('dataPoints', data.dataPoints);
            
            updateStatus('oled', data.sensors.oled);
            updateStatus('mpu', data.sensors.mpu);
            updateStatus('gps', data.sensors.gps);
            document.getElementById('calibrateBtn').disabled = !data.sensors.mpu.connected;

            const recordBtn = document.getElementById('recordBtn');
            recordBtn.textContent = data.isRecording ? 'Stop Recording' : 'Start Recording';
            recordBtn.classList.toggle('active', data.isRecording);
            
            const unitsBtn = document.getElementById('unitsBtn');
            unitsBtn.textContent = currentUnits === 'metric' ? 'Switch to MPH' : 'Switch to KPH';

            const throttleBar = document.getElementById('throttleBar');
            throttleBar.className = 'throttle-bar';
            if (data.isBraking) {
                throttleBar.classList.add('braking');
                throttleBar.style.width = Math.min(parseFloat(data.deceleration) * 50, 50) + '%';
            } else if (data.throttle > 5) {
                throttleBar.classList.add('throttle');
                throttleBar.style.width = (data.throttle / 2) + '%';
            }
        };

        function updateMetric(id, value, unit = '') { const el = document.getElementById(id); if(el) el.innerHTML = (value !== undefined ? value : '--') + unit; }
        
        function updateStatus(id, sensor) {
            const el = document.getElementById(`status-${id}`);
            if(el) {
                const textEl = el.querySelector('.status-text');
                el.className = `value ${sensor.connected ? 'online' : 'offline'}`;
                if(textEl) textEl.textContent = sensor.connected ? 'Online' : `Offline (${sensor.errorMsg})`;
            }
        }
        function sendCommand(payload) { ws.send(JSON.stringify(payload)); }
        function setDisplayMode(mode) { sendCommand({ command: 'setDisplayMode', mode: mode }); }
    </script>
</body>
</html>
)rawliteral";

// ========== INTERRUPT HANDLER ==========
void IRAM_ATTR throttleISR() {
  unsigned long currentMicros = micros();
  if (digitalRead(THROTTLE_PIN) == HIGH) {
    pwmRiseTime = currentMicros;
  } else if (pwmRiseTime > 0) {
    unsigned long width = currentMicros - pwmRiseTime;
    if (width >= 800 && width <= 2200) {
      throttlePWM = width;
      throttleConnected = true;
    }
    pwmRiseTime = 0;
  }
}

// ========== CORE SETUP & LOOP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  DEBUG_PRINTLN("\n======================================");
  DEBUG_PRINTLN("  RC Telemetry - Robust Startup");
  DEBUG_PRINTLN("======================================");

  setupPins();
  setupDisplay();
  setupSensors();
  setupWiFi();
  setupWebServer();

  DEBUG_PRINTLN("\nSetup complete. System running.");
  DEBUG_PRINTF("Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());
  DEBUG_PRINTLN("======================================");
}

void loop() {
  unsigned long now = millis();
  handleButton();
  handleSerialCommands();

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    updateSensors();
    updateSession();
    if (gyroCalibration.isCalibrating) updateGyroCalibration();
    addDataPoint();
    lastSensorRead = now;
  }
  
  if (oledStatus.connected && (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  if (now - lastWebSocketUpdate >= WEBSOCKET_INTERVAL) {
    sendWebSocketData();
    ws.cleanupClients();
    lastWebSocketUpdate = now;
  }
  
  if (now - lastGPSParse >= GPS_PARSE_INTERVAL) {
    while (gpsSerial.available() > 0) {
      if (gps.encode(gpsSerial.read())) {
        if (!gpsStatus.connected) DEBUG_PRINTLN("GPS data link established.");
        gpsStatus.connected = true;
        gpsStatus.errorMsg = "Active";
      }
    }
    if (gps.time.isUpdated() && gps.time.age() > 5000 && gpsStatus.connected) {
      DEBUG_PRINTLN("GPS data timeout.");
      gpsStatus.connected = false;
      gpsStatus.errorMsg = "Timeout";
    }
    lastGPSParse = now;
  }
  
  if (now - lastSensorReconnectAttempt >= SENSOR_RECONNECT_INTERVAL) {
    attemptSensorReconnections();
    lastSensorReconnectAttempt = now;
  }
  
  checkSystemHealth();
  updateStatusLED();
  updateMarqueeText();
  delay(1);
}

// ========== INITIALIZATION FUNCTIONS ==========
void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(THROTTLE_PIN, INPUT);
  pinMode(STEERING_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(THROTTLE_PIN), throttleISR, CHANGE);
  DEBUG_PRINTLN("Pins configured.");
}

void setupDisplay() {
  DEBUG_PRINT("Initializing OLED... ");
  swI2C.begin();
  swI2C.setClock(400000);
  if (u8g2.begin()) {
    oledStatus.connected = true;
    oledStatus.errorMsg = "Active";
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 10, "System Online");
    u8g2.sendBuffer();
    DEBUG_PRINTLN("OK");
  } else {
    oledStatus.connected = false;
    oledStatus.errorMsg = "Init Fail";
    DEBUG_PRINTLN("FAILED");
  }
}

void setupSensors() {
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN, 100000);
  DEBUG_PRINT("Initializing MPU6050... ");
  if (mpu.begin(0x68, &Wire)) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpuStatus.connected = true;
    mpuStatus.errorMsg = "Active";
    DEBUG_PRINTLN("OK");
  } else {
    mpuStatus.connected = false;
    mpuStatus.errorMsg = "Not Found";
    DEBUG_PRINTLN("FAILED");
  }
  
  DEBUG_PRINT("Initializing GPS... ");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gpsStatus.errorMsg = "No Data";
  DEBUG_PRINTLN("OK (waiting for data)");
}

void setupWiFi() {
  DEBUG_PRINT("Starting WiFi AP... ");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  DEBUG_PRINT("IP: ");
  DEBUG_PRINTLN(WiFi.softAPIP());
}

void setupWebServer() {
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/downloadCSV", HTTP_GET, [](AsyncWebServerRequest *request) {
    String csv = generateCSV();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
    response->addHeader("Content-Disposition", "attachment; filename=telemetry_log.csv");
    request->send(response);
  });
  server.begin();
  DEBUG_PRINTLN("Web server started.");
}

// ========== SENSOR & STATE MANAGEMENT ==========
void updateSensors() {
  static unsigned long lastThrottleSignal = 0;
  if (throttleConnected) lastThrottleSignal = millis();
  if (millis() - lastThrottleSignal > 500) {
    throttleConnected = false;
    throttlePWM = 1500;
  }
  if (mpuStatus.connected) {
    sensors_event_t a, g, temp;
    if (!mpu.getEvent(&a, &g, &temp)) {
      DEBUG_PRINTLN("MPU read failed!");
      mpuStatus.connected = false;
      mpuStatus.errorMsg = "Read Fail";
    }
  }
  if (gpsStatus.connected && gps.speed.isValid()) {
    calculateBasicBraking();
  } else {
    brakingData.isBraking = false;
    brakingData.brakingType = BRAKE_NONE;
    brakingData.deceleration = 0.0;
  }
}

void attemptSensorReconnections() {
  if (!oledStatus.connected && oledStatus.autoReconnect) {
    DEBUG_PRINT("Attempting OLED reconnect... ");
    if (u8g2.begin()) { oledStatus.connected = true; oledStatus.errorMsg = "Active"; DEBUG_PRINTLN("OK"); }
    else { DEBUG_PRINTLN("FAIL"); }
  }
  if (!mpuStatus.connected && mpuStatus.autoReconnect) {
    DEBUG_PRINT("Attempting MPU reconnect... ");
    if (mpu.begin(0x68, &Wire)) { mpuStatus.connected = true; mpuStatus.errorMsg = "Active"; DEBUG_PRINTLN("OK"); }
    else { DEBUG_PRINTLN("FAIL"); }
  }
  if (!gpsStatus.connected && gpsStatus.autoReconnect) {
    DEBUG_PRINTLN("Restarting GPS serial to listen for data...");
    gpsSerial.end();
    delay(100);
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsStatus.errorMsg = "No Data";
  }
}

void updateSession() {
  unsigned long now = millis();
  int throttlePercent = map(throttlePWM, 1000, 2000, 0, 100);
  if (throttlePercent > 5) {
    if (!currentSession.isActive) {
      currentSession = {true, now, now, currentSession.sessionNumber}; // Start new session
      DEBUG_PRINTF("Session %d started.\n", currentSession.sessionNumber);
    }
    currentSession.lastThrottleTime = now;
    if (gpsStatus.connected && gps.speed.isValid()) currentSession.maxSpeed = max(currentSession.maxSpeed, (float)gps.speed.kmph());
    if(mpuStatus.connected) {
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      currentSession.maxGTotal = max(currentSession.maxGTotal, (float)(sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)) / 9.81));
    }
    currentSession.maxDeceleration = max(currentSession.maxDeceleration, brakingData.deceleration);

  }
  if (currentSession.isActive && (now - currentSession.lastThrottleTime > SESSION_TIMEOUT)) {
    DEBUG_PRINTF("Session %d ended due to inactivity.\n", currentSession.sessionNumber);
    currentSession.isActive = false;
    generateSessionSummary(); // Generate summary text for marquee
    if (!webDisplayOverride) {
      currentMode = MODE_SESSION_SUMMARY;
    }
    currentSession.sessionNumber++;
  }
}

// ========== WEBSOCKET & DATA HANDLING ==========
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) { DEBUG_PRINTF("WS client #%u connected\n", client->id()); }
    if (type == WS_EVT_DISCONNECT) { DEBUG_PRINTF("WS client #%u disconnected\n", client->id()); }
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            DynamicJsonDocument doc(256);
            if (deserializeJson(doc, (char*)data).code() == DeserializationError::Ok) {
                if (doc.containsKey("command")) {
                    const char* command = doc["command"];
                    if (strcmp(command, "toggleRecording") == 0) {
                        isRecording = !isRecording;
                        DEBUG_PRINTLN(isRecording ? "Recording STARTED" : "Recording STOPPED");
                    } else if (strcmp(command, "toggleUnits") == 0) {
                        currentUnits = (currentUnits == METRIC) ? IMPERIAL : METRIC;
                    } else if (strcmp(command, "calibrateGyro") == 0) {
                        startGyroCalibration();
                    } else if (strcmp(command, "reconnectAll") == 0) {
                        attemptSensorReconnections();
                    } else if (strcmp(command, "clearData") == 0) {
                        dataCount = 0; dataWriteIndex = 0;
                        DEBUG_PRINTLN("Data cleared via web.");
                    } else if (strcmp(command, "setDisplayMode") == 0) {
                        int mode = doc["mode"];
                        if (mode >= 0 && mode < NUM_MODES) {
                            currentMode = (DisplayMode)mode;
                            webDisplayOverride = true;
                        }
                    } else if (strcmp(command, "releaseDisplay") == 0) {
                        webDisplayOverride = false;
                    }
                }
            }
        }
    }
}

void sendWebSocketData() {
  if (ws.count() == 0) return;
  DynamicJsonDocument doc(1024);
  doc["clients"] = ws.count();
  doc["speed"] = (gpsStatus.connected && gps.speed.isValid()) ? String(gps.speed.kmph(), 1) : "0.0";
  doc["throttle"] = map(throttlePWM, 1000, 2000, 0, 100);
  doc["steering"] = map(analogRead(STEERING_PIN), 0, 4095, -100, 100);
  doc["satellites"] = gps.satellites.isValid() ? gps.satellites.value() : 0;
  if (mpuStatus.connected) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    doc["gforce"] = String(sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)) / 9.81, 2);
  } else {
    doc["gforce"] = "0.00";
  }
  doc["isBraking"] = brakingData.isBraking;
  doc["deceleration"] = String(brakingData.deceleration, 2);
  doc["sessionActive"] = currentSession.isActive;
  doc["sessionNumber"] = currentSession.sessionNumber;
  doc["maxSpeed"] = String(currentSession.maxSpeed, 1);
  doc["dataPoints"] = dataCount;
  doc["isRecording"] = isRecording;
  doc["units"] = (currentUnits == METRIC) ? "metric" : "imperial";

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["oled"]["connected"] = oledStatus.connected;
  sensors["oled"]["errorMsg"] = oledStatus.errorMsg;
  sensors["mpu"]["connected"] = mpuStatus.connected;
  sensors["mpu"]["errorMsg"] = mpuStatus.errorMsg;
  sensors["gps"]["connected"] = gpsStatus.connected;
  sensors["gps"]["errorMsg"] = gpsStatus.errorMsg;

  String jsonString;
  serializeJson(doc, jsonString);
  ws.textAll(jsonString);
}

// ========== DATA LOGGING & ANALYSIS ==========
void addDataPoint() {
  if (!isRecording) return; 
  if (dataCount >= MAX_DATA_POINTS) {
    dataWriteIndex = (dataWriteIndex + 1) % MAX_DATA_POINTS;
  } else {
    dataWriteIndex = dataCount;
    dataCount++;
  }

  OptimizedDataPoint& point = dataRingBuffer[dataWriteIndex];
  point.timestamp = millis();
  
  if (gpsStatus.connected) {
    point.lat = gps.location.lat();
    point.lng = gps.location.lng();
    point.speed_kph_x10 = gps.speed.kmph() * 10;
    point.altitude_m = gps.altitude.meters();
    point.satellites = gps.satellites.value();
    point.hdop_x10 = gps.hdop.hdop() * 10;
  } else {
    memset(&point.lat, 0, sizeof(float) * 2 + sizeof(uint16_t) + sizeof(int16_t) + sizeof(uint8_t) * 2);
  }

  point.throttle_percent = map(throttlePWM, 1000, 2000, 0, 100);
  point.steering_angle = map(analogRead(STEERING_PIN), 0, 4095, -100, 100);

  if (mpuStatus.connected) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    point.accel_x_mg = a.acceleration.x * 101.97;
    point.accel_y_mg = a.acceleration.y * 101.97;
    point.accel_z_mg = a.acceleration.z * 101.97;
    point.gyro_x_dps = g.gyro.x * 57.2958;
    point.gyro_y_dps = g.gyro.y * 57.2958;
    point.gyro_z_dps = g.gyro.z * 57.2958;
    point.temperature_c = temp.temperature;
    point.g_total_x100 = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)) / 9.81 * 100;
  } else {
    memset(&point.accel_x_mg, 0, sizeof(int16_t) * 6 + sizeof(int8_t) + sizeof(uint16_t));
  }

  point.deceleration_x100 = brakingData.deceleration * 100;
  point.braking_type = brakingData.brakingType;
  point.flags = 0;
  if (currentSession.isActive) point.flags |= FLAG_SESSION_ACTIVE;
  if (gyroCalibration.isCalibrated) point.flags |= FLAG_MPU_CALIBRATED;
  if (gpsStatus.connected && gps.location.isValid()) point.flags |= FLAG_GPS_FIX;
  if (throttleConnected) point.flags |= FLAG_THROTTLE_CONN;
}

String generateCSV() {
  String csv = "timestamp,lat,lng,speed_kph,altitude_m,satellites,throttle_pct,steering_angle,g_total,deceleration_g\n";
  uint16_t readIndex = (dataCount < MAX_DATA_POINTS) ? 0 : (dataWriteIndex + 1) % MAX_DATA_POINTS;
  for (uint16_t i = 0; i < dataCount; i++) {
    const OptimizedDataPoint& p = dataRingBuffer[readIndex];
    csv += String(p.timestamp) + ",";
    csv += String(p.lat, 6) + "," + String(p.lng, 6) + ",";
    csv += String(p.speed_kph_x10 / 10.0f, 1) + ",";
    csv += String(p.altitude_m) + "," + String(p.satellites) + ",";
    csv += String(p.throttle_percent) + "," + String(p.steering_angle) + ",";
    csv += String(p.g_total_x100 / 100.0f, 2) + ",";
    csv += String(p.deceleration_x100 / 100.0f, 2) + "\n";
    readIndex = (readIndex + 1) % MAX_DATA_POINTS;
  }
  return csv;
}

void calculateBasicBraking() {
  unsigned long now = millis();
  if (brakingData.lastSpeedTime > 0) {
    float timeDiff = (now - brakingData.lastSpeedTime) / 1000.0f;
    if (timeDiff > 0.01) {
      float currentSpeed = gps.speed.kmph();
      float speedDiff = brakingData.lastSpeed - currentSpeed;
      if (speedDiff > 0.5) {
        brakingData.deceleration = (speedDiff / 3.6f) / timeDiff / 9.81f;
      } else {
        brakingData.deceleration = 0.0;
      }
    }
  }
  brakingData.lastSpeed = gps.speed.kmph();
  brakingData.lastSpeedTime = now;

  if (brakingData.deceleration > brakingData.GENTLE_THRESHOLD) {
    brakingData.isBraking = true;
    if (brakingData.deceleration > brakingData.EMERGENCY_THRESHOLD) brakingData.brakingType = BRAKE_EMERGENCY;
    else if (brakingData.deceleration > brakingData.HARD_THRESHOLD) brakingData.brakingType = BRAKE_HARD;
    else if (brakingData.deceleration > brakingData.MODERATE_THRESHOLD) brakingData.brakingType = BRAKE_MODERATE;
    else brakingData.brakingType = BRAKE_GENTLE;
  } else {
    brakingData.isBraking = false;
    brakingData.brakingType = BRAKE_NONE;
  }
}

// ========== GYRO CALIBRATION ==========
void startGyroCalibration() {
  if (!mpuStatus.connected) { 
    DEBUG_PRINTLN("Can't calibrate: MPU offline."); 
    return; 
  }
  DEBUG_PRINTLN("Starting gyro calibration... Keep vehicle still!");
  gyroCalibration.offsetX = 0.0;
  gyroCalibration.offsetY = 0.0;
  gyroCalibration.offsetZ = 0.0;
  gyroCalibration.isCalibrated = false;
  gyroCalibration.isCalibrating = true;
  gyroCalibration.calibrationSamples = 0;
  gyroCalibration.sumX = 0.0;
  gyroCalibration.sumY = 0.0;
  gyroCalibration.sumZ = 0.0;
}

void updateGyroCalibration() {
  if (!gyroCalibration.isCalibrating || !mpuStatus.connected) { 
    gyroCalibration.isCalibrating = false; 
    return; 
  }
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    gyroCalibration.sumX += g.gyro.x;
    gyroCalibration.sumY += g.gyro.y;
    gyroCalibration.sumZ += g.gyro.z;
    gyroCalibration.calibrationSamples++;
    if (gyroCalibration.calibrationSamples >= gyroCalibration.CALIBRATION_SAMPLES) {
      gyroCalibration.offsetX = gyroCalibration.sumX / gyroCalibration.calibrationSamples;
      gyroCalibration.offsetY = gyroCalibration.sumY / gyroCalibration.calibrationSamples;
      gyroCalibration.offsetZ = gyroCalibration.sumZ / gyroCalibration.calibrationSamples;
      gyroCalibration.isCalibrated = true;
      gyroCalibration.isCalibrating = false;
      DEBUG_PRINTLN("Gyro calibration complete.");
    }
  }
}

// ========== DISPLAY & UTILITY FUNCTIONS ==========
void updateDisplay() {
  u8g2.clearBuffer();
  switch(currentMode) {
    case MODE_COMBINED: drawCombinedMode(); break;
    case MODE_SPEED: drawSpeedMode(); break;
    case MODE_THROTTLE: drawThrottleMode(); break;
    case MODE_STEERING: drawSteeringMode(); break;
    case MODE_MPU: drawMPUMode(); break;
    case MODE_GPS: drawGPSMode(); break;
    case MODE_WIFI: drawWiFiMode(); break;
    case MODE_SESSION_SUMMARY: drawSessionSummaryMode(); break;
    case MODE_MARQUEE_DATA: drawMarqueeMode(); break;
  }
  u8g2.sendBuffer();
}

void drawCombinedMode() {
    u8g2.setFont(u8g2_font_5x7_tr);
    char buf[20];

    if (isRecording) {
        u8g2.drawStr(0, 7, "REC");
    }

    // Line 1: Speed
    if (gpsStatus.connected && gps.speed.isValid()) {
        sprintf(buf, "S:%.1f kph", gps.speed.kmph());
    } else {
        strcpy(buf, "S: -- kph");
    }
    u8g2.drawStr(18, 7, buf);

    // Line 2: Throttle & G-Force
    sprintf(buf, "T:%3d%%", map(throttlePWM, 1000, 2000, 0, 100));
    u8g2.drawStr(0, 17, buf);

    if (mpuStatus.connected) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        sprintf(buf, "G:%.2f", sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)) / 9.81);
    } else {
        strcpy(buf, "G: N/A");
    }
    u8g2.drawStr(36, 17, buf);

    // Line 3: Session
    if (currentSession.isActive) {
        sprintf(buf, "Session %d", currentSession.sessionNumber);
    } else {
        strcpy(buf, "Session Idle");
    }
    u8g2.drawStr(0, 27, buf);

    // Line 4: Status Icons
    u8g2.drawStr(0, 37, "GPS:");
    u8g2.drawStr(24, 37, "MPU:");
    u8g2.drawStr(50, 37, "WIFI:");
    u8g2.drawStr(18, 37, gpsStatus.connected ? "OK" : "-");
    u8g2.drawStr(44, 37, mpuStatus.connected ? "OK" : "-");
    u8g2.drawStr(70, 37, WiFi.softAPgetStationNum() > 0 ? "OK" : "-");
}

void drawSpeedMode() {
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 10, "Speed");
    if (gpsStatus.connected && gps.speed.isValid()) {
        u8g2.setFont(u8g2_font_logisoso16_tr);
        char buf[10];
        sprintf(buf, "%.1f", gps.speed.kmph());
        u8g2.drawStr(0, 35, buf);
        u8g2.setFont(u8g2_font_profont10_tr);
        u8g2.drawStr(45, 35, "kph");
    } else {
        u8g2.setFont(u8g2_font_profont12_tr);
        u8g2.drawStr(10, 30, "No GPS");
    }
}

void drawThrottleMode() {
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 10, "Throttle");
    u8g2.setFont(u8g2_font_logisoso16_tr);
    char buf[10];
    sprintf(buf, "%d%%", map(throttlePWM, 1000, 2000, 0, 100));
    u8g2.drawStr(0, 35, buf);
}

void drawSteeringMode() {
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 10, "Steering");
    u8g2.setFont(u8g2_font_logisoso16_tr);
    char buf[10];
    sprintf(buf, "%d", map(analogRead(STEERING_PIN), 0, 4095, -100, 100));
    u8g2.drawStr(0, 35, buf);
}

void drawMPUMode() {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 7, "MPU6050");
    if (mpuStatus.connected) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        char buf[20];
        sprintf(buf, "AX:%.1f AY:%.1f", a.acceleration.x, a.acceleration.y);
        u8g2.drawStr(0, 17, buf);
        sprintf(buf, "AZ:%.1f", a.acceleration.z);
        u8g2.drawStr(0, 27, buf);
        sprintf(buf, "T:%.1fC", temp.temperature);
        u8g2.drawStr(0, 37, buf);
    } else {
        u8g2.setFont(u8g2_font_profont12_tr);
        u8g2.drawStr(10, 30, "Offline");
    }
}

void drawGPSMode() {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 7, "GPS Status");
    if (gpsStatus.connected && gps.location.isValid()) {
        char buf[30];
        sprintf(buf, "Lat: %.3f", gps.location.lat());
        u8g2.drawStr(0, 17, buf);
        sprintf(buf, "Lng: %.3f", gps.location.lng());
        u8g2.drawStr(0, 27, buf);
        sprintf(buf, "Sats:%d Alt:%.0fm", gps.satellites.value(), gps.altitude.meters());
        u8g2.drawStr(0, 37, buf);
    } else {
        u8g2.setFont(u8g2_font_profont12_tr);
        u8g2.drawStr(10, 30, "No Signal");
    }
}

void drawWiFiMode() {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 7, "WiFi Status");
    u8g2.drawStr(0, 17, WIFI_SSID);
    u8g2.drawStr(0, 27, WiFi.softAPIP().toString().c_str());
    char buf[20];
    sprintf(buf, "Clients: %d", ws.count());
    u8g2.drawStr(0, 37, buf);
}

void drawSessionSummaryMode() {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 7, "Session Summary");
    char buf[30];
    sprintf(buf, "Top Speed: %.1f kph", currentSession.maxSpeed);
    u8g2.drawStr(0, 17, buf);
    sprintf(buf, "Max G-Force: %.2f g", currentSession.maxGTotal);
    u8g2.drawStr(0, 27, buf);
    sprintf(buf, "Max Decel: %.2f g", currentSession.maxDeceleration);
    u8g2.drawStr(0, 37, buf);
}

void drawMarqueeMode() {
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 10, "Session Log");
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.drawStr(0, 35, getMarqueeDisplay().c_str());
}

void generateSessionSummary() {
  char buffer[80];
  marqueeText = "S" + String(currentSession.sessionNumber -1) + ": ";
  sprintf(buffer, "Spd:%.1f|", currentSession.maxSpeed);
  marqueeText += buffer;
  sprintf(buffer, "G:%.2f|", currentSession.maxGTotal);
  marqueeText += buffer;
  sprintf(buffer, "Dec:%.2f", currentSession.maxDeceleration);
  marqueeText += buffer;
  resetMarquee();
}

void updateMarqueeText() {
  if (marqueeText.length() == 0) return;
  if (millis() - lastMarqueeUpdate > MARQUEE_SCROLL_INTERVAL) {
    marqueePosition++;
    if (marqueePosition >= marqueeText.length()) {
      marqueePosition = 0;
    }
    lastMarqueeUpdate = millis();
  }
}

String getMarqueeDisplay() {
  if (marqueeText.length() == 0) return "No Data";
  const int displayWidth = 14; // Chars for 5x7 font on 72px width
  if (marqueeText.length() <= displayWidth) return marqueeText;
  
  String display = marqueeText.substring(marqueePosition);
  if (display.length() < displayWidth) {
    display += " | " + marqueeText.substring(0, displayWidth - display.length() - 3);
  }
  return display.substring(0, displayWidth);
}

void resetMarquee() {
  marqueePosition = 0;
  lastMarqueeUpdate = millis();
}


void handleButton() {
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > BUTTON_DEBOUNCE) {
    lastButtonPress = millis();
    if (!webDisplayOverride) {
      currentMode = (DisplayMode)((currentMode + 1) % NUM_MODES);
      DEBUG_PRINTF("Display mode changed to %d\n", currentMode);
    }
  }
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  unsigned long interval = 1000;
  if (isRecording) interval = 100; // Fast blink for recording
  else if (currentSession.isActive) interval = 250;
  else if (ws.count() > 0) interval = 500;
  
  if (millis() - lastBlink > interval) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

void checkSystemHealth() {
  if (millis() - lastMemCheck > HEALTH_CHECK_INTERVAL) {
    DEBUG_PRINTF("Free heap: %d bytes\n", ESP.getFreeHeap());
    lastMemCheck = millis();
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    DEBUG_PRINTF("Serial command: '%s'\n", cmd.c_str());
    if (cmd == "status") printSystemInfo();
    else if (cmd == "test") performSelfTest();
    else if (cmd == "reconnect") attemptSensorReconnections();
    else if (cmd == "help") {
      DEBUG_PRINTLN("Commands: status, test, reconnect, help");
    }
  }
}

void printSystemInfo() {
  DEBUG_PRINTLN("--- System Status ---");
  DEBUG_PRINTF("Uptime: %s\n", formatUptime().c_str());
  DEBUG_PRINTF("Heap: %d bytes free\n", ESP.getFreeHeap());
  DEBUG_PRINTF("Data Points: %d/%d\n", dataCount, MAX_DATA_POINTS);
  DEBUG_PRINTF("OLED: %s, MPU: %s, GPS: %s\n", 
    oledStatus.connected ? "OK" : "FAIL", 
    mpuStatus.connected ? "OK" : "FAIL", 
    gpsStatus.connected ? "OK" : "FAIL");
  DEBUG_PRINTF("Web Clients: %d\n", ws.count());
  DEBUG_PRINTLN("---------------------");
}

void performSelfTest() {
  DEBUG_PRINTLN("--- Performing Self Test ---");
  bool pass = true;
  if (!oledStatus.connected) { DEBUG_PRINTLN("OLED: FAIL"); pass = false; }
  if (!mpuStatus.connected) { DEBUG_PRINTLN("MPU6050: FAIL"); pass = false; }
  if (pass) { DEBUG_PRINTLN("All critical sensors OK."); }
  printSystemInfo();
}

String formatUptime() {
  unsigned long s = millis() / 1000;
  char buf[12];
  sprintf(buf, "%02lu:%02lu:%02lu", s / 3600, (s % 3600) / 60, s % 60);
  return String(buf);
}
