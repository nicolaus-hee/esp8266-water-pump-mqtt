# esp8266-water-pump-mqtt
Avoid herb plants from running dry. Water them automatically with a small pump whenever needed. Monitor moisture readings and receive notifications when water reservoir is empty, through the [openHAB](https://openhab.org) smart home system and the [MQTT](https://mqtt.org) messaging protocol.

![My herb bed](/images/herb_bed.jpg)

# What you need
* ESP8266 (I used a [NodeMCU v2](https://www.amazon.de/gp/product/B0754HWZSQ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Moisture sensor (I used [this one](https://www.amazon.de/gp/product/B07CNRJN8W/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Water pump (I used [this one](https://www.amazon.de/Homengineer-Tauchpumpe-Brunnen-Bew%C3%A4sserung-Raspberry/dp/B07PGQNKKC/))
* Relay (I used [this one](https://www.amazon.de/gp/product/B07CNR7K9B/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Wiring, resistors, LEDs, a power source (I use [a 5.5mm terminal](https://www.amazon.de/gp/product/B009PH1J5Y/ref=ppx_yo_dt_b_asin_title_o01_s00?ie=UTF8&psc=1) wired to a power bank) etc.

# Breadboard prototype
Without NodeMCU
![Sketch without NodeMCU](/images/sketch_without_nodemcu.PNG)
With NodeMCU
![Sketch with NodeMCU](/images/sketch_with_nodemcu.PNG)

* LED (solid): Pump ran more than x seconds without reaching target moisture level; empty water container?
* LED (blink): Board is in OTA update mode
* Button top: Reset
* Button bottom: OTA; hold during boot triggers 3 min delay for OTA flash, prevents deep sleep

# What the code does
* Connect to WiFi & MQTT
* Every x minutes, read moisture level (x times, build average)
* If moisture < threshold, start water pump
* Stop pump when moisture threshold reached
* Publish readings & pump status changes to MQTT topics
* When not active, go into deep sleep
* Wake up after x minutes and repeat

Note: Settings (e.g. WiFi credentials, moisture threshold etc.) are all defined in the variables declaration section at the top of the code. The script includes  `ArduinoOTA` so you'll be able to upload a modified script without physically attaching the board to your computer. To activate it, hold the 'OTA button' while resetting.

# MQTT & openHAB integration
openHAB integration is optional. If you don't do it, the program will run without any issues. Though openHAB will allow you to monitor its activity. Make sure your WiFi & MQTT credentials are defined in the script.

## MQTT
The code will publish a status update after each sensor reading, like so:
```
Topic: stat/water_pump/STATUS
Payload: {"MOISTURE":"57","PUMP_VCC":"OFF","WATER_LEVEL":"OK"}
```
* MOISTURE: Last reading (0 = dry, 100 = wet)
* PUMP_VCC: Pump ON or OFF?
* WATER_LEVEL: OK or WARNING

## Channels & items
```Number Garden_Water_Pump_Moisture "Feuchtigkeit [%d %%]" { channel="mqtt:topic:garden-water-pump:garden-water-pump-moisture" }
Switch Garden_Water_Pump_Power "Pumpe ein/aus" { channel="mqtt:topic:garden-water-pump:garden-water-pump-pump-power" }
String Garden_Water_Pump_Water_Level "Wasservorrat" { channel="mqtt:topic:garden-water-pump:garden-water-pump-water-level" }
DateTime Garden_Water_Pump_Last_Update "Letztes Update [%1$td.%1$tm.%1$tY / %1$tH:%1$tM]" { channel="mqtt:topic:garden-water-pump:garden-water-pump-last-update" }
DateTime Garden_Water_Pump_Last_Flush "Letzte Bewässerung [%1$td.%1$tm.%1$tY / %1$tH:%1$tM]" { channel="mqtt:topic:garden-water-pump:garden-water-pump-last-flush" }
```

## Sitemap
```Default item=Garden_Water_Pump_Moisture
Text item=Garden_Water_Pump_Power
Default item=Garden_Water_Pump_Water_Level
Default item=Garden_Water_Pump_Last_Update
Default item=Garden_Water_Pump_Last_Flush
Chart item=Garden_Water_Pump_Moisture label="Verlauf Feuchtigkeit 24h" period=D refresh=1000
Chart item=Garden_Water_Pump_Moisture label="Verlauf Feuchtigkeit 1h" period=h refresh=1000
````

## Rules
To get a push notification in the openHAB app when the water reservoir is assumed empty:
```rule "garden water pump reservoir empty"
when
    Item Garden_Water_Pump_Water_Level changed to "WARNING"
then
    sendBroadcastNotification("Kräutergarten: Wasservorrat leer")
end
```
To update timestamps of last contact and last watering:
```rule "garden water pump timestamp for last flush"
when
    Item Garden_Water_Pump_Power received command ON or
    Item Garden_Water_Pump_Power changed to ON
then
    Garden_Water_Pump_Last_Flush.postUpdate(new DateTimeType());
end

rule "garden water pump timestamp for last update"
when
    Item Garden_Water_Pump_Power received update or
    Item Garden_Water_Pump_Moisture received update or
    Item Garden_Water_Pump_Water_Level received update
then
    Garden_Water_Pump_Last_Update.postUpdate(new DateTimeType());
end
```

## Screenshot
The result as displayed in openHAB's 'Paper UI':
![Sketch without NodeMCU](/images/openhab_screenshot.JPG)

# To do
- [ ] Low voltage warning
- [ ] Remote interaction (e.g. flush, change settings)
