// ===================================================
//  AtomS3 Secondary - v0.19.1(I2C FIXED)
//  - FIXED: ESP32 I2C  mode initialization
//  - Double buffered display (no flicker)
//  - GPS Time/Date + Temperature
// ===================================================
#include <M5Unified.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <EEPROM.h>
#include <esp_task_wdt.h>

// ========== UI & COLOR DEFINITIONS ==========
#define COLOR_BG 0x0000
#define COLOR_PRIMARY 0x07FF
#define COLOR_ACCENT 0xFFE0
#define COLOR_DANGER 0xF800
#define COLOR_SUCCESS 0x07E0
#define COLOR_TEXT 0xFFFF
#define COLOR_MUTED 0x8410
#define COLOR_BAR 0x2104
#define COLOR_WARNING 0xFD20
#define COLOR_INFO 0x001F

// ========== CONFIGURATION ==========
#define FW_VERSION "v0.19.3-I2CFIX"
#define G_FORCE_MAX_VISUAL 3.0f
#define SPEED_MAX_VISUAL 150.0f
#define G_FORCE_ALERT_THRESHOLD 2.0f
#define YAW_RATE_MAX_VISUAL 180.0f
#define NUM_MENUS 8
#define MARQUEE_SPEED 2
#define MARQUEE_PAUSE 1000
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128

// ========== PIN & I2C CONFIGURATION ==========
#define I2C_SECONDARY_ADDRESS 0x55
#define GROVE_SDA_PIN 2
#define GROVE_SCL_PIN 1
#define GPS_RX_PIN 5
#define GPS_TX_PIN 6
#define GPS_BAUD_RATE 115200

// ========== EEPROM & WDT CONFIGURATION ==========
#define MAX_RUNS 8
#define EEPROM_SIZE (MAX_RUNS * sizeof(float) + sizeof(uint32_t) + 1)
#define EEPROM_VALID_MARKER 0xA5
#define EEPROM_RECORDS_ADDR 1
#define EEPROM_BOOT_COUNT_ADDR (EEPROM_RECORDS_ADDR + MAX_RUNS * sizeof(float))
#define WDT_TIMEOUT_S 3
#define EEPROM_SAVE_DELAY 30000
#define SPEED_EPSILON 0.1f
#define KMH_TO_MPH 0.621371f

// ========== GLOBAL OBJECTS ==========
HardwareSerial& gpsSerial = Serial1;
TinyGPSPlus gps;
M5Canvas canvas(&M5.Display);

// ========== DATA STRUCTURES ==========
struct CompactTelemetry {
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
} __attribute__((packed)) i2c_data;

struct MarqueeText {
  String text;
  int x, y, width;
  int scroll_pos;
  unsigned long last_update;
  bool needs_scroll;
  int pause_counter;
};

struct IMUCalibration {
  float gyro_offset_x = 0;
  float gyro_offset_y = 0;
  float gyro_offset_z = 0;
  float accel_offset_x = 0;
  float accel_offset_y = 0;
  float accel_offset_z = 0;
  bool calibrated = false;
  int samples_collected = 0;
};

// ========== GLOBAL VARIABLES ==========
MarqueeText marquees[10];
int marquee_count = 0;

volatile bool isLogging = false;
volatile bool sessionActive = false;

bool unitsMetric = true;
bool imuConnected = false;
IMUCalibration imu_cal;
float yaw_rate_filtered = 0;
float yaw_rate_raw = 0;
const float YAW_FILTER_ALPHA = 0.1;

volatile int i2c_receive_count = 0;
volatile int i2c_request_count = 0;
volatile unsigned long last_i2c_activity = 0;

float topSpeeds[MAX_RUNS] = {0};
bool isMakingRun = false;
float currentRunMaxSpeed = 0.0f;
bool topSpeedsNeedSave = false;
unsigned long lastEEPROMSave = 0;

int currentMenu = 0;
int previousMenu = 0;
uint8_t pressCount = 0;
unsigned long lastPressTime = 0;
bool menuTransition = false;
const int doublePressTimeout = 300;
const int longPressTimeout = 1500;
unsigned long actionFeedbackTime = 0;
String actionFeedbackText = "";
uint16_t actionFeedbackColor = COLOR_SUCCESS;
float animationPhase = 0;

// ========== I2C COMMANDS ==========
enum I2CCommands {
  CMD_START_LOG = 0x10,
  CMD_STOP_LOG = 0x11,
  CMD_TOGGLE_UNITS = 0x12,
  CMD_RESET_SESSION = 0x13,
  CMD_CLEAR_MAX = 0x14,
  CMD_ZERO_IMU = 0x15,
  CMD_GET_TELEMETRY = 0x30
};

// ========== FUNCTION PROTOTYPES ==========
void drawIcon(const char* icon, int x, int y, uint16_t color, float scale = 1.0);
void showActionFeedback(const String& text, uint16_t color = COLOR_SUCCESS);
void drawActionFeedback();
void resetMarquees();
void drawMarqueeText(const String& text, int x, int y, int max_width, uint16_t color = COLOR_TEXT);
void drawHorizontalBar(int x, int y, int w, int h, float pct, uint16_t col);
void drawMenuHeader(const char* title, const char* icon, int x_offset = 0);
void drawMenuSpeed(int x_offset = 0);
void drawMenuCockpit(int x_offset = 0);
void drawMenuStats(int x_offset = 0);
void drawMenuGPS(int x_offset = 0);
void drawMenuI2C(int x_offset = 0);
void drawMenuSettings(int x_offset = 0);
void drawMenuSystemInfo(int x_offset = 0);
void drawMenuRecords(int x_offset = 0);
void drawSplashScreen();
void animateMenuTransition();
void receiveEvent(int numBytes);
void requestEvent();
void updateSensorData();
void handleButton();
void calibrateIMU();
void zeroIMU();
void saveTopSpeeds();
void loadTopSpeeds();
void logTopSpeed(float newSpeed);
float displaySpeed(float kmh);
String speedUnit();

// ========== EEPROM FUNCTIONS ==========
void saveTopSpeeds() {
  Serial.println("[EEPROM] Saving...");
  EEPROM.write(0, EEPROM_VALID_MARKER);
  for (int i = 0; i < MAX_RUNS; i++) {
    EEPROM.put(EEPROM_RECORDS_ADDR + (i * sizeof(float)), topSpeeds[i]);
  }
  EEPROM.commit();
}

void loadTopSpeeds() {
  if (EEPROM.read(0) == EEPROM_VALID_MARKER) {
    for (int i = 0; i < MAX_RUNS; i++) {
      EEPROM.get(EEPROM_RECORDS_ADDR + (i * sizeof(float)), topSpeeds[i]);
    }
  } else {
    for (int i = 0; i < MAX_RUNS; i++) topSpeeds[i] = 0.0f;
    saveTopSpeeds();
  }
}

void logTopSpeed(float newSpeed) {
  if (newSpeed < 5.0) return;
  int insertIndex = -1;
  for (int i = 0; i < MAX_RUNS; i++) {
    if (newSpeed > topSpeeds[i]) {
      insertIndex = i;
      break;
    }
  }
  if (insertIndex != -1) {
    for (int j = MAX_RUNS - 1; j > insertIndex; j--) {
      topSpeeds[j] = topSpeeds[j-1];
    }
    topSpeeds[insertIndex] = newSpeed;
    topSpeedsNeedSave = true;
  }
}

float displaySpeed(float kmh) { return unitsMetric ? kmh : (kmh * KMH_TO_MPH); }
String speedUnit() { return unitsMetric ? "km/h" : "mph"; }

void drawHorizontalBar(int x, int y, int w, int h, float pct, uint16_t col) {
  pct = constrain(pct, -1.0, 1.0);
  canvas.fillRoundRect(x, y, w, h, 3, COLOR_BAR);
  canvas.drawFastVLine(x + w / 2, y, h, COLOR_MUTED);
  if (pct > 0.01) {
    canvas.fillRoundRect(x + w / 2, y, w / 2 * pct, h, 2, col);
  } else if (pct < -0.01) {
    int bar_w = w / 2 * abs(pct);
    canvas.fillRoundRect(x + w / 2 - bar_w, y, bar_w, h, 2, col);
  }
}

void drawIcon(const char* icon, int x, int y, uint16_t color, float scale) {
  if (strcmp(icon, "SPEED") == 0) {
    int r = 6 * scale; canvas.drawCircle(x, y, r, color); canvas.drawCircle(x, y, r-1, color);
    float angle = -2.35 + (animationPhase * 0.1); int nx = x + cos(angle) * (r-2); int ny = y + sin(angle) * (r-2);
    canvas.drawLine(x, y, nx, ny, color); canvas.fillCircle(x, y, 2 * scale, color);
    for (int i = 0; i < 5; i++) { float a = -2.35 + (i * 1.17); int mx = x + cos(a) * r; int my = y + sin(a) * r; canvas.drawPixel(mx, my, color); }
  } else if (strcmp(icon, "GPS") == 0) {
    canvas.drawCircle(x, y, 5 * scale, color); canvas.drawLine(x-7*scale, y-7*scale, x+7*scale, y+7*scale, color);
    canvas.drawLine(x-5*scale, y, x+5*scale, y, color); canvas.fillCircle(x+5*scale, y-5*scale, 2*scale, color);
    if (millis() % 1000 < 500) { canvas.drawCircle(x+5*scale, y-5*scale, 4*scale, color); }
  } else if (strcmp(icon, "RECORD") == 0) {
    int r = 5 * scale; if (millis() % 1000 < 500) { canvas.fillCircle(x, y, r, COLOR_DANGER); canvas.drawCircle(x, y, r+2, COLOR_DANGER); } else { canvas.fillCircle(x, y, r-1, COLOR_DANGER); }
  } else if (strcmp(icon, "LINK") == 0) {
    int s = 3 * scale; canvas.drawCircle(x-s, y, s, color); canvas.drawCircle(x+s, y, s, color);
    canvas.drawLine(x-1, y-1, x+1, y-1, color); canvas.drawLine(x-1, y, x+1, y, color); canvas.drawLine(x-1, y+1, x+1, y+1, color);
  } else if (strcmp(icon, "CHECK") == 0) {
    int r = 6 * scale; canvas.drawCircle(x, y, r, color); canvas.drawLine(x-3*scale, y, x-1*scale, y+3*scale, color); canvas.drawLine(x-1*scale, y+3*scale, x+4*scale, y-2*scale, color);
  } else if (strcmp(icon, "ALERT") == 0) {
    int s = 6 * scale; canvas.drawTriangle(x, y-s, x-s, y+s-1, x+s, y+s-1, color); canvas.drawTriangle(x, y-s+1, x-s+1, y+s-2, x+s-1, y+s-2, color);
    canvas.drawLine(x, y-2, x, y+2, color); canvas.fillCircle(x, y+4, 1, color);
  } else if (strcmp(icon, "GEAR") == 0) {
    int r = 5 * scale; canvas.drawCircle(x, y, r, color); canvas.drawCircle(x, y, r/2, color);
    for (int i = 0; i < 8; i++) { float angle = (i * PI / 4) + (animationPhase * 0.05); int px = x + cos(angle) * (r+2); int py = y + sin(angle) * (r+2); canvas.fillCircle(px, py, 1, color); }
  } else if (strcmp(icon, "STAT_GAUGE") == 0) {
    canvas.drawCircle(x, y, 8*scale, color); canvas.drawLine(x, y-8*scale, x, y-6*scale, color); canvas.drawLine(x+8*scale, y, x+6*scale, y, color);
    canvas.drawLine(x, y+8*scale, x, y+6*scale, color); canvas.drawLine(x-8*scale, y, x-6*scale, y, color); canvas.fillCircle(x, y, 2*scale, COLOR_DANGER);
  } else if (strcmp(icon, "YAW") == 0) {
    int r = 6 * scale;
    float angle = (yaw_rate_filtered / YAW_RATE_MAX_VISUAL) * PI * 2;
    canvas.drawCircle(x, y, r, color);
    int ax = x + cos(angle) * (r-1);
    int ay = y + sin(angle) * (r-1);
    canvas.drawLine(x, y, ax, ay, color);
    canvas.fillCircle(x, y, 2, color);
  } else if (strcmp(icon, "TROPHY") == 0) {
    int s = 6 * scale;
    canvas.drawLine(x-s, y-s, x-s, y, color);
    canvas.drawLine(x+s, y-s, x+s, y, color);
    canvas.drawLine(x-s/2, y-s, x+s/2, y-s, color);
    canvas.fillRect(x-2, y, 4, s, color);
    canvas.fillRect(x-4, y+s, 8, 2, color);
  } else if (strcmp(icon, "GAUGE") == 0) {
    canvas.drawRect(x-5*scale, y-5*scale, 10*scale, 10*scale, color);
    canvas.drawLine(x, y, x+4*scale, y-4*scale, color);
    canvas.drawCircle(x, y, 2*scale, color);
  } else if (strcmp(icon, "SAT_BARS") == 0) {
    int s = 1 * scale;
    int h1 = 2 * scale; int h2 = 4 * scale; int h3 = 6 * scale; int h4 = 8 * scale;
    int w = 2 * scale; int y_base = y + 4*scale;
    uint16_t c1 = (i2c_data.sats >= 4) ? color : COLOR_MUTED;
    uint16_t c2 = (i2c_data.sats >= 6) ? color : COLOR_MUTED;
    uint16_t c3 = (i2c_data.sats >= 8) ? color : COLOR_MUTED;
    uint16_t c4 = (i2c_data.sats >= 10) ? color : COLOR_MUTED;
    canvas.fillRect(x - 5*s, y_base - h1, w, h1, c1);
    canvas.fillRect(x - 2*s, y_base - h2, w, h2, c2);
    canvas.fillRect(x + 1*s, y_base - h3, w, h3, c3);
    canvas.fillRect(x + 4*s, y_base - h4, w, h4, c4);
  }
}

void calibrateIMU() {
  Serial.println("[IMU] Calibrating...");
  imu_cal.samples_collected = 0;
  imu_cal.gyro_offset_x = 0; imu_cal.gyro_offset_y = 0; imu_cal.gyro_offset_z = 0;
  imu_cal.accel_offset_x = 0; imu_cal.accel_offset_y = 0; imu_cal.accel_offset_z = 0;
  imu_cal.calibrated = false;
  showActionFeedback("CAL START", COLOR_WARNING);
  
  const int samples = 100;
  for (int i = 0; i < samples; i++) {
    esp_task_wdt_reset();
    if (M5.Imu.update()) {
      auto data = M5.Imu.getImuData();
      if (!isnan(data.accel.x) && !isnan(data.gyro.z)) {
        imu_cal.gyro_offset_x += data.gyro.x;
        imu_cal.gyro_offset_y += data.gyro.y;
        imu_cal.gyro_offset_z += data.gyro.z;
        imu_cal.accel_offset_x += data.accel.x;
        imu_cal.accel_offset_y += data.accel.y;
        imu_cal.accel_offset_z += (data.accel.z - 1.0);
        imu_cal.samples_collected++;
      }
    }
    delay(10);
  }
  
  if (imu_cal.samples_collected > 50) {
    imu_cal.gyro_offset_x /= imu_cal.samples_collected;
    imu_cal.gyro_offset_y /= imu_cal.samples_collected;
    imu_cal.gyro_offset_z /= imu_cal.samples_collected;
    imu_cal.accel_offset_x /= imu_cal.samples_collected;
    imu_cal.accel_offset_y /= imu_cal.samples_collected;
    imu_cal.accel_offset_z /= imu_cal.samples_collected;
    imu_cal.calibrated = true;
    Serial.println("[IMU] Cal complete!");
    showActionFeedback("CAL DONE", COLOR_SUCCESS);
  } else {
    Serial.println("[IMU] Cal failed");
    showActionFeedback("CAL FAIL", COLOR_DANGER);
  }
}

void zeroIMU() { calibrateIMU(); }

void showActionFeedback(const String& text, uint16_t color) {
  actionFeedbackText = text;
  actionFeedbackColor = color;
  actionFeedbackTime = millis();
}

void drawActionFeedback() {
  if (millis() - actionFeedbackTime < 2000 && actionFeedbackText.length() > 0) {
    canvas.fillRoundRect(20, 102, 88, 20, 4, COLOR_BG);
    canvas.drawRoundRect(20, 102, 88, 20, 4, actionFeedbackColor);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(actionFeedbackColor);
    canvas.drawString(actionFeedbackText, 64, 112);
  }
}

void resetMarquees() { marquee_count = 0; }

void drawMarqueeText(const String& text, int x, int y, int max_width, uint16_t color) {
  int text_width = canvas.textWidth(text);
  if (text_width <= max_width) {
    canvas.setTextColor(color);
    canvas.drawString(text, x, y);
    return;
  }
  MarqueeText* m = nullptr;
  for (int i = 0; i < marquee_count; i++) {
    if (marquees[i].x == x && marquees[i].y == y) {
      m = &marquees[i];
      break;
    }
  }
  if (!m && marquee_count < 10) {
    m = &marquees[marquee_count++];
    m->x = x; m->y = y; m->width = max_width; m->scroll_pos = 0;
    m->last_update = millis(); m->pause_counter = MARQUEE_PAUSE;
  }
  if (m) {
    m->text = text + "   "; m->needs_scroll = true;
    if (millis() - m->last_update > 50) {
      if (m->pause_counter > 0) { m->pause_counter -= 50; }
      else {
        m->scroll_pos += MARQUEE_SPEED;
        if (m->scroll_pos > canvas.textWidth(m->text)) {
          m->scroll_pos = 0;
          m->pause_counter = MARQUEE_PAUSE;
        }
      }
      m->last_update = millis();
    }
    canvas.setClipRect(x, y - 8, max_width, 16);
    canvas.setTextColor(color);
    canvas.drawString(m->text, x - m->scroll_pos, y);
    canvas.drawString(m->text, x - m->scroll_pos + canvas.textWidth(m->text), y);
    canvas.clearClipRect();
  }
}

void drawMenuHeader(const char* title, const char* icon, int x_offset) {
  for (int i = 0; i < 20; i++) {
    uint16_t gradColor = canvas.color565(0, 100 - i*3, 150 - i*4);
    canvas.drawFastHLine(x_offset, i, DISPLAY_WIDTH, gradColor);
  }
  drawIcon(icon, 12 + x_offset, 10, COLOR_TEXT);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(COLOR_TEXT);
  canvas.drawString(title, 64 + x_offset, 10);
}

void drawMenuSpeed(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("SPEED", "SPEED", x_offset);
  tft.setTextDatum(MC_DATUM);
  tft.setFont(&fonts::Font7);
  tft.setTextColor(COLOR_ACCENT);
  String speedStr = String(displaySpeed(i2c_data.speed), 1);
  tft.drawString(speedStr, 64 + x_offset, 60);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString(speedUnit(), 64 + x_offset, 90);
  tft.setFont(&fonts::Font0);
  uint16_t gpsColor = i2c_data.sats > 6 ? COLOR_SUCCESS : (i2c_data.sats > 3 ? COLOR_WARNING : COLOR_DANGER);
  tft.setTextColor(gpsColor);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("GPS:" + String(i2c_data.sats), 5 + x_offset, 115);
  uint16_t gColor = i2c_data.gforce_total > G_FORCE_ALERT_THRESHOLD ? COLOR_DANGER : COLOR_SUCCESS;
  tft.setTextColor(gColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(i2c_data.gforce_total, 1) + "G", 64 + x_offset, 115);
  bool logging;
  noInterrupts();
  logging = isLogging;
  interrupts();
  if (logging) { 
    tft.setTextColor(COLOR_DANGER);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("REC", 123 + x_offset, 115);
  }
  if (i2c_data.gforce_total > G_FORCE_ALERT_THRESHOLD) {
    if (millis() % 500 < 250) {
      tft.drawRoundRect(2 + x_offset, 2, 124, 124, 8, COLOR_DANGER);
      tft.drawRoundRect(3 + x_offset, 3, 122, 122, 7, COLOR_DANGER);
    }
  }
  drawActionFeedback();
}

void drawMenuCockpit(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("COCKPIT", "GAUGE", x_offset);
  int topY = 28;
  uint16_t gpsColor = i2c_data.sats > 6 ? COLOR_SUCCESS : (i2c_data.sats > 3 ? COLOR_WARNING : COLOR_DANGER);
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SAT", 5 + x_offset, topY);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(gpsColor);
  tft.drawNumber(i2c_data.sats, 5 + x_offset, topY + 10);
  tft.setFont(&fonts::Font4);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_ACCENT);
  tft.drawString(String(displaySpeed(i2c_data.speed), 0), 64 + x_offset, topY + 12);
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString(speedUnit(), 64 + x_offset, topY + 24);
  tft.setTextColor(COLOR_MUTED);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("G", 123 + x_offset, topY);
  uint16_t gTotalColor = i2c_data.gforce_total > G_FORCE_ALERT_THRESHOLD ? COLOR_DANGER : COLOR_SUCCESS;
  tft.setFont(&fonts::Font2);
  tft.setTextColor(gTotalColor);
  tft.drawString(String(i2c_data.gforce_total, 1), 123 + x_offset, topY + 10);
  int barY = 65;
  int barHeight = 10;
  int barWidth = 90;
  int barX = 30;
  int labelX = 6;
  int valueX = 122;
  int spacing = 20;
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("LAT", labelX + x_offset, barY + barHeight/2);
  uint16_t latGColor = abs(i2c_data.gforce_lat) > G_FORCE_ALERT_THRESHOLD ? COLOR_DANGER : COLOR_PRIMARY;
  drawHorizontalBar(barX + x_offset, barY, barWidth, barHeight, i2c_data.gforce_lat / G_FORCE_MAX_VISUAL, latGColor);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(latGColor);
  tft.drawString(String(i2c_data.gforce_lat, 1), valueX + x_offset, barY + barHeight/2);
  barY += spacing;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("YAW", labelX + x_offset, barY + barHeight/2);
  drawHorizontalBar(barX + x_offset, barY, barWidth, barHeight, yaw_rate_filtered / YAW_RATE_MAX_VISUAL, COLOR_INFO);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(COLOR_INFO);
  tft.drawString(String(yaw_rate_filtered, 0), valueX + x_offset, barY + barHeight/2);
  unsigned long i2c_time_since;
  noInterrupts();
  i2c_time_since = millis() - last_i2c_activity;
  interrupts();
  bool i2c_ok = i2c_time_since < 2000;
  tft.setFont(&fonts::Font0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(i2c_ok ? COLOR_SUCCESS : COLOR_DANGER);
  tft.drawString(i2c_ok ? "LINK OK" : "NO LINK", 64 + x_offset, 115);
  bool logging;
  noInterrupts();
  logging = isLogging;
  interrupts();
  if (logging) drawIcon("RECORD", 5 + x_offset, 115, COLOR_DANGER, 0.5);
  drawActionFeedback();
}

void drawMenuStats(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("STATISTICS", "CHECK", x_offset);
  tft.setTextDatum(TL_DATUM);
  bool active;
  noInterrupts();
  active = sessionActive;
  interrupts();
  if (active) { 
    drawIcon("CHECK", 110 + x_offset, 28, COLOR_SUCCESS); 
    tft.setTextColor(COLOR_SUCCESS); 
    tft.drawString("Active", 70 + x_offset, 25);
  } else { 
    tft.setTextColor(COLOR_MUTED); 
    tft.drawString("Inactive", 70 + x_offset, 25); 
  }
  drawIcon("SPEED", 10 + x_offset, 45, COLOR_ACCENT);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Max Speed", 25 + x_offset, 40);
  tft.setTextColor(COLOR_ACCENT);
  tft.setFont(&fonts::Font2);
  String speedStr = String(displaySpeed(i2c_data.maxSpeed), 1) + " " + speedUnit();
  drawMarqueeText(speedStr, 25 + x_offset, 52, 90, COLOR_ACCENT);
  float speedPct = constrain(i2c_data.maxSpeed / SPEED_MAX_VISUAL, 0, 1);
  tft.fillRoundRect(10 + x_offset, 65, 108, 6, 3, COLOR_BAR);
  tft.fillRoundRect(10 + x_offset, 65, 108 * speedPct, 6, 3, COLOR_ACCENT);
  tft.setFont(&fonts::Font0);
  uint16_t gColor = i2c_data.maxG > G_FORCE_ALERT_THRESHOLD ? COLOR_WARNING : COLOR_SUCCESS;
  drawIcon("STAT_GAUGE", 10 + x_offset, 82, gColor);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Max G-Force", 25 + x_offset, 77);
  tft.setTextColor(gColor);
  tft.setFont(&fonts::Font2);
  String gStr = String(i2c_data.maxG, 2) + " G";
  drawMarqueeText(gStr, 25 + x_offset, 89, 90, gColor);
  float gPct = constrain(i2c_data.maxG / G_FORCE_MAX_VISUAL, 0, 1);
  tft.fillRoundRect(10 + x_offset, 102, 108, 6, 3, COLOR_BAR);
  tft.fillRoundRect(10 + x_offset, 102, 108 * gPct, 6, 3, gColor);
  tft.setFont(&fonts::Font0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Press to reset", 64 + x_offset, 118);
}

void drawMenuGPS(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("GPS INFO", "GPS", x_offset);
  bool gps_valid = gps.location.isValid() && gps.location.age() < 2000;
  tft.setTextDatum(TL_DATUM);
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Latitude:", 8 + x_offset, 35);
  String latStr = gps_valid ? String(gps.location.lat(), 6) : "No Fix";
  drawMarqueeText(latStr, 8 + x_offset, 45, 112, gps_valid ? COLOR_PRIMARY : COLOR_MUTED);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Longitude:", 8 + x_offset, 58);
  String lonStr = gps_valid ? String(gps.location.lng(), 6) : "No Fix";
  drawMarqueeText(lonStr, 8 + x_offset, 68, 112, gps_valid ? COLOR_PRIMARY : COLOR_MUTED);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Altitude:", 8 + x_offset, 81);
  tft.setTextColor(gps_valid ? COLOR_PRIMARY : COLOR_MUTED);
  String altStr = gps_valid ? String(gps.altitude.meters(), 1) + " m" : "---";
  tft.drawString(altStr, 60 + x_offset, 81);
  uint16_t gpsColor = i2c_data.sats > 6 ? COLOR_SUCCESS : (i2c_data.sats > 3 ? COLOR_WARNING : COLOR_DANGER);
  drawIcon("SAT_BARS", 20 + x_offset, 105, gpsColor, 2.0);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(gpsColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(i2c_data.sats), 45 + x_offset, 110);
  tft.setFont(&fonts::Font0);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("HDOP:", 70 + x_offset, 100);
  float hdop = gps.hdop.value() / 100.0;
  uint16_t hdopColor = hdop < 1.5 ? COLOR_SUCCESS : (hdop < 3.0 ? COLOR_WARNING : COLOR_DANGER);
  tft.setTextColor(hdopColor);
  tft.drawFloat(hdop, 2, 70 + x_offset, 110);
}

void drawMenuI2C(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("I2C STATUS", "LINK", x_offset);
  unsigned long timeSince;
  int rx_count, tx_count;
  noInterrupts();
  timeSince = millis() - last_i2c_activity;
  rx_count = i2c_receive_count;
  tx_count = i2c_request_count;
  interrupts();
  bool connected = (timeSince < 2000);
  drawIcon(connected ? "CHECK" : "ALERT", 110 + x_offset, 28, connected ? COLOR_SUCCESS : COLOR_DANGER);
  tft.setTextDatum(TL_DATUM);
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Received:", 8 + x_offset, 35);
  tft.setTextColor(COLOR_PRIMARY);
  tft.drawNumber(rx_count, 65 + x_offset, 35);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Sent:", 8 + x_offset, 48);
  tft.setTextColor(COLOR_PRIMARY);
  tft.drawNumber(tx_count, 65 + x_offset, 48);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Address:", 8 + x_offset, 61);
  tft.setTextColor(COLOR_INFO);
  String addr = String(I2C_SECONDARY_ADDRESS, HEX); addr.toUpperCase();
  tft.drawString("0x" + addr, 60 + x_offset, 61);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(connected ? COLOR_SUCCESS : COLOR_DANGER);
  tft.drawString(connected ? "CONNECTED" : "DISCONNECTED", 64 + x_offset, 105);
}

void drawMenuSettings(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("SETTINGS", "GEAR", x_offset);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Units:", 15 + x_offset, 35);
  int tx = 35 + x_offset, ty = 50, tw = 58, th = 20;
  uint16_t switchColor = unitsMetric ? COLOR_INFO : COLOR_ACCENT;
  tft.fillRoundRect(tx, ty, tw, th, 10, switchColor);
  tft.fillCircle(unitsMetric ? tx + tw - 12 : tx + 12, ty + 10, 8, COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.setFont(&fonts::Font2);
  tft.setTextColor(switchColor);
  tft.drawString(unitsMetric ? "METRIC" : "IMPERIAL", 64 + x_offset, 80);
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString(unitsMetric ? "km/h, meters" : "mph, feet", 64 + x_offset, 95);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("IMU Cal:", 15 + x_offset, 105);
  tft.setTextColor(imu_cal.calibrated ? COLOR_SUCCESS : COLOR_WARNING);
  tft.drawString(imu_cal.calibrated ? "Done" : "Needed", 70 + x_offset, 105);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_MUTED);
  tft.drawString("Press: Units / Hold: Cal", 64 + x_offset, 118);
}

void drawMenuSystemInfo(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("SYSTEM", "GEAR", x_offset);
  tft.setTextDatum(TL_DATUM);
  tft.setFont(&fonts::Font0);
  int y = 30; int spacing = 18;
  tft.setTextColor(COLOR_MUTED); tft.drawString("Firmware:", 8 + x_offset, y);
  tft.setTextColor(COLOR_PRIMARY); tft.drawString(FW_VERSION, 75 + x_offset, y);
  y += spacing;
  unsigned long up = millis() / 1000;
  String uptime = String(up / 3600) + "h " + String((up % 3600) / 60) + "m";
  tft.setTextColor(COLOR_MUTED); tft.drawString("Uptime:", 8 + x_offset, y);
  tft.setTextColor(COLOR_PRIMARY); tft.drawString(uptime, 75 + x_offset, y);
  y += spacing;
  uint32_t freeHeap = ESP.getFreeHeap();
  bool heapLow = freeHeap < 30000;
  tft.setTextColor(COLOR_MUTED); tft.drawString("Memory:", 8 + x_offset, y);
  tft.setTextColor(heapLow ? COLOR_WARNING : COLOR_SUCCESS);
  tft.drawString(String(freeHeap / 1024) + " KB", 75 + x_offset, y);
  y += spacing;
  tft.setTextColor(COLOR_MUTED); tft.drawString("IMU:", 8 + x_offset, y);
  tft.setTextColor(imuConnected ? COLOR_SUCCESS : COLOR_DANGER);
  tft.drawString(imuConnected ? "Connected" : "Error", 75 + x_offset, y);
}

void drawMenuRecords(int x_offset) {
  auto& tft = canvas;
  resetMarquees();
  drawMenuHeader("TOP SPEEDS", "TROPHY", x_offset);
  tft.setFont(&fonts::Font0);
  tft.setTextDatum(TL_DATUM);
  bool hasRecords = false;
  for (int i = 0; i < MAX_RUNS; i++) {
    if (topSpeeds[i] > 0.1) {
      hasRecords = true;
      break;
    }
  }
  if (!hasRecords) {
    tft.setFont(&fonts::Font2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_MUTED);
    tft.drawString("No records", 64 + x_offset, 64);
    return;
  }
  int y = 30;
  for (int i = 0; i < MAX_RUNS && i < 6; i++) {
    if (topSpeeds[i] > 0.1) {
      uint16_t color = (i == 0) ? COLOR_ACCENT : 
                       (i == 1) ? COLOR_PRIMARY : 
                       (i == 2) ? COLOR_SUCCESS : COLOR_TEXT;
      tft.setTextColor(color);
      char buf[32];
      snprintf(buf, sizeof(buf), "%d. %.1f %s", i + 1, 
               displaySpeed(topSpeeds[i]), speedUnit().c_str());
      tft.drawString(buf, 5 + x_offset, y);
      y += 12;
    }
  }
  tft.setFont(&fonts::Font0);
  tft.setTextColor(COLOR_MUTED);
  tft.setTextDatum(BL_DATUM);
  tft.drawString("Long press to clear", 5 + x_offset, 123);
}

void drawSplashScreen() {
  canvas.fillScreen(COLOR_BG);
  drawIcon("SPEED", 64, 40, COLOR_ACCENT, 2.0);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(COLOR_TEXT, COLOR_BG);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("SENSOR HUB", 64, 75);
  canvas.setTextColor(COLOR_MUTED, COLOR_BG);
  canvas.setFont(&fonts::Font0);
  canvas.drawString(FW_VERSION, 64, 95);
  canvas.pushSprite(0, 0);
  delay(2000);
}

void animateMenuTransition() {
  int direction = (currentMenu > previousMenu || (currentMenu == 0 && previousMenu == NUM_MENUS - 1)) ? 1 : -1;
  if (abs(currentMenu - previousMenu) > 1) direction *= -1;
  for (int i = 0; i <= 128; i += 16) {
    canvas.fillScreen(COLOR_BG);
    int offset_old = -i * direction;
    int offset_new = (128 - i) * direction;
    switch (previousMenu) {
      case 0: drawMenuSpeed(offset_old); break;
      case 1: drawMenuCockpit(offset_old); break;
      case 2: drawMenuStats(offset_old); break;
      case 3: drawMenuGPS(offset_old); break;
      case 4: drawMenuI2C(offset_old); break;
      case 5: drawMenuSettings(offset_old); break;
      case 6: drawMenuSystemInfo(offset_old); break;
      case 7: drawMenuRecords(offset_old); break;
    }
    switch (currentMenu) {
      case 0: drawMenuSpeed(offset_new); break;
      case 1: drawMenuCockpit(offset_new); break;
      case 2: drawMenuStats(offset_new); break;
      case 3: drawMenuGPS(offset_new); break;
      case 4: drawMenuI2C(offset_new); break;
      case 5: drawMenuSettings(offset_new); break;
      case 6: drawMenuSystemInfo(offset_new); break;
      case 7: drawMenuRecords(offset_new); break;
    }
    canvas.pushSprite(0, 0);
  }
  resetMarquees();
}

void receiveEvent(int numBytes) {
  noInterrupts();
  i2c_receive_count++;
  last_i2c_activity = millis();
  interrupts();
  if (numBytes == 0) return;
  uint8_t command = Wire.read();
  switch(command) {
    case CMD_START_LOG:
      noInterrupts(); isLogging = true; interrupts();
      showActionFeedback("REC ON", COLOR_SUCCESS);
      break;
    case CMD_STOP_LOG:
      noInterrupts(); isLogging = false; interrupts();
      showActionFeedback("REC OFF", COLOR_WARNING);
      break;
    case CMD_TOGGLE_UNITS:
      unitsMetric = !unitsMetric;
      showActionFeedback(unitsMetric ? "METRIC" : "IMPERIAL", COLOR_INFO);
      break;
    case CMD_RESET_SESSION:
      noInterrupts(); sessionActive = false; interrupts();
      i2c_data.maxSpeed = 0;
      i2c_data.maxG = 0;
      showActionFeedback("SESSION RESET", COLOR_WARNING);
      break;
    case CMD_CLEAR_MAX:
      i2c_data.maxSpeed = 0;
      i2c_data.maxG = 0;
      showActionFeedback("MAX CLEARED", COLOR_INFO);
      break;
    case CMD_ZERO_IMU:
      zeroIMU();
      break;
  }
  while(Wire.available()) Wire.read();
}

void requestEvent() {
  noInterrupts();
  i2c_request_count++;
  last_i2c_activity = millis();
  i2c_data.flags = 0;
  if (isLogging) i2c_data.flags |= (1 << 0);
  if (gps.location.isValid() && gps.location.age() < 2000) i2c_data.flags |= (1 << 1);
  if (sessionActive) i2c_data.flags |= (1 << 2);
  if (!unitsMetric) i2c_data.flags |= (1 << 3);
  if (imuConnected) i2c_data.flags |= (1 << 4);
  interrupts();
  Wire.write((uint8_t*)&i2c_data, sizeof(CompactTelemetry));
}

void updateSensorData() {
  if (gps.location.isValid() && gps.location.age() < 2000) {
    i2c_data.lat = gps.location.lat();
    i2c_data.lon = gps.location.lng();
    i2c_data.speed = gps.speed.kmph();
    i2c_data.sats = gps.satellites.value();
    if (gps.date.isValid()) {
      uint8_t day = gps.date.day();
      uint8_t month = gps.date.month();
      uint8_t year = gps.date.year() % 100;
      i2c_data.date = (day * 10000) + (month * 100) + year;
    } else i2c_data.date = 0;
    if (gps.time.isValid()) {
      uint8_t hour = gps.time.hour();
      uint8_t minute = gps.time.minute();
      uint8_t second = gps.time.second();
      uint8_t centisecond = gps.time.centisecond();
      i2c_data.time = (hour * 1000000UL) + (minute * 10000) + (second * 100) + centisecond;
    } else i2c_data.time = 0;
    if (!unitsMetric) i2c_data.speed *= 0.621371;
  }
  if (M5.Imu.update()) {
    auto data = M5.Imu.getImuData();
    if (!isnan(data.accel.x) && !isnan(data.accel.y) && !isnan(data.accel.z) && 
        !isnan(data.gyro.x) && !isnan(data.gyro.y) && !isnan(data.gyro.z)) {
      float accel_x = data.accel.x - (imu_cal.calibrated ? imu_cal.accel_offset_x : 0);
      float accel_y = data.accel.y - (imu_cal.calibrated ? imu_cal.accel_offset_y : 0);
      float accel_z = data.accel.z - (imu_cal.calibrated ? imu_cal.accel_offset_z : 0);
      float gyro_z = data.gyro.z - (imu_cal.calibrated ? imu_cal.gyro_offset_z : 0);
      yaw_rate_raw = gyro_z * 57.2958;
      yaw_rate_filtered = (YAW_FILTER_ALPHA * yaw_rate_raw) + ((1.0 - YAW_FILTER_ALPHA) * yaw_rate_filtered);
      i2c_data.yaw = yaw_rate_filtered;
      i2c_data.gforce_lon = accel_x;
      i2c_data.gforce_lat = accel_y;
      i2c_data.gforce_total = sqrt(sq(accel_x) + sq(accel_y) + sq(accel_z));
      imuConnected = true;
    } else imuConnected = false;
  } else {
    imuConnected = false;
    yaw_rate_filtered *= 0.9;
    i2c_data.yaw = yaw_rate_filtered;
  }
  float cpu_temp = temperatureRead(); 
  i2c_data.temp_c_x10 = (int16_t)(cpu_temp * 10);
  if (i2c_data.speed > 1.5) {
    if (!sessionActive) {
      noInterrupts(); sessionActive = true; interrupts();
    }
    i2c_data.maxSpeed = max(i2c_data.maxSpeed, i2c_data.speed);
    i2c_data.maxG = max(i2c_data.maxG, i2c_data.gforce_total);
    if (isMakingRun) {
      if (i2c_data.speed > currentRunMaxSpeed) currentRunMaxSpeed = i2c_data.speed;
      if (i2c_data.speed < (1.0f + SPEED_EPSILON)) {
        isMakingRun = false;
        logTopSpeed(currentRunMaxSpeed);
        currentRunMaxSpeed = 0.0f;
      }
    } else if (i2c_data.speed > (5.0f + SPEED_EPSILON)) {
      isMakingRun = true;
      currentRunMaxSpeed = i2c_data.speed;
    }
  }
}

void handleButton() {
  M5.update();
  static bool longPressHandled = false;
  if (M5.BtnA.isPressed()) {
    if (!longPressHandled) {
      if (currentMenu == 5 && M5.BtnA.pressedFor(longPressTimeout)) {
        if (imuConnected) calibrateIMU();
        else showActionFeedback("IMU N/A", COLOR_DANGER);
        longPressHandled = true;
        pressCount = 0;
      }
      else if (currentMenu == 7 && M5.BtnA.pressedFor(longPressTimeout)) {
        for (int i = 0; i < MAX_RUNS; i++) topSpeeds[i] = 0.0f;
        saveTopSpeeds();
        showActionFeedback("CLEARED", COLOR_DANGER);
        longPressHandled = true;
        pressCount = 0;
      }
    }
  } else {
    if (longPressHandled) longPressHandled = false;
  }
  if (M5.BtnA.wasPressed()) {
    if (longPressHandled) return;
    pressCount++;
    if (pressCount == 1) {
      lastPressTime = millis();
    } else if (pressCount == 2) {
      if (millis() - lastPressTime < doublePressTimeout) {
        previousMenu = currentMenu;
        currentMenu = (currentMenu + 1) % NUM_MENUS;
        menuTransition = true;
        pressCount = 0;
      } else {
        pressCount = 1;
        lastPressTime = millis();
      }
    }
  }
  if (pressCount == 1 && millis() - lastPressTime >= doublePressTimeout) {
    if (longPressHandled || M5.BtnA.isPressed()) {
      if (!M5.BtnA.isPressed()) pressCount = 0;
      return; 
    }
    switch(currentMenu) {
      case 2:
        i2c_data.maxSpeed = 0;
        i2c_data.maxG = 0;
        showActionFeedback("RESET", COLOR_WARNING);
        break;
      case 5:
        unitsMetric = !unitsMetric;
        showActionFeedback(unitsMetric ? "METRIC" : "IMPERIAL", COLOR_INFO);
        break;
    }
    pressCount = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== ATOMS3 SECONDARY BOOT ===");
  
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);
  M5.Display.setRotation(2);
  M5.Display.setBrightness(100);
  
  canvas.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  canvas.setTextDatum(TL_DATUM);
  canvas.setFont(&fonts::Font0);
  drawSplashScreen();
  
  Serial.println("=== " FW_VERSION " ===");
  
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("[EEPROM] FAILED!");
  } else {
    Serial.println("[EEPROM] OK");
    loadTopSpeeds();
  }
  
  uint32_t bootCount = 0;
  EEPROM.get(EEPROM_BOOT_COUNT_ADDR, bootCount);
  bootCount++;
  EEPROM.put(EEPROM_BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();
  Serial.printf("[BOOT] Count: %u\n", bootCount);
  
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_WDT || reason == ESP_RST_TASK_WDT) {
    Serial.println("[BOOT] WDT RESET!");
  }
  
  Serial.printf("[WDT] Init %ds\n", WDT_TIMEOUT_S);
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  Serial.println("[GPS] Init...");
  gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);
  
  // ✅✅✅ CRITICAL FIX - PROPER ESP32 I2C  MODE ✅✅✅
  Serial.println("[I2C] SECONDARY MODE INIT...");
  Serial.printf("[I2C] Address: 0x%02X\n", I2C_SECONDARY_ADDRESS);
  Serial.printf("[I2C] SDA:%d SCL:%d\n", GROVE_SDA_PIN, GROVE_SCL_PIN);
  
  // Enable pull-ups
  pinMode(GROVE_SDA_PIN, INPUT_PULLUP);
  pinMode(GROVE_SCL_PIN, INPUT_PULLUP);
  
  // Initialize as I2C BUS - address FIRST parameter
  Wire.begin(I2C_SECONDARY_ADDRESS, GROVE_SDA_PIN, GROVE_SCL_PIN, 100000);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  
  Serial.println("[I2C] SECONDARY READY!");
  Serial.println("[I2C] Waiting for Primary...\n");
  
  Serial.println("[IMU] Check...");
  if (M5.Imu.isEnabled()) {
    Serial.println("[IMU] OK!");
    imuConnected = true;
    delay(500);
    calibrateIMU();
  } else {
    Serial.println("[IMU] NOT FOUND");
    imuConnected = false;
  }
  
  Serial.println("\n=== INIT COMPLETE ===\n");
}

void loop() {
  static unsigned long lastSensorUpdate = 0;
  static unsigned long lastDisplay = 0;
  static unsigned long lastAnimation = 0;
  static unsigned long lastI2CCheck = 0;
  
  unsigned long now = millis();
  
  esp_task_wdt_reset();
  
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  
  if (now - lastSensorUpdate >= 50) {
    updateSensorData();
    lastSensorUpdate = now;
  }
  
  if (now - lastAnimation >= 16) {
    animationPhase += 0.1;
    if (animationPhase > TWO_PI) animationPhase = 0;
    lastAnimation = now;
  }
  
  handleButton();
  
  if (topSpeedsNeedSave && (now - lastEEPROMSave > EEPROM_SAVE_DELAY)) {
    saveTopSpeeds();
    topSpeedsNeedSave = false;
    lastEEPROMSave = now;
  }
  
  // I2C STATUS MONITOR
  if (now - lastI2CCheck >= 5000) {
    Serial.println("=== I2C STATUS ===");
    Serial.printf("RX:%d TX:%d\n", i2c_receive_count, i2c_request_count);
    unsigned long timeSince;
    noInterrupts();
    timeSince = millis() - last_i2c_activity;
    interrupts();
    Serial.printf("Last: %lums ago\n", timeSince);
    Serial.printf("%s\n", (timeSince < 2000) ? "CONNECTED" : "NO COMM");
    Serial.println("==================\n");
    lastI2CCheck = now;
  }
  
  if (now - lastDisplay >= 100) {
    canvas.fillScreen(COLOR_BG);
    if (menuTransition) {
      animateMenuTransition();
      menuTransition = false;
    } else {
      switch(currentMenu) {
        case 0: drawMenuSpeed(0); break;
        case 1: drawMenuCockpit(0); break;
        case 2: drawMenuStats(0); break;
        case 3: drawMenuGPS(0); break;
        case 4: drawMenuI2C(0); break;
        case 5: drawMenuSettings(0); break;
        case 6: drawMenuSystemInfo(0); break;
        case 7: drawMenuRecords(0); break;
      }
      drawActionFeedback();
      canvas.pushSprite(0, 0);
    }
    lastDisplay = now;
  }
  
  delay(1);
}
