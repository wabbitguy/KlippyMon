#include "Settings.h"
#include "NotoSansBold15.h"

//#define FORMAT_LittleFS  // Wipe SPIFFS and all files!
//
#define VERSION "v2.1"
#define hostNameCYD "KlippyMon"  // this will be the AP name you set to setup wifi
#define CONFIG "/config.txt"     // where all the settings are stored

#define nozzleGauge 1
#define progressGauge 2
#define bedGauge 3
#define headingY 132      // used for the graph headings
#define gaugeY 170        // used for the gauges
#define dataY gaugeY + 6  ///
//
time_t printStart;  //actual total print time start to finish
time_t printEnd;
bool gotStartTime = false;

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

// setup the SDcard now
int sd_init_flag = 0;
SPIClass SD_SPI;  // for the graphics on the SDcard
File root;
//
bool foundPrinter = false;
String printerName = "";  // name of the printer we found
//
#define RED_PIN 22
#define GREEN_PIN 16
#define BLUE_PIN 17

// these are variables where the data returned from the printer query is stored
String layerText, tempLine, printState;
float progress, nozzleTemp, nozzleTarget, bedTemp, bedTarget, printDuration, totalDuration;
uint16_t progressPercent;
uint16_t lastNozzleTemp, lastBedTemp, lastProgress;  // we only update as needed
bool greenON = true, greenOFF = false;               // for the RGB LED on the back

// ---------------- DISPLAY ----------------
#define SCREEN_W 240
#define SCREEN_H 320
uint8_t thePollTime = 10;  // how often to look for the printer/info
bool enablePoll = false;   // we handle this every x seconds
bool showSleep = false;    // show the sleeping printer
bool showHeat = false;     // showing heat icon
bool inPrepMode = false;   // are we getting ready
bool showIdle = false;
//
// ----------- CLOCK --------------
bool clock24 = false;  // not implemented yet
uint8_t lastSecond = 99;
uint8_t lastMinute;
uint8_t lastHour;
uint8_t lastDay;
uint8_t lastMonth;
uint16_t lastYear, myYear;
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myMonth, myWeekDay, tempMonth;
bool colonBlink = false;  // we want the colon to blink
bool activeETA = false;
uint16_t etaHH;
uint16_t etaMM;
uint8_t futureHour12, futureHour24, futureMinute;  // gives the time when the print will be done

// ---------------- TIME ----------------
#define NTP_SERVER "pool.ntp.org"
#define NTP_RESYNC_INTERVAL (4 * 3600)
unsigned long lastNtpSync = 0;
static const char ntpServerName[] = "pool.ntp.org";
uint16_t localPort;
uint8_t ntpUpdateFrequency = 123;  // update the time every x minutes
WiFiUDP Udp;

// Set web server port number to 80
WebServer server(80);

void handlePrinterOffLine();                                                            // draws the graphic when the printer is offline
void printDirectory(File dir, int numTabs);                                             // just for debugging the SD card
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y);                   // display BMP files (24 bit only)
uint16_t read16(fs::File &f);                                                           // used for reading the BMP
uint32_t read32(fs::File &f);                                                           // used for reading the BMP
int sd_init();                                                                          // init the SDcard
void handle_ClockDisplay();                                                             // where most of the magic is done...
void handlePolling(int8_t theSeconds);                                                  // how often the printer is looked for
void handleGaugeHeadings();                                                             // shows the gauge headings only
void handleGauge(uint8_t whichGauge, int16_t gaugeValue);                               // handles each gauge individually
void setRGB(bool redLevel, bool greenLevel, bool blueLevel);                            // for the LED on the back of the CYD
void handleHostName();                                                                  // searches for the printer at the IP and gets it's hostname (QIDI default would be mkspi)
void handleTimeUsed();                                                                  // this is a running total from the time of heating to the end head park
void handleETA();                                                                       // displays how many hrs, minutes are remaining.
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result);  // calculates ETA
void handlePrinterStatus();                                                             // when the printer is online, this sends requests to see what it's doing (standby, printing)
void configModeCallback(WiFiManager *myWiFiManager);                                    // for displaying the access point to connect to setup wifi
time_t getNtpTime();                                                                    // gets the time from the NTP pool
void sendNTPpacket(IPAddress &address);                                                 // asks for the time from the NTP pool
String SendHTML();                                                                      // web page that shows when you connect to KlippyMon with a browser
void handlePrinterUpdate();                                                             // when you change settings in the web page, this updates things.
String getPrinterSetup();                                                               // for the web page to display current printer settings
void writeSettings();                                                                   // write the settings to memory
void readSettings();                                                                    // read the settings from memory
void buildPrinterURLs();                                                                // build the http strings to find and check the printer on the LAN
String extractFileName(const String &path, bool withExt);                               // extract the current printing filament
void handleWifiReset();                                                                 // reset the WIFI KlippyMon connects to
void redirectHome();                                                                    // after a reset say bye bye to the client
void drawGrid();                                                                        // just for working on line spacing
//
void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}
//
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);

// Enable if you want to erase SPIFFS, this takes some time!
// then disable and reload sketch to avoid reformatting on every boot!
#ifdef FORMAT_LittleFS
  tft.setTextDatum(MC_DATUM);  // Middle Centre datum
  tft.drawString("Formatting LittleFS, so wait!", 120, 195);
  LittleFS.format();
  ESP.restart();  // then reboot
#endif
  //
  if (!LittleFS.begin(true)) {  // get the file system running
    Serial.println("Flash FS initialization failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }
  Serial.println("Flash FS available!");
  //
  SD.begin();
  root = SD.open("/");

  // and now the RGB LED on the back of the CYD
  // Only the green is used
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(0, 0, 0);
  //
  //Local intialization.
  WiFiManager wifiManager;
  //AP Configuration
  wifiManager.setHostname("KlippyMon");  // sets the custom DNS name that appears in your router
  wifiManager.setAPCallback(configModeCallback);
  //Exit After Config Instead of connecting
  wifiManager.setBreakAfterConfig(true);
  // --- now connect to the last Wifi point
  if (!wifiManager.autoConnect(hostNameCYD)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Seed Random With values Unique To This Device
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  String ipaddress = WiFi.localIP().toString();
  Udp.begin(localPort);  // port to use
  setSyncProvider(getNtpTime);
  //Set Sync Intervals
  setSyncInterval(ntpUpdateFrequency * 60);
  //
  // OTA Setup
  WiFi.hostname(hostNameCYD);
  MDNS.begin(hostNameCYD);  // start the MDNS server...
  ArduinoOTA.setHostname(hostNameCYD);
  // ArduinoOTA.setPassword((const char *)"12345");
  ArduinoOTA.begin();

  // pages we allow for the web server
  server.on("/", handle_OnConnect);                      // show a default page with all the settings
  server.on("/updatePrinterInfo", handlePrinterUpdate);  // if updated, go extract values
  server.on("/wifiReset", handleWifiReset);              // reset the WIFI (in the event the LAN router changes)
  // server.on("/updateconfig", handleUpdateConfig);
  // server.on("/updateweatherconfig", handleUpdateWeather);
  // server.on("/configure", handleConfigure);
  server.begin();  // and start the server

  // add an MDNS so you can use KlippyMon.local instead of IP addres
  MDNS.addService("http", "tcp", 80);

  tft.fillScreen(TFT_BLACK);  // erase the TFT
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);  //
  tft.drawString(String(VERSION) + " @2026 - Wabbit Wanch Design", 120, 310, 2);

  // now we read in the default settings from the settings.h portion
  readSettings();      // go read in the default settings
  buildPrinterURLs();  // now build the URL's from the last saved data
}
//
void loop() {
  static time_t prevDisplay = 0;
  timeStatus_t ts = timeStatus();
  utc = now();
  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      //update the schedule checking only if time has changed
      if (now() != prevDisplay) {
        prevDisplay = now();
        handle_ClockDisplay();  // go handle the schedule if it's on
        tmElements_t tm;
        breakTime(now(), tm);
      }
      break;
    case timeNotSet:
      now();
      delay(3000);
  }
  if (enablePoll == true) {
    if (printerName == "") {   // we need to find the printer name first
      handleHostName();        // see if it's online yet fetch the host name
      handlePrinterOffLine();  // handle no printer online
    } else {
      handlePrinterStatus();  // go handle the requests to the printer
      handleETA();            // update the ETA of when it should be done
    }
    enablePoll = false;  // reset the polling trigger
  }
  ArduinoOTA.handle();
  server.handleClient();
}
//
// -------- WIFI Signal Strength ---------
//
void drawWiFiQuality() {
  const byte numBars = 5;                      // set the number of total bars to display
  const byte barWidth = 3;                     // set bar width, height in pixels
  const byte barHeight = 20;                   // should be multiple of numBars, or to indicate zero value
  const byte barSpace = 1;                     // set number of pixels between bars
  const uint16_t barXPosBase = SCREEN_W - 25;  // set the X-pos for drawing the bars
  const byte barYPosBase = 20;                 // set the baseline Y-pos for drawing the bars
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {  // current bar loop
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {  // draw bar height loop
      for (byte ii = 0; ii < barWidth; ii++) {    // draw bar width loop
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
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}
//
void handlePrinterOffLine() {
  if (printerName == "") {                                          // draw the sleeping printer
    if (showSleep == false) {                                       // we only show the printer graphic ONCE
      drawBmp(SD, OFFLINE_IMAGE, 27, 100);                          //display sleeping printer
      String ipaddress = "KlippyMon " + WiFi.localIP().toString();  // get my IP address
      tft.setTextColor(TFT_WHITE, TFT_BLACK);                       // white text
      tft.setTextDatum(BC_DATUM);
      tft.drawString(ipaddress, 120, 294, 2);  // show the IP address of KlippyMon
      showSleep = true;                        // only show it once...
    }
  } else {                                     // the printer is AWAKE!
    tft.fillRect(0, 76, 239, 220, TFT_BLACK);  // get rid of anything there it won't be back here.
    uint8_t xpos = (SCREEN_W / 2) - 1;
    uint8_t ypos = 106;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Now show the printer name
    tft.setTextDatum(BC_DATUM);
    tft.drawString(printerName, xpos, ypos, 4);  // show the name
    handleGaugeHeadings();                       // show the headings
    handlePrinterStatus();                       // go handle the requests to the printer
  }
}
//
// ---------------- BMP DRAW ----------------
void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
//
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y) {
  if ((x >= tft.width()) || (y >= tft.height())) return;

  // Open requested file on SD card
  File bmpFS = fs.open(filename, "r");
  if (!bmpFS) {
    Serial.print("File not found");
    digitalWrite(SD_CS, HIGH);  // Always release
    SPI.endTransaction();       // Release HSPI bus
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;  //, col;
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
        // Convert 24 to 16-bit colours
        for (uint16_t col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);
      // Serial.print("Loaded in ");
      // Serial.print(millis() - startTime);
      // Serial.println(" ms");
    } else Serial.println("BMP format not recognized.");
  }
  bmpFS.close();
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}
//
int sd_init() {
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS, SD_SPI, 40000000)) {
    Serial.println("Card Mount Failed");
    return 1;
  } else {
    Serial.println("Card Mount Successed");
  }
  // listDir(SD, "/", 0);
  // Serial.println("TF Card init over.");
  sd_init_flag = 1;
  return 0;
}
//
void handle_ClockDisplay() {
  tmElements_t tm;
  breakTime(now(), tm);
  char buffer[24];  // holds the formated time/day/date
  uint8_t theHour;  // either 12/24hr format
  //

  // --- Current local time ---
  time_t utc = now();
  time_t localTime = timeZoneRule.toLocal(utc, &tcr);

  myHour = hourFormat12(localTime);
  myDay = day(localTime);
  my24Hour = hour(localTime);    // 24-hour clock
  myMinute = minute(localTime);  // use localTime
  mySecond = second(localTime);  // use localTime
  myYear = year(localTime);
  myMonth = month(localTime);  // 1 to 12
                               //
  // now we draw the time and the date
  uint8_t xpos = (SCREEN_W / 2) - 1;
  uint8_t ypos = 71;
  tft.setTextColor(TFT_GREEN, TFT_BLACK);  // Now show the clock time
  tft.setTextDatum(BC_DATUM);
  if (show24HR == true) {
    theHour = my24Hour;  // we show this one
  } else {
    theHour = myHour;  // typical 12hr display
  }
  if (colonBlink == false) {
    sprintf(buffer, " %2u:%02u ", theHour, myMinute);  // kill the leading 0 on the hours
  } else {
    sprintf(buffer, " %2u %02u ", theHour, myMinute);  // kill the leading 0 on the hours
  }
  colonBlink = !colonBlink;               // toggle the value for next pass
  tft.drawString(buffer, xpos, ypos, 7);  // Overwrite the text to clear it
  tft.setTextPadding(0);                  // Reset padding width to none
  handlePolling(mySecond);                // go check for a trigger
  if (mySecond == (thePollTime + 5)) {    // don't check the same time as the printer
    drawWiFiQuality();                    // draw the wifi strength
  }
  //
  lastSecond = mySecond;             // save it for next time
  lastMinute = myMinute;             // save this for the next pass
  lastHour = my24Hour;               // save for the next pass
  lastDay = myDay;                   // save the day...
  lastYear = myYear;                 // save the last year we checked
  lastMonth = myMonth;               // save it
  if (activeETA) {                   // do we have an print end ETA established
    if (etaHH != 0 || etaMM != 0) {  // even if we do, is it still zero, skip if it is
      time_t addSeconds = ((time_t)etaHH * 3600) + ((time_t)etaMM * 60);
      // --- Future time: current + offset ---
      time_t futureTime = localTime + addSeconds;  // gives the time when the print will be done
      futureHour12 = hourFormat12(futureTime);
      futureHour24 = hour(futureTime);
      futureMinute = minute(futureTime);
      if (show24HR == true) {
        theHour = futureHour24;  // we show this one
      } else {
        theHour = futureHour12;  // typical 12hr display
      }
      sprintf(buffer, "End Time: %2u:%02u ", theHour, futureMinute);  // time when print will be done
      tft.setTextColor(TFT_WHITE, TFT_BLACK);                         // white text
      tft.fillRect(1, 242, 239, 26, TFT_BLACK);                       // remove the old ETA first
      tft.drawString(buffer, 120, 272, 4);                            // ETA when print will be done
    }
    //uint8_t futureSecond = second(futureTime);
    //uint8_t futureDay = day(futureTime);
    //uint8_t futureMonth = month(futureTime);
    //uint16_t futureYear = year(futureTime);
    //bool futurePM = isPM(futureTime);
  }
  //drawGrid();// strictly for helping with line positions on the TFT
}
//this checks to see if the seconds needs a poll trigger
void handlePolling(int8_t theSeconds) {
  if (theSeconds % thePollTime) {
    //enablePoll = false;;
  } else {
    enablePoll = true;  // yep, it's time!
  }
}
//
void handleGaugeHeadings() {
  tft.drawString("Nozzle", 40, headingY, 2);     // labels
  tft.drawString("Progress", 120, headingY, 2);  // labels
  tft.drawString("Bed", 200, headingY, 2);       // labels
}
//
void handleGauge(uint8_t whichGauge, int16_t gaugeValue) {
  float temp;
  uint16_t theMove;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  switch (whichGauge) {
    case nozzleGauge:
      temp = (float(gaugeValue) / maxNozzleTemp);                                          // what percentage of the nozzle temp are we using?
      theMove = (temp * 280) + 40;                                                         // get the amount to draw
      tft.drawSmoothArc(40, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);      // Nozzle
      tft.drawSmoothArc(40, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);  // Active part
      tft.fillCircle(40, gaugeY, 20, TFT_BLACK);                                           // erase the 250 nozzle
      if (nozzleTarget != 0) {
        tft.setTextColor(TFT_RED, TFT_BLACK);  // we are HEATING value in RED
      }
      tft.drawString(String(gaugeValue), 40, dataY, 2);  // show the value (not heating)
      break;
    case progressGauge:
      if (printState == "printing") {                                                           // only calculate when printing
        temp = (float(gaugeValue) / 100);                                                       // progress amount done
        theMove = (temp * 280) + 40;                                                            // get the amount to draw
        tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);        // Progress gauge
        if (theMove > 40) {                                                                     // if we try to draw a zero, it goes around
          tft.drawSmoothArc(120, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);  // Active part
        }
        tft.fillCircle(120, gaugeY, 20, TFT_BLACK);         // percentage done, erase before showing anything new
        tft.drawString(String(gaugeValue), 120, dataY, 2);  // now show the value we have done
        showIdle = false;                                   // not showing it now
      } else {
        tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);  // turn off the progress
        tft.drawString("Idle", 120, dataY, 2);                                            // labels
        if (showIdle == false) {                                                          // if we aqren't showing the idle icon, show it
          tft.fillRect(0, 206, 239, 64, TFT_BLACK);                                       // get rid of the ETA and end time stuff
          drawBmp(SD, IDLE_IMAGE, 88, 210);                                               //display idle printer
          showIdle = true;                                                                // we are showing it
        }
      }
      break;
    case bedGauge:
      temp = (float(gaugeValue) / maxBedTemp);                                              // progress amount done
      theMove = (temp * 280) + 40;                                                          // get the amount to draw
      tft.drawSmoothArc(200, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);      // Bed
      tft.drawSmoothArc(200, gaugeY, 32, 22, 40, theMove, TFT_GREEN, TFT_DARKGREY, false);  // Active part
      tft.fillCircle(200, gaugeY, 20, TFT_BLACK);                                           // erase the old  bed number
      if (bedTarget != 0) {
        tft.setTextColor(TFT_RED, TFT_BLACK);  // we are HEATING value in RED
      }
      tft.drawString(String(gaugeValue), 200, dataY, 2);  // show new bed temp
      break;
  }
}
void drawGrid() {
  for (int myY = 0; myY < 320; myY += 16) {
    tft.drawFastHLine(0, myY, 239, TFT_GREEN);
  }
}
//
void setRGB(bool redLevel, bool greenLevel, bool blueLevel) {
  digitalWrite(RED_PIN, !redLevel);
  digitalWrite(GREEN_PIN, !greenLevel);
  digitalWrite(BLUE_PIN, !blueLevel);
}
//
void handleHostName() {
  if (WiFi.status() == WL_CONNECTED) {
    setRGB(0, greenON, 0);  // see if this does it
    HTTPClient http;
    http.setConnectTimeout(100);  // Sets a 1-second connection timeout
    http.begin(printerURLInfo);   // this gets the host name of the printer
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        //drawCenteredText("JSON Error", &FreeMonoBold12pt7b, 0);
        Serial.println("JSON Error");
      } else {  // no error process to get the name
        printerName = doc["result"]["hostname"] | "";
        Serial.println(printerName);
      }
    } else {
      //Serial.println("No Printer Found");
    }
    http.end();
    setRGB(0, greenOFF, 0);   // turn off the LED
    if (printerName != "") {  // if we found a printer
      lastBedTemp = 0;        // zero out all previous settings
      lastNozzleTemp = 0;     // force an update
      lastProgress = 0;
    }
  }
}
// this function includes the heat/levelling/printing/end homing time
void handleTimeUsed() {
  char buffer[30];                                  // holds the formated time
  double elapsed = difftime(printEnd, printStart);  // find the difference
  tft.fillRect(1, 230, 239, 32, TFT_BLACK);         // remove the END of print time first
  int32_t totalSeconds = (int)elapsed;
  int hrs = totalSeconds / 3600;
  int mins = (totalSeconds % 3600) / 60;
  // int seconds = totalSeconds % 60;// we don't care about the seconds....
  tft.setTextColor(TFT_GREEN, TFT_BLACK);           // green to show success
  if (hrs != 0) {                                   // print took more than 1 hour
    sprintf(buffer, "Total: %1u:%02u", hrs, mins);  // kill the leading 0 on the hours
  } else {
    sprintf(buffer, "Total: %1umins", mins);  // kill the leading 0 on the minutes
  }
  drawBmp(SD, IDLE_IMAGE, 88, 210);     //display idle printer
  showIdle = true;                      // we are showing it
  tft.drawString(buffer, 120, 304, 4);  // and draw the time the print actually took
}
//
void handleETA() {
  char timeLeft[24];                        // buffer to use
  if (printState == "printing") {           // if we are printing something, give me a time
    if ((progress * 100) > lastProgress) {  // the percentage done has increased
      lastProgress = progress * 100;        // save it for next time
      estimateTimeRemaining(printDuration, progress * 100, timeLeft);
      if (showHeat) {                              // showing the heat icon?
        tft.fillRect(1, 210, 239, 64, TFT_BLACK);  // erase the icon
        showHeat = false;                          // we only do it once
      }
      if (inPrepMode == false) {                                  // now update the time left
        tft.setTextColor(TFT_WHITE, TFT_BLACK);                   // white text
        tft.fillRect(1, 214, 239, 26, TFT_BLACK);                 // remove the old ETA first
        tft.drawString("ETA: " + String(timeLeft), 120, 240, 4);  // ETA when print will be done
      }
      //Serial.print("Remaining time to print: ");
      //Serial.println(timeLeft);  // in HH:MM
      activeETA = true;
    }
  } else {
    activeETA = false;
  }
}
//
void estimateTimeRemaining(float elapsedSeconds, float percentComplete, char *result) {
  if (percentComplete <= 0.0f || percentComplete > 100.0f) {
    //snprintf(result, 16, "--:--");
    snprintf(result, 16, "--:--");

    return;
  }
  float totalEstimated = elapsedSeconds / (percentComplete / 100.0f);
  float remainingSeconds = totalEstimated - elapsedSeconds;

  if (remainingSeconds < 0) remainingSeconds = 0;

  unsigned long remaining = (unsigned long)remainingSeconds;
  etaHH = remaining / 3600;
  etaMM = (remaining % 3600) / 60;
  //unsigned int ss = remaining % 60;

  snprintf(result, 16, "%02u:%02u", etaHH, etaMM);
}
//
void handlePrinterStatus() {
  if (WiFi.status() == WL_CONNECTED) {                             // are we connected to WIFI?
    setRGB(0, greenON, 0);                                         // see if this does it
    HTTPClient http;                                               // setup the http client
    http.setConnectTimeout(100);                                   // Sets a 1-second connection timeout
    http.begin(printerURLQ);                                       // string to ask for the printer data
    int httpCode = http.GET();                                     // and ask for it
    if (httpCode == 200) {                                         // 200 means the printer replied to us
      String payload = http.getString();                           // get the stuff it sent into a string
      JsonDocument doc;                                            // now shove it into a JSON doc
      DeserializationError error = deserializeJson(doc, payload);  // and setup the data for reading
      if (error) {                                                 // if there was a data error, well, JSON error!
        //drawCenteredText("JSON Error", &FreeMonoBold12pt7b, 0);
        Serial.println("JSON Error");
      } else {                                                                     // we got good data, JSON is happy, parse it
        printState = doc["result"]["status"]["print_stats"]["state"] | "unknown";  // either printing or standby

        // Temperatures we always get even when idle
        nozzleTemp = doc["result"]["status"]["extruder"]["temperature"] | 0.0;  // nozzle current temp
        if (round(nozzleTemp) != lastNozzleTemp) {                              // we need a change or skip it
          lastNozzleTemp = round(nozzleTemp);                                   // save the value for next pass
          handleGauge(nozzleGauge, lastNozzleTemp);                             // and update the gauge
        }
        bedTemp = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;  // current bed temp
        if (round(bedTemp) != lastBedTemp) {                                   // only update on a change
          lastBedTemp = round(bedTemp);                                        // save it for next time
          handleGauge(bedGauge, lastBedTemp);                                  // and update the gauge
        }
        printDuration = doc["result"]["status"]["print_stats"]["print_duration"] | 0.0;  // this is 0.00 until bed is hot, levelled and nozzle up to temp
        progress = doc["result"]["status"]["display_status"]["progress"] | 0.0;          // get the progress....0 to 10.00
        nozzleTarget = doc["result"]["status"]["extruder"]["target"] | 0.0;              // nozzle target temp
        bedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;               // target bed temp
        //
        if (printState == "standby") {               // if the printer is in standy, it's powered up but idle, not printing
          lastProgress = 0;                          // reset this for me when idle, used to check progress
          handleGauge(progressGauge, lastProgress);  // and update the gauge
          if (gotStartTime == true) {                // we just finished a print job
            printEnd = time(nullptr);                // grab the ending of the print job
            gotStartTime = false;                    // reset the flag now
            if (thePrintFile != "") {
              tft.fillRect(1, 208, 239, 92, TFT_BLACK);  // get rid of the filename we just printed
              handleTimeUsed();                          // go show the total time to took for the print job
            }
            thePrintFile = "";  // and we aren't printing anything
            inPrepMode = false;
          }
        } else {                                                                           // else we are actually heating/levelling/printing something
          if (thePrintFile == "") {                                                        // if the name is blank, go get it. once.
            String temp = doc["result"]["status"]["print_stats"]["filename"] | "Unknown";  // get the filepath
            thePrintFile = extractFileName(temp, false);                                   // go get the filename without extension
            tft.loadFont("NotoSansBold15");                                                // Load font
            tft.setTextColor(TFT_GREEN, TFT_BLACK);                                        // white text
            tft.setTextDatum(BC_DATUM);
            tft.fillRect(0, 272, 239, 31, TFT_BLACK);   // get rid of last total print time first
            tft.drawString(thePrintFile, 120, 296, 2);  // show the name of the file printing
            //Serial.println(thePrintFile);               //
            tft.unloadFont();  // Remove font to recover memory
          }
          if (gotStartTime == false) {                 // grab the start time
            gotStartTime = true;                       // set a flag so we only grab it once
            printStart = time(nullptr);                // or: time(NULL)
            tft.fillRect(1, 210, 239, 64, TFT_BLACK);  // remove the old total (actual print time and icon) of print time first
          }
          // progress 0.00 means we are heating up the bed
          tft.setTextColor(TFT_WHITE, TFT_BLACK);                                             // set the font colour for me
          if (printDuration == 0.00) {                                                        // we are heating or levelling
            tft.drawSmoothArc(120, gaugeY, 32, 22, 40, 320, TFT_DARKGREY, TFT_BLACK, false);  // erase the idle label
            tft.drawString("PREP", 120, dataY, 2);                                            // show we are in bed heat/level/nozzle heat mode
            inPrepMode = true;                                                                // working on heating and levelling
            if (nozzleTarget != 0 || bedTarget != 0) {
              showHeat = true;                      // flag so we know the icon is visible
              drawBmp(SD, HEATING_IMAGE, 88, 210);  //display that we're heating up the printer
            }
          } else {  // at this point, we are printing
            inPrepMode = false;
            progressPercent = int(progress * 100.0);     // make it a number from 0 to 100 (no decimals)
            if (lastProgress != progressPercent) {       // did the progress change
              lastProgress = progressPercent;            // save it for next time
              handleGauge(progressGauge, lastProgress);  // and update the gauge
            }
          }
        }
      }
    } else {
      //Serial.println("Printer Offine");          // the printer just went offline
      printerName = "";                          // reset the name til we power it back on
      tft.fillRect(0, 76, 239, 220, TFT_BLACK);  // get rid of anything there it won't be back here.
      showSleep = false;                         // back to waiting on the printer
    }
    http.end();              // close off the http request
    setRGB(0, greenOFF, 0);  // see if this does it
  } else {
    Serial.println("WiFi is lost");  // for whatever reason,we lost wifi
  }
}
//
//To Display <Setup> if not connected to AP
void configModeCallback(WiFiManager *myWiFiManager) {
  //Serial.println("Setup");
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(hostNameCYD), SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.loadFont("NotoSansBold15");  // Load font
  tft.drawString("Access Point Active", SCREEN_W / 2, (SCREEN_H / 2) + 36, 2);
  tft.unloadFont();  // Remove font to recover memory
  delay(2000);
}
//
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress timeServerIP;  // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  //  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0;  // return 0 if unable to get the time
}
//
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
//
String SendHTML() {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>KlippyMon</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;} h3 {color: #444444;} h4 {color: #444444;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #1abc9c;}\n";
  ptr += ".button_on {background-color: #1abc9c;}\n";
  ptr += ".button-off {background-color: #FF2D00;}\n";
  ptr += ".button_off {background-color: #FF2D00;}\n";
  ptr += ".infoButton {background-color: #e7e7e7; color: black;}\n";
  ptr += ".resetBTN {background-color: #f44336;}\n"; /* RED */
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Printer Configuration</h1>";
  ptr += "<h3>WabbitWanch Design</h3>";
  ptr += "<h4>Version: " + String(VERSION) + " - WiFi: " + String(getWifiQuality()) + "</h4>\n";
  //
  ptr += getPrinterSetup();  // add in all the printer settings
  //
  ptr += "<br><hr style=\"width:20%\">";
  ptr += "Wifi Control";
  ptr += "<a class=\"button resetBTN\" href=\"/wifiReset\">RESET</a>\n";
  //   ptr += "<br><br>" + DEBUG;
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
//
void handlePrinterUpdate() {
  String temp;
  if (server.hasArg("printerIP")) {  // update the new IP number
    printerIP = server.arg("printerIP");
  }
  if (server.hasArg("maxBedTemp")) {
    temp = server.arg("maxBedTemp");
  }
  maxBedTemp = temp.toInt();

  if (server.hasArg("maxNozzleTemp")) {
    temp = server.arg("maxNozzleTemp");
  }
  maxNozzleTemp = temp.toInt();

  if (server.hasArg("show24HR")) {  // this only shows if the box was checked
    show24HR = true;
    //}
  } else {  // checkbox is off
    show24HR = false;
  }
  writeSettings();                            // go update the settings
  buildPrinterURLs();                         // update the printer URLS for me now
  server.send(200, "text/html", SendHTML());  // and update the web display
}
//
String getPrinterSetup() {
  String printerForm = String(printer_Info);            //get the printer info first
  printerForm.replace("%IP%", printerIP);               // replace the IP
  printerForm.replace("%MBT%", String(maxBedTemp));     // maximum bed temp
  printerForm.replace("%MNT%", String(maxNozzleTemp));  // maximum nozzle temperature
  if (show24HR == true) {                               // if this on, we want to show checkbox is on
    printerForm.replace("%boxState%", "checked");       // want 24hr time?
  } else {                                              // else it's off so leave it
    printerForm.replace("%boxState%", "unchecked");     // want 24hr time?
  }
  return printerForm;  // and go back with the values
}
//
void writeSettings() {
  // Save decoded message to SPIFFS file for playback on power up.
  File f = LittleFS.open(CONFIG, "w");
  if (!f) {
    Serial.println("File open failed!");
  } else {
    Serial.println("Saving settings now...");
    f.println("printerIP=" + printerIP);
    f.println("maxBedTemp=" + String(maxBedTemp));
    f.println("maxNozzleTemp=" + String(maxNozzleTemp));
    f.println("show24HR=" + String(show24HR));
    f.close();
  }
  //readSettings();
}
//
void readSettings() {
  if (LittleFS.exists(CONFIG) == false) {
    Serial.println("Settings File does not yet exist");
    writeSettings();
    return;
  }
  File fr = LittleFS.open(CONFIG, "r");
  String line;
  while (fr.available()) {
    line = fr.readStringUntil('\n');

    if (line.indexOf("printerIP=") >= 0) {
      printerIP = line.substring(line.lastIndexOf("printerIP=") + 10);
      printerIP.trim();
      Serial.println("printerIP: " + printerIP);
    }
    if (line.indexOf("maxBedTemp=") >= 0) {
      maxBedTemp = line.substring(line.lastIndexOf("maxBedTemp=") + 11).toInt();
      Serial.println("maxBedTemp: " + String(maxBedTemp));
    }
    if (line.indexOf("maxNozzleTemp=") >= 0) {
      maxNozzleTemp = line.substring(line.lastIndexOf("maxNozzleTemp=") + 14).toInt();
      Serial.println("maxNozzleTemp: " + String(maxNozzleTemp));
    }
    if (line.indexOf("show24HR=") >= 0) {
      show24HR = line.substring(line.lastIndexOf("show24HR=") + 9).toInt();
      Serial.println("show24HR: " + String(show24HR));
    }
  }
  fr.close();
  // now we need to update our settings for the URL
  //printerClient.updatePrintClient(PrinterApiKey, PrinterServer, PrinterPort, PrinterAuthUser, PrinterAuthPass, HAS_PSU);
}
void buildPrinterURLs() {
  printerURLInfo = "http://" + printerIP + printerINFO;  // looks for the printer at the IP address given
  //Serial.println(printerURLInfo);
  printerURLQ = "http://" + printerIP + printQuery;  // and this is all the data when the printer is active
  //Serial.println(printerURLQ);
}

// Extract filename WITH or WITHOUT extension (ie "TOP_PLA" or "TOP_PLA.gcode")
String extractFileName(const String &path, bool withExt) {
  if (withExt == false) {                                 // just return the name without the extension (.gcode)
    int slashIdx = path.lastIndexOf('/');                 // find the last /
    int dotIdx = path.lastIndexOf('.');                   // and where the last . starts
    int start = (slashIdx >= 0) ? slashIdx + 1 : 0;       // and then the start of the name
    int end = (dotIdx > start) ? dotIdx : path.length();  // and the end position
    return path.substring(start, end);                    // now extract the name from that
  } else {
    int slashIdx = path.lastIndexOf('/');  // find the last / before the filename
    return path.substring(slashIdx + 1);   // grab the whole name with extension
  }
}

void handleWifiReset() {  // this will wipe the WIFI settings
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Wifi reset in 5 seconds", SCREEN_W / 2, SCREEN_H / 2, 2);
  delay(5000);
  redirectHome();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();  // kill the config for the WIFI
  ESP.restart();                // then kick back into the portal mode
}

void redirectHome() {
  // Send them back to the Root Directory
  server.sendHeader("Location", String("/"), true);
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");
  server.client().stop();
}
//