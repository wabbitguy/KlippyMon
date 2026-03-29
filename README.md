# KlippyMon
Klipper Monitor using a 2.8" CYD (cheap yellow display)

<p>Written for my QIDI X-PLUS 3 so I can glance at what it's doing when I'm not in the shop. Obviously I could pop out a phone and log into MoonRaker/MainSail but I found it easier to glance at the CYD.</p>

<p>The code is compiled with the Arduino IDE 2.3.8 (as of March 2026). The settings.h file has a list of all the libaries that are required to compile the code.</p>

<h3>CYD Board</h3>
<p></p>There are a couple of 2.8" CYD boards, the one I am using is based on the ST7789. An earlier one was based on a ILI9431. Thus if you haven't bought the board yet, ensure you get one with the ST7789 driver for the TFT.</p>

<p>The back of the CYD I used (and found on Amazon; it was advertised as a bitcoin miner and from FreeNove):</p>

![CYD_BACK](https://github.com/user-attachments/assets/29b2ec5e-6125-4fb0-a921-22b3310af7e6)

<h3>Running KlippyMon</h3>
<p></p>After uploading the code the display will present a message that access point "KlippyMon" is available. This is a captive portal that you need to connect to via WiFi. If you use an iPhone, go to Settings-->Wi-Fi and look for KlippyMon. Select it and in a few seconds you'll be presented with a list of wifi access points to chose from at your location. Select the network access point you want, enter in the password and press "Save". KlippyMon will store your settings and reboot.</p>

<p>After the reboot, KlippyMon will be scanning every 10 seconds for a printer that lives at it's default IP address of 192.168.1.6. Which is roughly a 254 to 1 chance of actually being where your printer is on your LAN. The default screen (assuming your printer lives elsewhere) will be:</p>

![KlippyMon_Sleeping](https://github.com/user-attachments/assets/adf20147-9816-4fce-aa31-d8cafd13f8bc)

<p>What is required next is for you to use any web browser and to navigate to the IP shown OR KlippyMon.local if that works for you. Here you can put in the IP address of your printer (you don't need to add any :10088 or anything. That's been taken care of in the code itself.</p>

![KlippyMon_Config](https://github.com/user-attachments/assets/766d3c52-9fa5-420b-9a76-eb4a2393a139)

<p>When KlippyMon starts either with it's default IP or once you enter your printers IP, you'll either be greeted with a "sleep" image or the current state of your printer (temps, etc)</p>

![KlippyMon_Sleeping](https://github.com/user-attachments/assets/97bc97c3-f0ab-410e-adb5-e11c5d80468a)

<p>OR</p>

![KlippyMon_Printer_Idle](https://github.com/user-attachments/assets/379de026-7f83-4aa5-84e0-d24250875c09)

<p>When you start a print job, KlippyMon's temperature numbers will turn red (tells you it's heating or maintaining the heat) and the PROGRESS will indicate PREP. PREP means it's not actually printing, it's preparing to start to print. A background timer will also be set in KlippyMon so at the end of the print you'll get the total time the print took. That includes bed warm up, any levelling that is done and the nozzle heating time. Once the printing has actually started you'll get an ETA that's updated every 10 seconds throughout the entire printing process.</p>

![KlippyMon_Printing](https://github.com/user-attachments/assets/a92186f5-833b-4040-84e4-9901ec7dcae2)

<p>At the end of the print, you'll get the total elapsed time (accurate to within 20 seconds).</p>

![KlippyMon_Print_Done](https://github.com/user-attachments/assets/064ae2a8-7e6c-4014-bfce-0b91eaf4450e)

<br><br>
<h3>Modify The Code</h3>
<p>The first step is to change the time configuration in the settings.h file to reflect your timezone. Look for the following line:</p>

<p><b>// ---------------- TIME HELPERS ----------------</b></p>

<p>Change the timezone as needed (it currently defaults to British Columbia/Canada PDT all the time). There are a number of examples shown in the code to help. Additonal details can be found all over the net for these libary settings, or directly from the source.:</p>

<p><a href="https://github.com/JChristensen/Timezone">TimeZone GitHub</a></p>

<p>If you already know the IP of your printer (it could be in your Slicer Printer Settings), plus the max bed and nozzle temp, you can edit these as well, but there's a web interface built in KlippyMon so it's not required.</p>

<h3>Compiling the Code</h3>

<p>Setup the Arduino IDE with these settings for the board. Your port will be different depending on if you're Mac or Windows based.</p>

<img width="400" height="339" alt="Arduino_IDE" src="https://github.com/user-attachments/assets/7d10bdc0-275f-4eb0-b717-c013bba3e59b" />

