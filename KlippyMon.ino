/* ESP32 Dev Module 
   NO OTA 2MB APP/2MB SPIFFS
*/

#include "Settings.h"     // some basic settings (language, TimeZone)
#include "Translation.h"  // language translations
#include "Ntfy.h"         // for push nofitications if you want them
#include "NTP_Time.h"     // time server change

//#define FORMAT_LittleFS  // Wipe LittleFS and all files! Disable after use.

#define VERSION "v4.0"
#define hostNameCYD "KlippyMon"
#define CONFIG "/config.txt"

// ---------------- GAUGE IDs ----------------
#define nozzleGauge 1
#define progressGauge 2
#define bedGauge 3

// ---------------- LAYOUT Y POSITIONS ----------------

#define clockBottomY 44  // tighten clock up
#define printerNameY 60  // tighter below clock
#define headingY 77      // gauge headings
#define gaugeY 112       // arc centres (moved up 16px)
#define dataY 110        // gauge values
#define statusZoneY 146  // status zone starts earlier
#define graphicX 65      // re-centre: (240-110)/2
#define graphicY 147     // graphic starts here, ends at 257
#define endTimeY 284     // ETA/End line (257 + 4 + ~11px font)
#define filenameY 300    // filename
#define versionY 308     // version string
#define statusZoneH (versionY - 16 - statusZoneY)
#define belowClockY 44  // start of area below clock, used for fillRect clears
#define chamberX 32     // for display a chamber temp if you have one
#define chamberY 195    // ditto
// ---------------- RGB LED ----------------
#define RED_PIN 22
#define GREEN_PIN 16
#define BLUE_PIN 17

// ---------------- DISPLAY ----------------
#define SCREEN_W 240
#define SCREEN_H 320

TFT_eSPI tft = TFT_eSPI();

// ---------------- PRINTER STATE ----------------
bool foundPrinter = false;
String printerName = "";

String printState;
float progress, nozzleTemp, nozzleTarget, bedTemp, bedTarget, printDuration, totalDuration;
uint16_t progressPercent;
uint16_t lastNozzleTemp = 9999, lastBedTemp = 9999, lastProgress = 9999;

bool greenON = true;
bool greenOFF = false;

uint8_t thePollTime = 10;  // seconds between printer polls

float savedTotalDuration = 0.0;  // save the total duration so it doesn't get nop'd out

float toolheadX = 0, toolheadY = 0, toolheadZ = 0, toolheadE = 0;  // for checking against no printer movement

// ---------------- PRINTER STATE MACHINE ----------------
typedef enum {
  STATE_IDLE,      // standby - printer on but doing nothing
  STATE_PREP,      // printing flag set but printDuration == 0 (heating/levelling/homing/filament)
  STATE_PRINTING,  // printDuration > 0 - actual print in progress
  STATE_COMPLETE   // just transitioned from PRINTING to IDLE
} PrinterState;

PrinterState currentState = STATE_IDLE;
PrinterState lastState = STATE_IDLE;

bool showSleep = false;
bool showIdle = false;
bool justFinished = false;
uint32_t finishedAt = 0;
String thePrintFileRaw;  // full untruncated path, used only for thumbnail fetch
#define FINISHED_DISPLAY_MS 30000
#define HTTP_CONNECT_TIMEOUT 2000   // ms - increase if on printer is on WiFi
#define HTTP_RESPONSE_TIMEOUT 3000  // ms

// ---------------- PRINTER POLLING TASK (core 0) ----------------
struct PrinterData {
  String name;   // "" = no printer
  bool ok;
  String state;
  String file;   // full path
  float progress, nozzleTemp, nozzleTarget, bedTemp, bedTarget, printDuration, totalDuration;
  float toolX, toolY, toolZ, toolE;
  float chamberTemp, chamberTarget;
  uint32_t seq;  // bumped after every poll
};

PrinterData sharedData;
uint32_t lastSeq = 0;
SemaphoreHandle_t dataMutex;      // shared data, printerIP/Port, thumbnail
TaskHandle_t netTaskHandle;
bool netReset = false;            // settings changed
String thumbRequest = "";
volatile bool thumbDone = false;

#define NET_POLL_NOW 0x01
#define NET_THUMB 0x02
#define NET_STACK 12288

// ---------------- SCREEN SAVER ----------------
#define matrixTopY 48
#define MATRIX_COLS 20   // 12px wide
#define MATRIX_ROWS 17   // 16px high
#define MATRIX_FRAME_MS 50

struct MatrixDrop {
  int16_t head;   // negative = waiting
  uint8_t len;
  uint8_t speed;  // frames per row
  uint8_t tick;
};
MatrixDrop drops[MATRIX_COLS];
bool saverOn = false;
uint32_t offlineSince = 0;

// ---------------- CLOCK ----------------
uint8_t lastSecond = 99;
uint8_t lastMinute, lastHour, lastDay, lastMonth;
uint16_t lastYear, myYear;
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myMonth, myWeekDay;
bool colonBlink = false;
bool activeETA = false;
uint16_t etaHH, etaMM;

// Web server
WebServer server(80);

// ---------------- FORWARD DECLARATIONS ----------------
void handlePrinterOffLine();
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y);
uint16_t read16(fs::File &f);
uint32_t read32(fs::File &f);
void handle_ClockDisplay();
void handleGaugeHeadings();
void handleGauge(uint8_t whichGauge, int16_t gaugeValue);
void setRGB(bool redLevel, bool greenLevel, bool blueLevel);
void netTask(void *param);
String fetchHostName(const String &base);
String buildQueryURL(const String &base);
void serviceThumbnail(const String &ip, const String &port);
uint8_t *fetchThumbnail(const String &ip, const String &port, const String &rawPath, int &len);
void handlePrinterData();
void applyPrinterData(const PrinterData &d);
void requestThumbnail(const String &rawPath);
void handleThumbnail();
bool drawThumbnail(uint8_t *buf, int len);
void handleScreenSaver();
void resetDrop(MatrixDrop &m);
void drawMatrixCell(uint8_t col, int16_t row, uint16_t color);
void handleTimeUsed();
void handleETA();
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result);
bool fetchPrinterData(const String &url, PrinterData &d);
PrinterState determinePrinterState();
void updatePrinterDisplay(PrinterState state);
void handlePrinterStatus();
void configModeCallback(WiFiManager *myWiFiManager);
String SendHTML(String savedPopup = "");
String buildSavedPopup();
void handlePrinterUpdate();
String getPrinterSetup();
void writeSettings();
void readSettings();
String extractFileName(const String &path, bool withExt);
void drawFileName();
void handleWifiReset();
void redirectHome();
void drawWiFiQuality();
int8_t getWifiQuality();
void handle_OnConnect();
void drawVersionString();
int pngDraw(PNGDRAW *pDraw);
void fetchPrinterLimits(const String &base);          // get the printers default limits
String ntfyServerDisplay();                           // cleans up the URL for local host server
void beginHTTP(HTTPClient &http, const String &url);  // timeouts for http

// ============================================================
//  VERSION STRING
// ============================================================
void drawVersionString() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(VERSION) + " @2026 - Wabbit Wanch Design", 120, versionY, 2);
}

// ============================================================
//  SETUP
// ============================================================
void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(server.hasArg("saved") ? buildSavedPopup() : ""));
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.invertDisplay(false);  // off until the settings are loaded (invertColours); don't rely on whichever User_Setup.h is in the library
  tft.setRotation(0);

#ifdef FORMAT_LittleFS
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Formatting LittleFS, please wait...", 120, 160, 2);
  LittleFS.format();
  ESP.restart();
#endif

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS init failed!");
    while (1) yield();
  }
  Serial.println("LittleFS ready.");

  readSettings();  // early, so an inverted panel is right from the WiFi setup screen on
  tft.invertDisplay(invertColours);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(0, 0, 0);

  WiFiManager wifiManager;
  wifiManager.setHostname("KlippyMon");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setBreakAfterConfig(true);
  if (!wifiManager.autoConnect(hostNameCYD)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  WiFi.hostname(hostNameCYD);
  MDNS.begin(hostNameCYD);

  server.on("/", handle_OnConnect);
  server.on("/updatePrinterInfo", handlePrinterUpdate);
  server.on("/wifiReset", handleWifiReset);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  tft.fillScreen(TFT_BLACK);
  drawVersionString();

  udp.begin(localPort);
  syncTime();

  if (checkTimezoneOffsets()) writeSettings();

  dataMutex = xSemaphoreCreateMutex();
  ntfyQueue = xQueueCreate(4, sizeof(NtfyMsg));
  offlineSince = millis();
  xTaskCreatePinnedToCore(netTask, "netTask", NET_STACK, NULL, 1, &netTaskHandle, 0);  // printer polling on core 0
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  static time_t prevDisplay = 0;
  static uint8_t lastSyncHour = 99;

  utc = now();

  if (utc != prevDisplay) {
    prevDisplay = utc;
    handle_ClockDisplay();
  }

  uint8_t currentHour = hour(toLocal(utc));
  if (currentHour != lastSyncHour) {
    syncTime();
    if (currentHour == 2) {
      if (checkTimezoneOffsets()) writeSettings();
    }
    lastSyncHour = currentHour;
  }

  handlePrinterData();  // pick up the latest poll
  handleThumbnail();  // draw a downloaded thumbnail
  handleScreenSaver();  // Matrix rain when offline

  server.handleClient();
}

// ============================================================
//  WIFI QUALITY BAR GRAPH
// ============================================================
void drawWiFiQuality() {
  const byte numBars = 5;
  const byte barWidth = 3;
  const byte barHeight = 20;
  const byte barSpace = 1;
  const uint16_t barXPosBase = SCREEN_W - 25;
  const byte barYPosBase = 20;
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {
      for (byte ii = 0; ii < barWidth; ii++) {
        byte nextBarThreshold = (i + 1) * (100 / numBars);
        byte currentBarThreshold = i * (100 / numBars);
        byte currentBarIncrements = (barHeight / numBars) * (i + 1);
        float rangePerBar = (100 / numBars);
        float currentBarStrength;
        if ((quality > currentBarThreshold) && (quality < nextBarThreshold)) {
          currentBarStrength = ((quality - currentBarThreshold) / rangePerBar) * currentBarIncrements;
        } else if (quality >= nextBarThreshold) {
          currentBarStrength = currentBarIncrements;
        } else {
          currentBarStrength = 0;
        }
        if (j < currentBarStrength) {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barColor);
        } else {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barBackColor);
        }
      }
    }
  }
}

int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) return 0;
  else if (dbm >= -50) return 100;
  else return 2 * (dbm + 100);
}

// ============================================================
//  CLOCK DISPLAY  (WabbitWeather style, NotoSansBold36)
// ============================================================
void handle_ClockDisplay() {
  char buffer[24];
  uint8_t theHour;

  time_t utc = now();
  time_t localTime = toLocal(utc);

  myHour = hourFormat12(localTime);
  my24Hour = hour(localTime);
  myMinute = minute(localTime);
  mySecond = second(localTime);
  myDay = day(localTime);
  myMonth = month(localTime);
  myYear = year(localTime);

  uint8_t xpos = (SCREEN_W / 2) - 1;

  tft.loadFont(AA_FONT_LARGE, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  theHour = (show24HR) ? my24Hour : myHour;

  if (colonBlink == false) {
    sprintf(buffer, " %2u:%02u ", theHour, myMinute);
  } else {
    sprintf(buffer, " %2u %02u ", theHour, myMinute);
  }
  colonBlink = !colonBlink;

  tft.setTextPadding(tft.textWidth(" 44:44 "));
  tft.drawString(buffer, xpos, clockBottomY);
  tft.setTextPadding(0);
  tft.unloadFont();

  // Save for next pass
  lastSecond = mySecond;
  lastMinute = myMinute;
  lastHour = my24Hour;
  lastDay = myDay;
  lastYear = myYear;
  lastMonth = myMonth;

  if (mySecond == (thePollTime + 5)) {
    drawWiFiQuality();
  }
}

// ============================================================
//  GAUGE HEADINGS
// ============================================================
void handleGaugeHeadings() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(strNozzle, 40, headingY, 2);
  tft.drawString(strProgress, 120, headingY, 2);
  tft.drawString(strBed, 200, headingY, 2);
}

// ============================================================
//  GAUGE DRAW
// ============================================================
void handleGauge(uint8_t whichGauge, int16_t gaugeValue) {
  float temp;
  uint16_t theMove;
  tft.setTextDatum(MC_DATUM);

  switch (whichGauge) {
    case nozzleGauge:
      temp = (float(gaugeValue) / maxNozzleTemp);
      theMove = (temp * 280) + 40;
      tft.drawSmoothArc(40, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      tft.drawSmoothArc(40, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);
      tft.fillCircle(40, gaugeY, 20, TFT_BLACK);
      tft.setTextColor((nozzleTarget != 0) ? TFT_RED : TFT_WHITE, TFT_BLACK);
      tft.drawString(String(gaugeValue), 40, dataY, 2);
      break;

    case progressGauge:
      temp = (float(gaugeValue) / 100);
      theMove = (temp * 280) + 40;
      tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      if (theMove > 40) {
        tft.drawSmoothArc(120, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);
      }
      tft.fillCircle(120, gaugeY, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(String(gaugeValue) + "%", 120, dataY, 2);
      break;

    case bedGauge:
      temp = (float(gaugeValue) / maxBedTemp);
      theMove = (temp * 280) + 40;
      tft.drawSmoothArc(200, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      tft.drawSmoothArc(200, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);
      tft.fillCircle(200, gaugeY, 20, TFT_BLACK);
      tft.setTextColor((bedTarget != 0) ? TFT_RED : TFT_WHITE, TFT_BLACK);
      tft.drawString(String(gaugeValue), 200, dataY, 2);
      break;
  }
}

// ============================================================
//  RGB LED
// ============================================================
void setRGB(bool redLevel, bool greenLevel, bool blueLevel) {
  digitalWrite(RED_PIN, !redLevel);
  digitalWrite(GREEN_PIN, !greenLevel);
  digitalWrite(BLUE_PIN, !blueLevel);
}

// ============================================================
//  PRINTER OFFLINE SCREEN
// ============================================================
void handlePrinterOffLine() {
  if (printerName == "") {
    if (showSleep == false) {
      tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
      drawBmp(LittleFS, OFFLINE_IMAGE, 27, belowClockY + 4);
      String ipaddress = "KlippyMon " + WiFi.localIP().toString();
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(BC_DATUM);
      tft.drawString(ipaddress, 120, versionY - 16, 2);
      drawVersionString();  // programmer info
      showSleep = true;
      offlineSince = millis();
    }
  } else {
    tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
    tft.loadFont(AA_FONT_SMALL, LittleFS);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(printerName, 120, printerNameY);
    tft.unloadFont();
    handleGaugeHeadings();
    drawVersionString();
  }
}

// ============================================================
//  SCREEN SAVER — Matrix rain below the clock
// ============================================================
void handleScreenSaver() {
#if SCREENSAVER_MINUTES > 0
  static uint32_t lastFrame = 0;

  if (!saverOn) {
    if (printerName == "" && showSleep
        && millis() - offlineSince >= SCREENSAVER_MINUTES * 60000UL) {
      tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
      for (uint8_t c = 0; c < MATRIX_COLS; c++) resetDrop(drops[c]);  // random start, length, speed
      saverOn = true;
    }
    return;
  }

  if (millis() - lastFrame < MATRIX_FRAME_MS) return;
  lastFrame = millis();

  for (uint8_t c = 0; c < MATRIX_COLS; c++) {
    MatrixDrop &m = drops[c];
    if (++m.tick < m.speed) continue;
    m.tick = 0;
    m.head++;
    drawMatrixCell(c, m.head, TFT_WHITE);  // head
    drawMatrixCell(c, m.head - 1, TFT_GREEN);  // trail
    drawMatrixCell(c, m.head - m.len / 2, TFT_DARKGREEN);  // fade
    drawMatrixCell(c, m.head - m.len, TFT_BLACK);  // erase tail
    if (m.head - m.len >= MATRIX_ROWS) resetDrop(m);  // start over
  }
#endif
}

void resetDrop(MatrixDrop &m) {
  m.head = -random(1, 40);
  m.len = random(6, 14);
  m.speed = random(1, 4);
  m.tick = 0;
}

void drawMatrixCell(uint8_t col, int16_t row, uint16_t color) {
  static const char glyphs[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$+-*/=<>#";
  if (row < 0 || row >= MATRIX_ROWS) return;
  int16_t x = col * 12;
  int16_t y = matrixTopY + row * 16;
  tft.fillRect(x, y, 11, 16, TFT_BLACK);
  if (color == TFT_BLACK) return;
  tft.setTextColor(color);
  tft.drawChar(glyphs[random(sizeof(glyphs) - 1)], x + 1, y, 2);
}

// ============================================================
//  HTTP HELPER
// ============================================================
void beginHTTP(HTTPClient &http, const String &url) {
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT);
  http.begin(url);
}

// ============================================================
//  PRINTER POLLING TASK (core 0) - no TFT or LittleFS here
// ============================================================
void netTask(void *param) {
  PrinterData d = {};
  uint8_t failCount = 0;

  for (;;) {
    TickType_t cycleStart = xTaskGetTickCount();

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    String ip = printerIP;
    String port = printerPort;
    bool reset = netReset;
    netReset = false;
    xSemaphoreGive(dataMutex);
    String base = "http://" + ip + ":" + port;

    if (reset) {
      d.name = "";
      failCount = 0;
      hasChamber = false;
      chamberSensorName = "";
    }

    if (d.name == "") {
      d.name = fetchHostName(base);  // GET /printer/info
      if (d.name != "") fetchPrinterLimits(base);  // max temps, chamber sensor
    }

    d.ok = false;
    if (d.name != "") {
      d.ok = fetchPrinterData(buildQueryURL(base), d);  // status query
      if (d.ok) {
        failCount = 0;  // reset on success
      } else {
        failCount++;
        Serial.println("fetchPrinterData failed - count: " + String(failCount));
        if (failCount >= 3) {  // 3 consecutive failures (~30 seconds) before giving up
          failCount = 0;
          d.name = "";
        }
      }
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (!netReset) {
      d.seq = sharedData.seq + 1;
      sharedData = d;
    }
    xSemaphoreGive(dataMutex);

    NtfyMsg msg;
    while (xQueueReceive(ntfyQueue, &msg, 0) == pdTRUE) ntfyPost(msg);  // send queued ntfy messages

    TickType_t nextPoll = cycleStart + pdMS_TO_TICKS(thePollTime * 1000UL);
    for (;;) {
      TickType_t ticks = xTaskGetTickCount();
      if ((int32_t)(nextPoll - ticks) <= 0) break;
      uint32_t bits = 0;
      if (xTaskNotifyWait(0, UINT32_MAX, &bits, nextPoll - ticks) == pdFALSE) break;
      if (bits & NET_THUMB) serviceThumbnail(ip, port);  // download the requested thumbnail
      if (bits & NET_POLL_NOW) break;
    }
  }
}

// ============================================================
//  FIND PRINTER HOSTNAME
// ============================================================
String fetchHostName(const String &base) {
  String name = "";
  if (WiFi.status() == WL_CONNECTED) {
    setRGB(0, greenON, 0);
    HTTPClient http;
    beginHTTP(http, base + printerINFO);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        name = doc["result"]["hostname"] | "";
      }
    }
    http.end();
    setRGB(0, greenOFF, 0);
  }
  return name;
}

// ============================================================
//  PHASE 1 — FETCH & PARSE
// ============================================================
bool fetchPrinterData(const String &url, PrinterData &d) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost");
    return false;
  }
  setRGB(0, greenON, 0);
  HTTPClient http;
  beginHTTP(http, url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    setRGB(0, greenOFF, 0);
    return false;
  }

  String payload = http.getString();
  http.end();
  setRGB(0, greenOFF, 0);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("JSON Error");
    return false;
  }

  d.nozzleTarget = doc["result"]["status"]["extruder"]["target"] | 0.0;
  d.bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;
  d.state = doc["result"]["status"]["print_stats"]["state"] | "unknown";
  d.nozzleTemp = doc["result"]["status"]["extruder"]["temperature"] | 0.0;
  d.bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
  d.printDuration = doc["result"]["status"]["print_stats"]["print_duration"] | 0.0;
  d.totalDuration = doc["result"]["status"]["print_stats"]["total_duration"] | 0.0;
  d.progress = doc["result"]["status"]["display_status"]["progress"] | 0.0;
  d.file = doc["result"]["status"]["print_stats"]["filename"] | "";

  JsonArray toolheadPos = doc["result"]["status"]["toolhead"]["position"];
  if (!toolheadPos.isNull() && toolheadPos.size() >= 4) {
    d.toolX = toolheadPos[0];
    d.toolY = toolheadPos[1];
    d.toolZ = toolheadPos[2];
    d.toolE = toolheadPos[3];
  }

  if (hasChamber && chamberSensorName.length() > 0 && doc["result"]["status"].containsKey(chamberSensorName.c_str())) {
    d.chamberTemp = doc["result"]["status"][chamberSensorName.c_str()]["temperature"] | 0.0;
    d.chamberTarget = doc["result"]["status"][chamberSensorName.c_str()]["target"] | 0.0;
  }

  return true;
}

// ============================================================
//  NEW PRINTER DATA FROM netTask
// ============================================================
void handlePrinterData() {
  if (sharedData.seq == lastSeq) return;

  PrinterData d;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  d = sharedData;
  lastSeq = d.seq;
  xSemaphoreGive(dataMutex);

  if (d.name == "") {
    if (printerName != "") {  // just went offline
      printerName = "";
      tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
      drawVersionString();
      showSleep = false;
      showIdle = false;
    }
    handlePrinterOffLine();  // Asleep screen
    return;
  }

  if (printerName == "") {  // just found
    printerName = d.name;
    Serial.println(printerName);
    lastBedTemp = 0;
    lastNozzleTemp = 0;
    lastProgress = 0;
    saverOn = false;
    handlePrinterOffLine();  // draw the printer screen
  }

  if (d.ok) {
    applyPrinterData(d);  // copy into the display globals
    handlePrinterStatus();  // state, gauges, ETA
  }
}

void applyPrinterData(const PrinterData &d) {
  nozzleTarget = d.nozzleTarget;
  bedTarget = d.bedTarget;
  printState = d.state;
  nozzleTemp = d.nozzleTemp;
  bedTemp = d.bedTemp;
  printDuration = d.printDuration;
  totalDuration = d.totalDuration;
  if (totalDuration > 0) savedTotalDuration = totalDuration;
  progress = d.progress;
  toolheadX = d.toolX;
  toolheadY = d.toolY;
  toolheadZ = d.toolZ;
  toolheadE = d.toolE;
  chamberTemp = d.chamberTemp;
  chamberTarget = d.chamberTarget;

  if (thePrintFile == "" && printState != "standby"
      && printState != "canceled" && printState != "cancelled"
      && printState != "error") {
    if (d.file != "") {
      thePrintFileRaw = d.file;
      thePrintFile = extractFileName(d.file, false);
    }
  }
}

// ============================================================
//  FETCH PRINTER TEMP LIMITS FROM KLIPPER CONFIG
// ============================================================
void fetchPrinterLimits(const String &base) {
  HTTPClient http;
  beginHTTP(http, base + "/printer/objects/query?configfile=config");  // config section only, no settings copy
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument filter;  // keep only the two max_temps
    filter["result"]["status"]["configfile"]["config"]["extruder"]["max_temp"] = true;
    filter["result"]["status"]["configfile"]["config"]["heater_bed"]["max_temp"] = true;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!error) {
      String nozzle = doc["result"]["status"]["configfile"]["config"]["extruder"]["max_temp"] | "350";
      String bed = doc["result"]["status"]["configfile"]["config"]["heater_bed"]["max_temp"] | "120";
      maxNozzleTemp = nozzle.toInt();
      maxBedTemp = bed.toInt();
    } else {
      Serial.printf("fetchPrinterLimits JSON error: %s (%u bytes)\n", error.c_str(), payload.length());
    }
  } else {
    Serial.printf("fetchPrinterLimits failed: %d\n", httpCode);
  }
  http.end();

  // ── Chamber detection ─────────────────────────────────────
  beginHTTP(http, base + "/printer/objects/list");
  httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      hasChamber = false;
      chamberSensorName = "";
      JsonArray objects = doc["result"]["objects"].as<JsonArray>();
      for (JsonVariant obj : objects) {
        String name = obj.as<String>();
        if (name == "heater_generic hot" || name == "heater_generic chamber" || name == "heater_generic Chamber" || name == "temperature_sensor chamber" || name == "temperature_sensor Chamber" || name == "temperature_sensor chamber_temp") {
          chamberSensorName = name;
          hasChamber = true;
          break;
        }
      }
    }
  }
  http.end();
}

// ============================================================
//  TOTAL TIME USED (shown at print end)
// ============================================================
void handleTimeUsed() {
  char buffer[30];
  int32_t totalSecs = (int32_t)savedTotalDuration;
  int hrs = totalSecs / 3600;
  int mins = (totalSecs % 3600) / 60;

  tft.fillRect(0, statusZoneY, 239, statusZoneH, TFT_BLACK);
  drawBmp(LittleFS, SUCCESS_IMAGE, graphicX, graphicY);

  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (hrs != 0) {
    sprintf(buffer, "%s: %1u:%02u %s", strTotal, hrs, mins, strHrs);
  } else {
    sprintf(buffer, "%s: %u %s", strTotal, mins, strMins);
  }
  tft.drawString(buffer, 120, endTimeY);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  drawFileName();  // centred, or cut to fit
  tft.unloadFont();

  justFinished = true;
  showIdle = true;
}

// ============================================================
//  ESTIMATE TIME REMAINING
// ============================================================
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result) {
  if (percentComplete <= 0.0f || percentComplete > 100.0f) {
    snprintf(result, 16, "--:--");
    return;
  }
  float totalEstimated = elapsedSeconds / (percentComplete / 100.0f);
  float remainingSeconds = totalEstimated - elapsedSeconds;
  if (remainingSeconds < 0) remainingSeconds = 0;

  unsigned long remaining = (unsigned long)remainingSeconds;
  etaHH = remaining / 3600;
  etaMM = (remaining % 3600) / 60;
  snprintf(result, 16, "%02u:%02u", etaHH, etaMM);
}

// ============================================================
//  ETA
// ============================================================
void handleETA() {
  if (currentState != STATE_PRINTING) {
    activeETA = false;
    return;
  }
  char timeLeft[24];
  uint16_t pct = uint16_t(progress * 100);
  if (pct > lastProgress) {
    lastProgress = pct;
    estimateTimeRemaining(printDuration, pct, timeLeft);

    // Build the end time string
    time_t utc = now();
    time_t localTime = toLocal(utc);
    time_t addSeconds = ((time_t)etaHH * 3600) + ((time_t)etaMM * 60);
    time_t futureTime = localTime + addSeconds;
    uint8_t theHour = (show24HR) ? hour(futureTime) : hourFormat12(futureTime);
    uint8_t theMinute = minute(futureTime);
    bool nextDay = (hour(futureTime) < hour(localTime) && etaHH > 0);

    char endStr[50];

    if (!show24HR) {
      const char *ampm = (hour(futureTime) >= 12) ? "PM" : "AM";
      if (nextDay) {
        sprintf(endStr, "ETA: %s  -  FPT: %u:%02u %s", timeLeft, theHour, theMinute, ampm);
      } else {
        sprintf(endStr, "ETA: %s  -  FPT: %u:%02u %s", timeLeft, theHour, theMinute, ampm);
      }
    } else {
      sprintf(endStr, "ETA: %s  -  FPT: %u:%02u", timeLeft, theHour, theMinute);
    }

    tft.loadFont(AA_FONT_SMALL, LittleFS);
    tft.setTextDatum(BC_DATUM);
    tft.fillRect(0, endTimeY - 16, 239, 18, TFT_BLACK);

    if (nextDay) {
      // Draw ETA part in white
      char etaPart[40];
      sprintf(etaPart, "ETA: %s  -  FPT: ", timeLeft);
      char endPart[16];
      if (!show24HR) {
        const char *ampm = (hour(futureTime) >= 12) ? "PM" : "AM";
        sprintf(endPart, "%u:%02u %s", theHour, theMinute, ampm);
      } else {
        sprintf(endPart, "%u:%02u", theHour, theMinute);
      }
      // Measure ETA part width to position End part
      int16_t etaWidth = tft.textWidth(etaPart);
      int16_t totalWidth = etaWidth + tft.textWidth(endPart);
      int16_t startX = 120 - (totalWidth / 2);
      tft.setTextDatum(BL_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(etaPart, startX, endTimeY);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString(endPart, startX + etaWidth, endTimeY);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(endStr, 120, endTimeY);
    }

    tft.unloadFont();
    activeETA = true;
  }
}
// ============================================================
//  PHASE 2 — DETERMINE STATE
// ============================================================
PrinterState determinePrinterState() {
  lastState = currentState;

  // Cancelled or error: always go straight to idle, never STATE_COMPLETE
  // Note: Creality K1/K1 Max reports "cancelled" (British spelling),
  // while standard Klipper/Moonraker reports "canceled".
  if (printState == "canceled" || printState == "cancelled" || printState == "error") {
    return STATE_IDLE;
  }

  if (printState == "standby") {
    if (lastState == STATE_PRINTING || lastState == STATE_PREP) {
      return STATE_COMPLETE;
    }
    return STATE_IDLE;
  }

  if (printDuration == 0.0) {
    return STATE_PREP;
  }

  return STATE_PRINTING;
}

// ============================================================
//  PHASE 3 — UPDATE DISPLAY
// ============================================================
void updatePrinterDisplay(PrinterState state) {
  tft.setTextDatum(MC_DATUM);

  // Always update temperature gauges
  if (round(nozzleTemp) != lastNozzleTemp) {
    lastNozzleTemp = round(nozzleTemp);
    handleGauge(nozzleGauge, lastNozzleTemp);
  }
  if (round(bedTemp) != lastBedTemp) {
    lastBedTemp = round(bedTemp);
    handleGauge(bedGauge, lastBedTemp);
  }

  switch (state) {

    case STATE_IDLE:
      tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      tft.fillCircle(120, gaugeY, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(strIdle, 120, dataY, 2);

      // Coming from PRINTING/PREP without a STATE_COMPLETE means it was
      // cancelled or errored out — clear the tracked filename so the next
      // print picks up its own name, and force the idle graphic to redraw.
      if (lastState == STATE_PRINTING || lastState == STATE_PREP) {
        thePrintFile = "";
        thePrintFileRaw = "";
        showIdle = false;
      }

      if (!showIdle && !justFinished) {
        tft.fillRect(0, statusZoneY, 239, statusZoneH, TFT_BLACK);
        drawBmp(LittleFS, IDLE_IMAGE, graphicX + 7, graphicY + 7);
        showIdle = true;
      }
      activeETA = false;
      break;

    case STATE_PREP:
      tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);
      tft.fillCircle(120, gaugeY, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(strPrep, 120, dataY, 2);
      if (!showIdle) {
        tft.fillRect(0, statusZoneY, 239, statusZoneH, TFT_BLACK);
        drawBmp(LittleFS, HEATING_IMAGE, graphicX, graphicY);
        showIdle = true;
      }
      activeETA = false;
      break;

    case STATE_PRINTING:
      showIdle = false;
      justFinished = false;
      // Serial.println("STATE_PRINTING - lastState: " + String(lastState) + " thePrintFile: " + thePrintFile);
      if (thePrintFile != "" && lastState != STATE_PRINTING) {
        // Serial.println("Thumbnail block firing - lastState: " + String(lastState));
        // Serial.println("thePrintFile on reconnect: " + thePrintFile);
        ntfyResetForNewPrint();
        tft.fillRect(0, statusZoneY, 239, statusZoneH, TFT_BLACK);
        requestThumbnail(thePrintFileRaw);  // netTask fetches it
        tft.loadFont(AA_FONT_SMALL, LittleFS);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);
        tft.fillRect(0, filenameY - 14, 239, 16, TFT_BLACK);  // clear filename line
        drawFileName();  // centred, or cut to fit
        tft.unloadFont();
        handleGauge(progressGauge, 0);  // ← start at 0% immediately
        lastProgress = 0;               // ← force redraw on first real progress
      }

      progressPercent = uint16_t(progress * 100.0);
      if (progressPercent != lastProgress) {
        lastProgress = progressPercent;
        handleGauge(progressGauge, lastProgress);
      }
     ntfyCheckStall(progress, toolheadX, toolheadY, toolheadZ, toolheadE);  // was progressPercent

      // ── Chamber temp ──────────────────────────────────────
      if (hasChamber) {
        tft.loadFont(AA_FONT_SMALL, LittleFS);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("CHMBR", chamberX, chamberY);
        tft.fillRect(0, chamberY + 6, 63, 18, TFT_BLACK);  // clear number line only
        tft.setTextColor((chamberTarget > 0) ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.drawString(String((int)chamberTemp) + "C", chamberX, chamberY + 18);
        tft.unloadFont();
      }
      break;

    case STATE_COMPLETE:
      lastProgress = 0;
      handleGauge(progressGauge, 0);

      if (thePrintFile != "") {
        ntfyPrintComplete(thePrintFileRaw, savedTotalDuration);  // send notification
        tft.fillRect(0, filenameY - 14, 239, 16, TFT_BLACK);
        handleTimeUsed();
      }
      thePrintFile = "";
      showIdle = true;
      activeETA = false;
      break;
  }
}

// ============================================================
//  handlePrinterStatus
// ============================================================
void handlePrinterStatus() {
  currentState = determinePrinterState();
  updatePrinterDisplay(currentState);
  handleETA();
}

// ============================================================
//  WIFI CONFIG AP SCREEN
// ============================================================
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(hostNameCYD), SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.drawString(strAPActive, SCREEN_W / 2, (SCREEN_H / 2) + 36, 2);
  tft.unloadFont();
  delay(2000);
}

// ============================================================
//  nfty setup
// ============================================================

String ntfyServerDisplay() {
  String s = ntfyServer;
  s.replace("https://", "");
  s.replace("http://", "");
  // Strip port if present
  int colonIdx = s.lastIndexOf(':');
  if (colonIdx > 0) s = s.substring(0, colonIdx);
  return s;
}

// ============================================================
//  WEB PAGE — dark theme
// ============================================================
String SendHTML(String savedPopup) {
  String page = String(HTML_TEMPLATE);

  // Header
  page.replace("%VERSION%", String(VERSION));
  page.replace("%WIFI_QUALITY%", String(getWifiQuality()));

  page.replace("%SAVED_POPUP%", savedPopup);// empty unless just saved

  // WiFi reset modal
  page.replace("%WIFI_RESET_TITLE%", String(wcWifiResetTitle));
  page.replace("%WIFI_RESET_BODY%", String(wcWifiResetBody));
  page.replace("%WIFI_RESET_YES%", String(wcWifiResetYes));
  page.replace("%WIFI_RESET_CANCEL%", String(wcWifiResetCancel));
  page.replace("%WIFI_RESET_BTN%", String(wcWifiResetBtn));

  // Section headings
  page.replace("%SEC_PRINTER%", String(wcSecPrinter));
  page.replace("%SEC_NTFY%", String(wcSecNtfy));
  page.replace("%SEC_WIFI%", String(wcSecWifi));

  // Printer setup block
  page.replace("%PRINTER_SETUP%", getPrinterSetup());

  // Ntfy labels
  page.replace("%NTFY_ENABLED%", String(wcNtfyEnabled));
  page.replace("%NTFY_SERVER%", String(wcNtfyServer));
  page.replace("%NTFY_PORT%", String(wcNtfyPort));
  page.replace("%NTFY_TOPIC%", String(wcNtfyTopic));
  page.replace("%NTFY_TOKEN%", String(wcNtfyToken));
  page.replace("%NTFY_STALL_MIN%", String(wcNtfyStallMin));

  // Ntfy values
  page.replace("%NTFY_ENABLED_CHECKED%", ntfyEnabled ? " checked" : "");
  page.replace("%NTFY_SERVER_VAL%", ntfyServerDisplay());
  page.replace("%NTFY_PORT_VAL%", ntfyPort);
  page.replace("%NTFY_TOPIC_VAL%", ntfyTopic);
  page.replace("%NTFY_TOKEN_VAL%", ntfyToken);
  page.replace("%NTFY_STALL_MIN_VAL%", String(ntfyStallMin));

  // Save button
  page.replace("%SAVE_BTN%", String(wcSaveBtn));

  return page;
}

void handlePrinterUpdate() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (server.hasArg("printerIP")) printerIP = server.arg("printerIP");
  if (server.hasArg("printerPort")) printerPort = server.arg("printerPort");
  xSemaphoreGive(dataMutex);
  show24HR = server.hasArg("show24HR");
  invertColours = server.hasArg("invertColours");
  tzOverride = server.hasArg("tzOverride");
  if (server.hasArg("tzOverrideOffset")) tzOverrideOffset = server.arg("tzOverrideOffset").toInt();
  ntfyEnabled = server.hasArg("ntfyEnabled");
  if (server.hasArg("ntfyServer")) {
    ntfyServer = server.arg("ntfyServer");
    ntfyServer.trim();
  }
  if (server.hasArg("ntfyPort")) {
    ntfyPort = server.arg("ntfyPort");
    ntfyPort.trim();
  }
  if (server.hasArg("ntfyTopic")) {
    ntfyTopic = server.arg("ntfyTopic");
    ntfyTopic.trim();
  }
  if (server.hasArg("ntfyToken")) {
    ntfyToken = server.arg("ntfyToken");
    ntfyToken.trim();
  }
  if (server.hasArg("ntfyStallMin")) ntfyStallMin = server.arg("ntfyStallMin").toInt();

  // Build the full ntfy URL
  if (!ntfyServer.startsWith("http://") && !ntfyServer.startsWith("https://")) {
    bool isIP = true;
    for (char c : ntfyServer) {
      if (!isDigit(c) && c != '.') {
        isIP = false;
        break;
      }
    }
    ntfyServer = isIP ? "http://" + ntfyServer + ":" + ntfyPort
                      : "https://" + ntfyServer;
  }

  writeSettings();  // ← everything is set before we save

  // Reset display
  tft.invertDisplay(invertColours);
  tft.fillRect(0, clockBottomY, 239, SCREEN_H - clockBottomY, TFT_BLACK);
  drawVersionString();
  printerName = "";
  showSleep = false;
  showIdle = false;
  saverOn = false;
  justFinished = false;
  currentState = STATE_IDLE;
  lastState = STATE_IDLE;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  netReset = true;
  lastSeq = sharedData.seq;  // skip any old result
  xSemaphoreGive(dataMutex);
  xTaskNotify(netTaskHandle, NET_POLL_NOW, eSetBits);
  server.sendHeader("Location", "/?saved=1");  // back to the page, popup says it's saved
  server.send(303);
}

// Saved popup: a click anywhere closes it. The ?saved=1 comes off the address, so a refresh doesn't show it again.
String buildSavedPopup() {
  return "<div class='mbg open' onclick='this.classList.remove(\"open\")'>"
         "<div class='modal okm'><h3>" + String(wcSaved) + "</h3>"
         "<div class='mbtns'><button type='button' class='mok okb'>" + String(wcOkBtn) + "</button></div>"
         "</div></div>"
         "<script>history.replaceState(null,'','/');</script>";
}

// Every UTC offset in use, in minutes, for the override dropdown
static const int16_t TZ_CHOICES[] = {
  -720, -660, -600, -570, -540, -480, -420, -360, -300, -240, -210, -180, -150, -120, -60,
  0, 60, 120, 180, 210, 240, 270, 300, 330, 345, 360, 390, 420, 480, 525, 540, 570,
  600, 630, 660, 720, 765, 780, 825, 840
};
static const uint8_t TZ_CHOICE_COUNT = sizeof(TZ_CHOICES) / sizeof(TZ_CHOICES[0]);

// "UTC−8" or "UTC+5:30"; full always shows the minutes ("UTC−8:00")
String utcLabel(int32_t secs, bool full) {
  int32_t mins = abs(secs) / 60;
  String s = String("UTC") + (secs < 0 ? "&minus;" : "+") + String(mins / 60);
  if (full || mins % 60) s += (mins % 60 < 10 ? ":0" : ":") + String(mins % 60);
  return s;
}

// Override toggle + offset list, the service's offset below
// Box off: the list starts at the received offset, so it's one step up or down to fix
String tzOverrideField() {
  int32_t shown = tzOverride ? tzOverrideOffset : tzOffset;
  String html = "<div><div class='tzbox'><label class='toggle'><input type='checkbox' name='tzOverride'"
                + String(tzOverride ? " checked" : "") + "><span class='slider'></span></label>"
                "<select name='tzOverrideOffset'>";
  for (uint8_t i = 0; i < TZ_CHOICE_COUNT; i++) {
    int32_t secs = TZ_CHOICES[i] * 60;
    html += "<option value='" + String(secs) + "'" + (secs == shown ? " selected" : "") + ">"
            + utcLabel(secs, true) + "</option>";
  }
  html += "</select></div><div class='tzrecv'>" + String(wcTzReceived) + ": " + utcLabel(tzOffset, false) + "</div></div>";
  return html;
}

String getPrinterSetup() {
  String printerForm = String(printer_Info);
  printerForm.replace("%LBL_PRINTER_IP%", String(wcPrinterIP));
  printerForm.replace("%LBL_PRINTER_PORT%", String(wcPrinterPort));
  printerForm.replace("%LBL_CLOCK24%", String(wcClock24));
  printerForm.replace("%IP%", printerIP);
  printerForm.replace("%PORT%", printerPort);
  printerForm.replace("%boxState%", show24HR ? "checked" : "unchecked");
  printerForm.replace("%LBL_INVERT%", String(wcInvert));
  printerForm.replace("%LBL_INVERT_HINT%", String(wcInvertHint));
  printerForm.replace("%invertState%", invertColours ? "checked" : "");
  printerForm.replace("%LBL_TZ_OVERRIDE%", String(wcTzOverride));
  printerForm.replace("%TZ_OVERRIDE%", tzOverrideField());
  return printerForm;
}

void writeSettings() {
  File f = LittleFS.open(CONFIG, "w");
  if (!f) {
    Serial.println("Settings write failed!");
    return;
  }
  f.println("printerIP=" + printerIP);
  f.println("printerPort=" + printerPort);
  f.println("show24HR=" + String(show24HR));
  f.println("invertColours=" + String(invertColours));
  f.println("ntfyPort=" + ntfyPort);  // write the port number if there is one
  f.println("ntfyEnabled=" + String(ntfyEnabled));
  f.println("ntfyServer=" + ntfyServer);
  f.println("ntfyTopic=" + ntfyTopic);
  f.println("ntfyToken=" + ntfyToken);
  f.println("ntfyStallMin=" + String(ntfyStallMin));
  f.println("tzOffset=" + String(tzOffset));
  f.println("tzOverride=" + String(tzOverride));
  f.println("tzOverrideOffset=" + String(tzOverrideOffset));
  f.close();
}

void readSettings() {
  if (!LittleFS.exists(CONFIG)) {
    writeSettings();
    return;
  }
  File fr = LittleFS.open(CONFIG, "r");
  String line;
  while (fr.available()) {
    line = fr.readStringUntil('\n');
    if (line.indexOf("printerIP=") >= 0) {
      printerIP = line.substring(10);
      printerIP.trim();
    }
    if (line.indexOf("printerPort=") >= 0) {
      printerPort = line.substring(12);
      printerPort.trim();
    }
    if (line.indexOf("show24HR=") >= 0) show24HR = line.substring(9).toInt();
    if (line.startsWith("invertColours=")) invertColours = line.substring(14).toInt();
    if (line.indexOf("ntfyPort=") >= 0) {
      ntfyPort = line.substring(9);
      ntfyPort.trim();
    };  // read custom port local host
    if (line.indexOf("ntfyEnabled=") >= 0) ntfyEnabled = line.substring(12).toInt();
    if (line.indexOf("ntfyServer=") >= 0) {
      ntfyServer = line.substring(11);
      ntfyServer.trim();
    }
    if (line.indexOf("ntfyTopic=") >= 0) {
      ntfyTopic = line.substring(10);
      ntfyTopic.trim();
    }
    if (line.indexOf("ntfyToken=") >= 0) {
      ntfyToken = line.substring(10);
      ntfyToken.trim();
    }
    if (line.indexOf("ntfyStallMin=") >= 0) ntfyStallMin = line.substring(13).toInt();
    if (line.indexOf("tzOffset=") >= 0) tzOffset = line.substring(9).toInt();
    if (line.startsWith("tzOverride=")) tzOverride = line.substring(11).toInt();
    if (line.startsWith("tzOverrideOffset=")) tzOverrideOffset = line.substring(17).toInt();
  }
  fr.close();
}

String buildQueryURL(const String &base) {
  String url = base + printQuery;
  if (hasChamber) {
    String encodedName = chamberSensorName;
    encodedName.replace(" ", "+");
    url += "&" + encodedName;
  }
  return url;
}

// ============================================================
//  BMP DRAW FROM LittleFS
// ============================================================
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y) {
  if ((x >= tft.width()) || (y >= tft.height())) return;

  File bmpFS = fs.open(filename, "r");
  if (!bmpFS) {
    Serial.print("BMP not found: ");
    Serial.println(filename);
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;
  uint8_t r, g, b;

  if (read16(bmpFS) == 0x4D42) {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
      y += h - 1;
      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {
        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t *bptr = lineBuffer;
        uint16_t *tptr = (uint16_t *)lineBuffer;
        for (uint16_t col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);
    } else {
      Serial.println("BMP must be 24-bit uncompressed.");
    }
  }
  bmpFS.close();
}

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

// ============================================================
//  FILENAME HELPER
// ============================================================
String extractFileName(const String &path, bool withExt) {
  int slashIdx = path.lastIndexOf('/');
  int start = (slashIdx >= 0) ? slashIdx + 1 : 0;

  String result;
  if (!withExt) {
    int dotIdx = path.lastIndexOf('.');
    int end = (dotIdx > start) ? dotIdx : path.length();
    result = path.substring(start, end);
  } else {
    result = path.substring(start);
  }

  return result;
}

// Filename on its line: centred if it fits, otherwise left justified and cut with "..." so it never wraps.
// Uses the loaded font and the colour already set.
void drawFileName() {
  const int16_t maxW = SCREEN_W - 4;
  String s = thePrintFile;
  uint8_t datum = tft.getTextDatum();
  if (tft.textWidth(s) <= maxW) {
    tft.setTextDatum(BC_DATUM);
    tft.drawString(s, SCREEN_W / 2, filenameY);
  } else {
    while (s.length() && tft.textWidth(s + "...") > maxW) {
      while (s.length() && (s[s.length() - 1] & 0xC0) == 0x80) s.remove(s.length() - 1);  // whole UTF-8 characters only
      if (s.length()) s.remove(s.length() - 1);
    }
    tft.setTextDatum(BL_DATUM);
    tft.drawString(s + "...", 2, filenameY);
  }
  tft.setTextDatum(datum);
}

// ============================================================
//  WIFI RESET
// ============================================================
void handleWifiReset() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(strWifiReset, SCREEN_W / 2, SCREEN_H / 2, 2);
  delay(5000);
  redirectHome();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
}

void redirectHome() {
  server.sendHeader("Location", String("/"), true);
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");
  server.client().stop();
}

// Called by PNGdec for each decoded row
int pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[110];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(graphicX, graphicY + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;  // return 1 to continue decoding
}

// ============================================================
//  THUMBNAIL
// ============================================================
void requestThumbnail(const String &rawPath) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  thumbRequest = rawPath;
  xSemaphoreGive(dataMutex);
  xTaskNotify(netTaskHandle, NET_THUMB, eSetBits);
}

void handleThumbnail() {
  if (!thumbDone) return;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  uint8_t *buf = thumbBuffer;
  int len = thumbBufferSize;
  thumbBuffer = nullptr;
  thumbDone = false;
  xSemaphoreGive(dataMutex);

  if (printerName != "" && currentState == STATE_PRINTING) {
    if (!buf || !drawThumbnail(buf, len)) {  // PNG decode to screen
      drawBmp(LittleFS, PRINTING_IMAGE, graphicX + 7, graphicY + 7);
    }
  }
  free(buf);
}

bool drawThumbnail(uint8_t *buf, int len) {
  int rc = png.openRAM(buf, len, pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);
    tft.endWrite();
    png.close();
  } else {
    Serial.println("PNG open failed: " + String(rc));
  }
  return (rc == PNG_SUCCESS);
}

void serviceThumbnail(const String &ip, const String &port) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  String rawPath = thumbRequest;
  thumbRequest = "";
  xSemaphoreGive(dataMutex);
  if (rawPath == "") return;

  int len = 0;
  uint8_t *buf = fetchThumbnail(ip, port, rawPath, len);  // HTTP download into RAM

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  free(thumbBuffer);
  thumbBuffer = buf;
  thumbBufferSize = len;
  thumbDone = true;
  xSemaphoreGive(dataMutex);
}

uint8_t *fetchThumbnail(const String &ip, const String &port, const String &rawPath, int &len) {
  String thumbFile = rawPath;
  int slashIdx = thumbFile.lastIndexOf('/');
  if (slashIdx >= 0) thumbFile = thumbFile.substring(slashIdx + 1);
  int dotIdx = thumbFile.lastIndexOf('.');
  if (dotIdx > 0) thumbFile = thumbFile.substring(0, dotIdx);

  String encodedFile = thumbFile;
  encodedFile.replace(" ", "%20");
  encodedFile.replace("(", "%28");
  encodedFile.replace(")", "%29");
  encodedFile.replace("[", "%5B");
  encodedFile.replace("]", "%5D");
  encodedFile.replace("&", "%26");
  encodedFile.replace("+", "%2B");

  // Standard Moonraker path (works for the vast majority of printers)
  String thumbURL = "http://" + ip + ":" + port + "/server/files/gcodes/.thumbs/" + encodedFile + "-110x110.png";

  HTTPClient http;
  beginHTTP(http, thumbURL);
  int httpCode = http.GET();

  // Fallback: Creality K1/K1 Max serve thumbnails via Nginx on port 80,
  // under a mistyped "/downloads/humbnail/" path instead of Moonraker's API.
  if (httpCode != 200) {
    http.end();
    delay(150);  // let the socket close cleanly before firing the second request
    thumbURL = "http://" + ip + "/downloads/humbnail/" + encodedFile + ".png";
    Serial.println("Standard thumbnail path failed, trying Creality fallback: " + thumbURL);
    beginHTTP(http, thumbURL);
    httpCode = http.GET();
  }

  if (httpCode != 200) {
    Serial.println("Thumb fetch failed: " + String(httpCode));
    http.end();
    return nullptr;
  }

  len = http.getSize();
  if (len <= 0) {
    Serial.println("Thumb: invalid content length, aborting");
    http.end();
    return nullptr;
  }

  uint8_t *buf = (uint8_t *)malloc(len);
  if (!buf) {
    Serial.println("malloc failed!");
    http.end();
    return nullptr;
  }

  WiFiClient *stream = http.getStreamPtr();
  int bytesRead = 0;
  uint32_t lastData = millis();
  while (http.connected() && bytesRead < len && millis() - lastData < HTTP_RESPONSE_TIMEOUT) {
    int avail = stream->available();
    int n = (avail > 0) ? stream->read(buf + bytesRead, min(avail, len - bytesRead)) : 0;
    if (n > 0) {
      bytesRead += n;
      lastData = millis();
    } else {
      delay(1);
    }
  }
  http.end();

  Serial.println("Bytes read: " + String(bytesRead));
  len = bytesRead;
  return buf;
}