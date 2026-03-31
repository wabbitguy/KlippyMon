/** The MIT License (MIT)

Copyright (c) 2026 Mel Patrick

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>  //https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <Timezone.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include "LittleFS.h"
#include <WebServer.h>

//  http://192.168.1.115:7125/printer/objects/query?print_stats&server&virtual_sdcard&toolhead&display_status&heater_bed
//these are the settings for your printer
// maximums for bed temp and nozzle temp
uint8_t maxBedTemp = 120;
uint16_t maxNozzleTemp = 350;

//this the IP information of your printer on the LAN
String printerIP = "192.168.1.6";  // just a default IP address, put yours in if you know it

String printQuery = ":7125/printer/objects/query?print_stats&display_status&extruder&heater_bed";
String printerINFO = ":7125/printer/info";
String printerURLQ = "";
String printerURLInfo = "";
String thePrintFile = "";  // filename of the currently printing file

// This is the graphic that shows when the printer is powered off
#define OFFLINE_IMAGE "/Asleep.bmp"   // graphics MUST be in 24bit format 186 x 186 (H x W)
#define IDLE_IMAGE "/Idle.bmp"        // graphic when active but not printing (64 x 64)
#define HEATING_IMAGE "/Heating.bmp"  // graphic to indicate heating
//
// ---------------- SD Card FILES ----------------
#define SD_MOSI 23  // pin numbers for the CDY 2.8 7789
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

//Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = { "AEDT", First, Sun, Oct, 2, 660 };  //UTC + 11 hours
TimeChangeRule aEST = { "AEST", First, Sun, Apr, 3, 600 };  //UTC + 10 hours
//Timezone timeZoneRule(aEDT, aEST);

//Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  //Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    //Central European Standard Time
//Timezone timeZoneRule(CEST, CET);

//United Kingdom (London, Belfast)
TimeChangeRule BST = { "BST", Last, Sun, Mar, 1, 60 };  //British Summer Time
TimeChangeRule GMT = { "GMT", Last, Sun, Oct, 2, 0 };   //Standard Time
//Timezone timeZoneRule(BST, GMT);

//US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, -240 };  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -300 };   //Eastern Standard Time = UTC - 5 hours
//Timezone timeZoneRule(usEDT, usEST);

//US Central Time Zone (Chicago, Houston)
TimeChangeRule usCDT = { "CDT", Second, dowSunday, Mar, 2, -300 };
TimeChangeRule usCST = { "CST", First, dowSunday, Nov, 2, -360 };
//Timezone timeZoneRule(usCDT, usCST);

//US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = { "MDT", Second, dowSunday, Mar, 2, -360 };
TimeChangeRule usMST = { "MST", First, dowSunday, Nov, 2, -420 };
//Timezone timeZoneRule(usMDT, usMST);

//Arizona is US Mountain Time Zone but does not use DST
//Timezone timeZoneRule(usMST, usMST);

//US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule usPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };
TimeChangeRule usPST = { "PST", First, dowSunday, Nov, 2, -480 };
//Timezone timeZoneRule(usPDT, usPST);
//
// ---------------- TIME HELPER ----------------
//TimeZone Settings Details https://github.com/JChristensen/Timezone
//BC Canada Pacific Standard Time always
TimeChangeRule bcPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };  // 7 hour offset - BC, Canada

//Edit the info in the (xxx,xxx) according To Your Timezone and Daylight Saving Time
// for example if in the US pacific northwest you'd use
// Timezone timeZoneRule(usPDT, usPST);
// Examples are provided above for various timezone settings
//
Timezone timeZoneRule(bcPDT, bcPDT);

//Pointer To The Time Change Rule, Use to Get The TZ Abbrev
TimeChangeRule *tcr;
time_t utc;
bool show24HR = false;

const char printer_Info[] PROGMEM = R"rawliteral(
<style>
  label {
    display: inline-block;
    width: 150px;
        font-size: 18px;
    text-align: right;
  }
  input[type="text"] {
    font-size: 16px;
     width: 120px;
  }
  input[type="submit"] {
    font-size: 16px;
    padding: 10px 30px;
    display: block;
    margin: 10px auto;
  }
  .checkbox-row {
    margin: 10px 0;
  }
  .checkbox-row label {
    display: inline-block;
    width: 120px;
    text-align: right;
    margin-right: 8px;
    font-size: 16px;
  }
  .checkbox-row input[type="checkbox"] {
    transform: scale(1.5);
</style>
<form action="/updatePrinterInfo">
  <label for="printerIP">Printer IP:</label>
  <input type="text" id="printerIP" name="printerIP" value="%IP%"><br>
  <label for="maxBedTemp">Max Bed Temp:</label>
  <input type="text" id="maxBedTemp" name="maxBedTemp" value="%MBT%"><br>
  <label for="maxNozzleTemp">Max Nozzle Temp:</label>
  <input type="text" id="maxNozzleTemp" name="maxNozzleTemp" value="%MNT%"><br>
    <div class="checkbox-row">
    <label for="Show 24Hr">Show 24Hr:</label>
    <input type="checkbox" id="show24HR" name="show24HR" value="true" %boxState%>
  </div>
  <input type="submit" value="Update">
</form> 
)rawliteral";