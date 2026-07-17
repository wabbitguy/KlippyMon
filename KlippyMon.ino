/* ESP32 Dev Module 
   NO OTA 2MB APP/2MB SPIFFS
*/

#include "Settings.h"     // some basic settings (TimeZone)
#include "Language.h"     // language you want to use
#include "Translation.h"  // language translations
#include "Ntfy.h"         // for push nofitications if you want them
#include "NTP_Time.h"     // time server change

//#define FORMAT_LittleFS  // Wipe LittleFS and all files! Disable after use.

#define VERSION "v3.6"
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

bool enablePoll = false;
uint8_t thePollTime = 10;

bool forcePoll = true;  // true at boot, true after settings update

float savedTotalDuration = 0.0;  // save the total duration so it doesn't get nop'd out

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
void handlePolling(int8_t theSeconds);
void handleGaugeHeadings();
void handleGauge(uint8_t whichGauge, int16_t gaugeValue);
void setRGB(bool redLevel, bool greenLevel, bool blueLevel);
void handleHostName();
void handleTimeUsed();
void handleETA();
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result);
bool fetchPrinterData();
PrinterState determinePrinterState();
void updatePrinterDisplay(PrinterState state);
void handlePrinterStatus();
void configModeCallback(WiFiManager *myWiFiManager);
String SendHTML();
void handlePrinterUpdate();
String getPrinterSetup();
void writeSettings();
void readSettings();
void buildPrinterURLs();
String extractFileName(const String &path, bool withExt);
void handleWifiReset();
void redirectHome();
void drawWiFiQuality();
int8_t getWifiQuality();
void handle_OnConnect();
void drawVersionString();
bool fetchAndDrawThumbnail();
int pngDraw(PNGDRAW *pDraw);
void fetchPrinterLimits();                            // get the printers default limits
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
  server.send(200, "text/html", SendHTML());
}

void setup() {
  Serial.begin(115200);

  tft.init();
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

  readSettings();
  if (checkTimezoneOffsets()) writeSettings();
  buildPrinterURLs();
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

  if (enablePoll == true) {
    if (printerName == "") {
      handleHostName();
      handlePrinterOffLine();
    } else {
      handlePrinterStatus();
    }
    enablePoll = false;
  }

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

  handlePolling(mySecond);

  if (mySecond == (thePollTime + 5)) {
    drawWiFiQuality();
  }
}

// ============================================================
//  POLLING TRIGGER
// ============================================================
void handlePolling(int8_t theSeconds) {
  if (forcePoll) {
    enablePoll = true;
    forcePoll = false;  // clear it, back to normal timer polling
    return;
  }
  if ((theSeconds % thePollTime) == 0) {
    enablePoll = true;
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
    }
  } else {
    tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
    tft.loadFont(AA_FONT_SMALL, LittleFS);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(printerName, 120, printerNameY);
    tft.unloadFont();
    handleGaugeHeadings();
    handlePrinterStatus();
    drawVersionString();
  }
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
//  FIND PRINTER HOSTNAME
// ============================================================
void handleHostName() {
  if (WiFi.status() == WL_CONNECTED) {
    setRGB(0, greenON, 0);
    HTTPClient http;
    beginHTTP(http, printerURLInfo);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        printerName = doc["result"]["hostname"] | "";
        Serial.println(printerName);
      }
    }
    http.end();
    setRGB(0, greenOFF, 0);
    if (printerName != "") {
      fetchPrinterLimits();
      buildPrinterURLs();
      lastBedTemp = 0;
      lastNozzleTemp = 0;
      lastProgress = 0;
    }
  }
}

// ============================================================
//  PHASE 1 — FETCH & PARSE
// ============================================================
bool fetchPrinterData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost");
    return false;
  }
  setRGB(0, greenON, 0);
  HTTPClient http;
  beginHTTP(http, printerURLQ);
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

  nozzleTarget = doc["result"]["status"]["extruder"]["target"] | 0.0;
  bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;
  printState = doc["result"]["status"]["print_stats"]["state"] | "unknown";
  nozzleTemp = doc["result"]["status"]["extruder"]["temperature"] | 0.0;
  bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
  printDuration = doc["result"]["status"]["print_stats"]["print_duration"] | 0.0;
  totalDuration = doc["result"]["status"]["print_stats"]["total_duration"] | 0.0;
  if (totalDuration > 0) savedTotalDuration = totalDuration;
  progress = doc["result"]["status"]["display_status"]["progress"] | 0.0;

  if (thePrintFile == "" && printState != "standby"
      && printState != "canceled" && printState != "cancelled"
      && printState != "error") {
    String rawPath = doc["result"]["status"]["print_stats"]["filename"] | "";
    if (rawPath != "") {
      thePrintFileRaw = rawPath;
      thePrintFile = extractFileName(rawPath, false);
    }
  }

if (hasChamber && chamberSensorName.length() > 0 &&
      doc["result"]["status"].containsKey(chamberSensorName.c_str())) {
    chamberTemp = doc["result"]["status"][chamberSensorName.c_str()]["temperature"] | 0.0;
    chamberTarget = doc["result"]["status"][chamberSensorName.c_str()]["target"] | 0.0;
  }

  return true;
}

// ============================================================
//  FETCH PRINTER TEMP LIMITS FROM KLIPPER CONFIG
// ============================================================
void fetchPrinterLimits() {
  HTTPClient http;
  beginHTTP(http, "http://" + printerIP + ":" + printerPort + "/printer/objects/query?configfile");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String nozzle = doc["result"]["status"]["configfile"]["config"]["extruder"]["max_temp"] | "350";
      String bed = doc["result"]["status"]["configfile"]["config"]["heater_bed"]["max_temp"] | "120";
      maxNozzleTemp = nozzle.toInt();
      maxBedTemp = bed.toInt();
      writeSettings();
    } else {
      Serial.println("fetchPrinterLimits JSON error");
    }
  } else {
    Serial.printf("fetchPrinterLimits failed: %d\n", httpCode);
  }
  http.end();

  // ── Chamber detection ─────────────────────────────────────
  beginHTTP(http, "http://" + printerIP + ":" + printerPort + "/printer/objects/list");
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
  tft.drawString(thePrintFile, 120, filenameY);
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
        if (!fetchAndDrawThumbnail()) {
          drawBmp(LittleFS, PRINTING_IMAGE, graphicX + 7, graphicY + 7);
        }
        tft.loadFont(AA_FONT_SMALL, LittleFS);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);
        tft.fillRect(0, filenameY - 14, 239, 16, TFT_BLACK);  // clear filename line
        tft.drawString(thePrintFile, 120, filenameY);
        tft.unloadFont();
        handleGauge(progressGauge, 0);  // ← start at 0% immediately
        lastProgress = 0;               // ← force redraw on first real progress
      }

      progressPercent = uint16_t(progress * 100.0);
      if (progressPercent != lastProgress) {
        lastProgress = progressPercent;
        handleGauge(progressGauge, lastProgress);
      }
      ntfyCheckStall(progress);  // was progressPercent

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
  static uint8_t failCount = 0;

  if (!fetchPrinterData()) {
    failCount++;
    Serial.println("fetchPrinterData failed - count: " + String(failCount));
    if (failCount >= 3) {  // 3 consecutive failures (~30 seconds) before giving up
      failCount = 0;
      printerName = "";
      tft.fillRect(0, belowClockY, 239, SCREEN_H - belowClockY, TFT_BLACK);
      drawVersionString();
      showSleep = false;
      showIdle = false;
    }
    return;
  }

  failCount = 0;  // reset on success
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
  tft.drawString("Access Point Active", SCREEN_W / 2, (SCREEN_H / 2) + 36, 2);
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
String SendHTML() {
  String page = String(HTML_TEMPLATE);

  // Header
  page.replace("%VERSION%", String(VERSION));
  page.replace("%WIFI_QUALITY%", String(getWifiQuality()));

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
  if (server.hasArg("printerIP")) printerIP = server.arg("printerIP");
  if (server.hasArg("printerPort")) printerPort = server.arg("printerPort");
  show24HR = server.hasArg("show24HR");
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
  tft.fillRect(0, clockBottomY, 239, SCREEN_H - clockBottomY, TFT_BLACK);
  drawVersionString();
  printerName = "";
  showSleep = false;
  showIdle = false;
  justFinished = false;
  currentState = STATE_IDLE;
  lastState = STATE_IDLE;
  forcePoll = true;
  buildPrinterURLs();
  server.send(200, "text/html", SendHTML());
}

String getPrinterSetup() {
  String printerForm = String(printer_Info);
  printerForm.replace("%IP%", printerIP);
  printerForm.replace("%PORT%", printerPort);
  printerForm.replace("%boxState%", show24HR ? "checked" : "unchecked");
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
  f.println("ntfyPort=" + ntfyPort);  // write the port number if there is one
  f.println("ntfyEnabled=" + String(ntfyEnabled));
  f.println("ntfyServer=" + ntfyServer);
  f.println("ntfyTopic=" + ntfyTopic);
  f.println("ntfyToken=" + ntfyToken);
  f.println("ntfyStallMin=" + String(ntfyStallMin));
  f.println("tzOffset=" + String(tzOffset));
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
  }
  fr.close();
}

void buildPrinterURLs() {
  printerURLInfo = "http://" + printerIP + ":" + printerPort + printerINFO;
  printerURLQ = "http://" + printerIP + ":" + printerPort + printQuery;
  if (hasChamber) {
    String encodedName = chamberSensorName;
    encodedName.replace(" ", "+");
    printerURLQ += "&" + encodedName;
  }
  // Serial.println("buildPrinterURLs hasChamber: " + String(hasChamber));
  // Serial.println("printerURLQ: " + printerURLQ);
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

  if (result.length() > 28) result = result.substring(0, 28) + "~";
  return result;
}

// ============================================================
//  WIFI RESET
// ============================================================
void handleWifiReset() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("WiFi reset in 5 seconds", SCREEN_W / 2, SCREEN_H / 2, 2);
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

bool fetchAndDrawThumbnail() {
  String thumbFile = thePrintFileRaw;
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
  String thumbURL = "http://" + printerIP + ":" + printerPort +
                     "/server/files/gcodes/.thumbs/" + encodedFile + "-110x110.png";

  httpThumb.begin(thumbURL);
  int httpCode = httpThumb.GET();

  // Fallback: Creality K1/K1 Max serve thumbnails via Nginx on port 80,
  // under a mistyped "/downloads/humbnail/" path instead of Moonraker's API.
  if (httpCode != 200) {
    httpThumb.end();
    delay(150);  // let the socket close cleanly before firing the second request
    thumbURL = "http://" + printerIP + "/downloads/humbnail/" + encodedFile + ".png";
    Serial.println("Standard thumbnail path failed, trying Creality fallback: " + thumbURL);
    httpThumb.begin(thumbURL);
    httpCode = httpThumb.GET();
  }

  if (httpCode != 200) {
    Serial.println("Thumb fetch failed: " + String(httpCode));
    httpThumb.end();
    return false;
  }

  thumbBufferSize = httpThumb.getSize();
  if (thumbBufferSize <= 0) {
    Serial.println("Thumb: invalid content length, aborting");
    httpThumb.end();
    return false;
  }

  thumbBuffer = (uint8_t *)malloc(thumbBufferSize);
  if (!thumbBuffer) {
    Serial.println("malloc failed!");
    httpThumb.end();
    return false;
  }

  WiFiClient *stream = httpThumb.getStreamPtr();
  int bytesRead = 0;
  while (httpThumb.connected() && bytesRead < thumbBufferSize) {
    if (stream->available()) {
      thumbBuffer[bytesRead++] = stream->read();
    }
  }
  httpThumb.end();

  Serial.println("Bytes read: " + String(bytesRead));

  int rc = png.openRAM(thumbBuffer, bytesRead, pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);
    tft.endWrite();
    png.close();
  } else {
    Serial.println("PNG open failed: " + String(rc));
  }

  free(thumbBuffer);
  thumbBuffer = nullptr;

  return (rc == PNG_SUCCESS);
}