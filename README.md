# KlippyMon v3.4
### A Klipper 3D Printer Monitor for the ESP32 CYD (Cheap Yellow Display)

**Wabbit Wanch Design © 2026**

<img width="480" height="710" alt="Success" src="https://github.com/user-attachments/assets/24e3e37d-d5f5-41eb-8d7b-8b91401bdf6d" />


KlippyMon turns a budget ESP32 CYD into a dedicated 3D printer monitor for Klipper-based printers. It connects to your printer's Moonraker API over WiFi and displays real-time print status, temperatures, progress, ETA, and thumbnail previews — all on a compact 2.8" colour TFT.

---

## Features

- Real-time nozzle, bed, and chamber temperatures
- Print progress gauge with ETA and Finish Print Time (FPT)
- Thumbnail preview fetched directly from Moonraker
- Print filename display
- Automatic chamber temperature detection (QIDI, Creality K1 series, and others)
- Push notifications via ntfy (print complete and stall/jam alerts)
- Seven language support (English, French, German, Spanish, Dutch, Portuguese, Turkish)
- Dark-themed web UI for configuration — no recompiling needed
- Auto-detection of printer temperature limits
- WiFi quality indicator
- NTP clock with 12/24 hour mode

---

## Hardware Required

- **ESP32 CYD** (Cheap Yellow Display) — ESP32-2432S028R or compatible
- USB-C cable for flashing
- Your Klipper printer running Moonraker

<img width="412" height="242" alt="CYD" src="https://github.com/user-attachments/assets/ad9de71c-f7fa-4994-bd0c-ac1980c0c63c" />

---

## Libraries Required

Install the following libraries via Arduino Library Manager:

- TFT_eSPI
- ArduinoJson
- WiFiManager
- TimeLib (Time by Michael Margolis)
- Timezone by Jack Christensen
- PNGdec
- LittleFS (included with ESP32 Arduino core)

---
## KlippyMon Running

<img width="240" height="315" alt="Progress" src="https://github.com/user-attachments/assets/e25fa739-13da-4165-ade8-73d7765d6bc9" />

---


## Quick Start

### 1. Flash the Firmware

1. Open the project in Arduino IDE 2.3.9
2. Select your board: **ESP32 Dev Module** NO OTA (2MB APP/2MB SPIFFS)
3. Set the language in `Language.h` (default is English — see [Language Settings](#language-settings))
4. Set your timezone in `Settings.h` (see [Timezone Settings](#timezone-settings))
5. Compile and upload to the CYD

### 2. Upload the Data Folder (Fonts and Images)

KlippyMon stores its fonts and images in the LittleFS filesystem on the CYD. This must be uploaded separately after flashing the firmware.

> **Important:** Close the Serial Monitor before uploading the data folder — the upload will fail if the serial port is in use.

1. Open the command palette in Arduino IDE 2.x:
   - **Mac**: Cmd+Shift+P
   - **Windows**: Ctrl+Shift+P
2. Type **Upload LittleFS** and select **Arduino: Upload LittleFS to Pico/ESP8266/ESP32**
3. Wait for the upload to complete — the CYD screen will go blank during this process

> **Note:** If the LittleFS upload option does not appear in the command palette, you need to install the LittleFS upload plugin. Download it from:
> [https://github.com/earlephilhower/arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload)
> and follow the installation instructions, then restart Arduino IDE.

### 3. Connect to WiFi

On first boot KlippyMon will create a WiFi hotspot called **KlippyMon**. Connect to it from your phone or computer and you'll be redirected to the WiFi setup page. Enter your network credentials and KlippyMon will reboot and connect.

<img width="230" height="346" alt="WIFI_Setup" src="https://github.com/user-attachments/assets/d38fe8db-eb6e-4586-83c2-09a030c8fbe2" />

### 4. Configure Your Printer

Once connected to your network, open a browser and go to:

```
http://klippymon.local
```

KlippyMon has a built-in web interface accessible from any browser on your local network at `http://klippymon.local` or the IP address shown on the display.

<img width="320" height="644" alt="Configure" src="https://github.com/user-attachments/assets/65cf71e1-84a1-4475-a073-114dab7fda2d" />

> **Tip:** If `klippymon.local` doesn't work on your network, use the IP address shown on the KlippyMon display when it is waiting for your printer to come online. It will be shown at the bottom of the screen, for example: `KlippyMon 192.168.1.45`

| Field | Description |
|-------|-------------|
| Printer IP | The IP address of your Klipper printer |
| Printer Port | Moonraker port — usually 7125, QIDI uses 10088 |
| 24 Hour Clock | Toggle between 12 and 24 hour time display |

If you want to use nfty for notifications, read the section in this document about the options for setup.

Click **Update & Save**. KlippyMon will find your printer and start displaying data within a few seconds.

KlippyMon will poll your printer every 10 seconds to update it's display. Thus it is possible for it to have a slight delay sometimes.

---

## Language Settings

Open `Language.h` and uncomment the language you want. Only one language can be active at a time.

```cpp
#define LANG_EN   // English  (default)
//#define LANG_FR   // French
//#define LANG_DE   // German
//#define LANG_ES   // Spanish
//#define LANG_NL   // Dutch
//#define LANG_PT   // Portuguese
//#define LANG_TR   // Turkish
```

Recompile and flash after changing the language.

---

## Timezone Settings

Open `Settings.h` and find the timezone section. Set your timezone rule to match your location. Examples are provided in the file for common timezones.

```cpp
// Example — US Pacific Time
TimeChangeRule myDST = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule mySTD = {"PST", First, Sun, Nov, 2, -480};
Timezone timeZoneRule(myDST, mySTD);
```

A full list of timezone rules can be found at:
[https://github.com/JChristensen/Timezone](https://github.com/JChristensen/Timezone)

Recompile and upload after changing the timezone.

---

## Chamber Temperature

KlippyMon automatically detects whether your printer has a chamber temperature sensor. If one is found, the chamber temperature is displayed on the left side of the screen during printing — no configuration required.

Supported chamber sensor names:
- `heater_generic hot` — QIDI Tech printers
- `heater_generic chamber` / `heater_generic Chamber`
- `temperature_sensor chamber` / `temperature_sensor Chamber`
- `temperature_sensor chamber_temp` — Creality K1 / K1C / K1 Max

If your printer's chamber sensor uses a different name in `printer.cfg`, it will not be detected automatically.

The chamber temperature displays in **white** when the chamber is at ambient temperature and **red** when the chamber heater is actively heating.

---

## Printer Compatibility

KlippyMon works with any Klipper printer running Moonraker. Thus should work with:

- **QIDI Tech** (Q1 Pro, X-Max 3, X-Plus 3) — port 10088
- **Creality K1 / K1C / K1 Max** — port 7125
- **Elegoo Neptune 4 series** — port 7125
- **Any Voron or other Klipper build** — port 7125

---

## Troubleshooting

**KlippyMon shows the offline screen and won't find my printer**
- Check the printer IP and port in the web UI at the IP shown on the display
- Make sure Moonraker is running on the printer
- Try accessing `http://PRINTER_IP:PORT` from your browser — you should see the Mainsail or Fluidd interface

**Chamber temperature shows 0°C**
- Your printer may not have a chamber sensor, or the sensor name in `printer.cfg` is not in the supported list. See [Chamber Temperature](#chamber-temperature) above.

**The web UI is not loading with a browser at `http://klippymon.local`**
- Use the IP address shown on the KlippyMon display instead
- mDNS (`.local` addresses) may not work on all networks, particularly on some Android devices

---

## Push Notifications (ntfy)

> **Advanced feature** — ntfy setup is optional. KlippyMon works perfectly without it.

KlippyMon can send push notifications to your phone and smartwatch when:

1. **A print completes** — you'll get a notification with the filename and total print time
2. **The print stalls** — no progress for the configured timeout (default 2 minutes) — useful for detecting filament jams, runouts, or filament changes

Notifications are sent via **ntfy** — a free, open source push notification service. You have two options:

---

### Option A — Public ntfy.sh Server (Easiest)

This is the simplest option and requires no server setup.

1. Install the **ntfy** app on your phone:
   - **iPhone**: [App Store](https://apps.apple.com/us/app/ntfy/id1625396347)
   - **Android**: [Google Play](https://play.google.com/store/apps/details?id=io.heckel.ntfy) or [F-Droid](https://f-droid.org/en/packages/io.heckel.ntfy/)

2. Open the ntfy app and subscribe to a topic. Choose something unique that others won't guess, for example: `klippymon-abc123`

3. In the KlippyMon web UI:
   - Enable ntfy
   - Set **Server URL** to `ntfy.sh`
   - Set **Topic** to your chosen topic name
   - Leave **Token** blank
   - Click **Update & Save**


> **Privacy note:** The public ntfy.sh server is free but your topic name and messages are visible to anyone who knows your topic name. Choose a topic name that is hard to guess.

---

### Option B — Self-Hosted ntfy (Raspberry Pi)

Self-hosting ntfy on a Raspberry Pi on your local network keeps your notifications private and doesn't rely on any external service. This is a great option if you already have a Pi running on your network (such as a PiHole).

> **Important for iPhone users:** Even with a self-hosted server, the ntfy iOS app requires your Pi to be able to reach `ntfy.sh` on the internet to deliver instant push notifications to your lock screen. Your actual notification content stays on your Pi — ntfy.sh only acts as a doorbell ping. If your Pi has no internet access, notifications will be delayed.

#### Install ntfy on Raspberry Pi

SSH into your Pi and run the following commands. Make sure to check the [ntfy releases page](https://github.com/binwiederhier/ntfy/releases) for the latest version number.

**For Raspberry Pi 4 and 5 (64-bit / aarch64):**
```bash
wget https://github.com/binwiederhier/ntfy/releases/download/v2.23.0/ntfy_2.23.0_linux_arm64.deb
sudo dpkg -i ntfy_2.23.0_linux_arm64.deb
sudo systemctl enable ntfy
sudo systemctl start ntfy
```

**For older Raspberry Pi (32-bit / armv7):**
```bash
wget https://github.com/binwiederhier/ntfy/releases/download/v2.23.0/ntfy_2.23.0_linux_armv7.deb
sudo dpkg -i ntfy_2.23.0_linux_armv7.deb
sudo systemctl enable ntfy
sudo systemctl start ntfy
```

#### Configure ntfy

Edit the ntfy configuration file:

```bash
sudo nano /etc/ntfy/server.yml
```

Add these three lines at the bottom of the file:

```yaml
listen-http: ":2586"
base-url: "http://YOUR_PI_IP:2586"
upstream-base-url: "https://ntfy.sh"
```

Replace `YOUR_PI_IP` with your Pi's actual IP address (e.g. `192.168.1.82`).

> **Port conflict note:** If your Pi is running PiHole, ntfy cannot use the default port 80 as PiHole's web interface is already using it. Port 2586 is used in this example but you can use any unused port above 1024.

> **Important:** All three lines are required. If you set `upstream-base-url` without also setting `base-url`, ntfy will fail to start.

Save the file and restart ntfy:

```bash
sudo systemctl restart ntfy
sudo systemctl status ntfy
```

You should see **active (running)** in green.

#### Test ntfy

From any computer on your network in a terminal:

```bash
curl -d "ntfy is alive!" http://YOUR_PI_IP:2586/klippymon
```

You should get a JSON response back confirming the message was received.

#### Set Up the ntfy phone App

**iPhone:**
1. Install **ntfy** from the [App Store](https://apps.apple.com/us/app/ntfy/id1625396347)
2. Open the app and tap **+** in the top right
3. Change the server URL from `https://ntfy.sh` to `http://YOUR_PI_IP:2586`
4. Enter your topic name (e.g. `klippymon`)
5. Tap **Subscribe**

**Android:**
1. Install **ntfy** from [Google Play](https://play.google.com/store/apps/details?id=io.heckel.ntfy) or [F-Droid](https://f-droid.org/en/packages/io.heckel.ntfy/)
2. Open the app and tap **+**
3. Change the server URL to `http://YOUR_PI_IP:2586`
4. Enter your topic name
5. Tap **Subscribe**

#### Configure KlippyMon for Self-Hosted ntfy

In the KlippyMon web UI:
- Enable ntfy
- Set **Server URL** to your Pi's IP address (e.g. `192.168.1.82`)
- Set **Port (Local Host)** to `2586` (or whatever port you chose)
- Set **Topic** to your topic name
- Click **Update & Save**

#### ntfy Troubleshooting

**Notifications are delayed or not arriving**
- Make sure the ntfy app is installed — the web app in Safari does not deliver lock screen notifications
- Verify `upstream-base-url: "https://ntfy.sh"` is in your `server.yml` — this is required for instant iOS push notifications
- Verify `base-url` is also set — ntfy will not start without it if `upstream-base-url` is configured
- Test with curl from another machine on your network to confirm ntfy is reachable

**Notifications work but are slow**
- If using the ntfy web app (PWA) instead of the native app, notifications only arrive when the page is open. Install the native app from the App Store or Google Play.

---
## License

MIT License — free to use, modify and distribute. Attribution appreciated.

---

## Credits

KlippyMon is developed by **Wabbit Wanch Design**.  
ntfy app is developed by [Philipp Heckel](https://github.com/binwiederhier/ntfy).

---

*KlippyMon v3.0 © 2026 Wabbit Wanch Design*
