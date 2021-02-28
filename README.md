# esp8266-water-pump-mqtt
Avoid herb plants from running dry. Water them automatically with a pump whenever needed. Monitor moisture readings and receive notifications when level is too low through the [openHAB](https://openhab.org) smart home system and the [MQTT](https://mqtt.org) messaging protocol. This was my final project for [CS50x](https://cs50.harvard.edu/x/) 2020.

<img src="/images/herb_bed.jpg" alt="First prototype of watering system" width="600">

# Parts
* ESP8266 (I used a [NodeMCU v2](https://www.amazon.de/gp/product/B0754HWZSQ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Moisture sensor (I used [this one](https://www.amazon.de/gp/product/B07CNRJN8W/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Water pump (I used [this one](https://www.amazon.de/dp/B001CV02U4/ref=pe_3044161_185740101_TE_item), note: I found [this](https://www.amazon.de/Homengineer-Tauchpumpe-Brunnen-Bew%C3%A4sserung-Raspberry/dp/B07PGQNKKC/) typical Arduino pump is too weak)
* Relay (I used [this one](https://www.amazon.de/gp/product/B07CNR7K9B/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Voltage regulator if your pump needs a Vcc incompatible with your board (I used [this one](https://www.amazon.de/DECARETA-Converter-Einstellbar-Wandler-MP1584EN/dp/B07QMCW2LY/ref=sr_1_3?dchild=1&qid=1614499324&sr=8-3))
* Wiring, resistors, LEDs, a power source (I use [a 5.5mm terminal](https://www.amazon.de/gp/product/B009PH1J5Y/ref=ppx_yo_dt_b_asin_title_o01_s00?ie=UTF8&psc=1)) etc.

# Breadboard prototype
<img src="/images/board_web.jpg" width="600" alt="Sketch including NodeMCU">
<img src="/images/sketch_with_nodemcu.png" width="600" alt="Sketch including NodeMCU">

[Sketch without NodeMCU](/images/sketch_without_nodemcu.png)

# What the code does
* Connect to WiFi & MQTT
* Read moisture level (x times, build average)
* ~~If moisture < threshold, start water pump~~
* ~~Stop pump when moisture threshold reached~~
* Publish readings & pump status changes to MQTT topics
* Check for MQTT command to start pump
* If pump command received, enable pump for 5 seconds*

**Note:** I disabled automatic pumping seeing that the moisture sensor doesn't always return correct readings. Uncomment respective lines if you're feeling more adventurous.

# MQTT & openHAB integration
openHAB integration is optional. You will need an MQTT client to read & control the pump, though. Make sure your WiFi & MQTT credentials are defined in the script.

## MQTT
The program will send and receive these MQTT commands:

Topic | Payload | Comment
----- | ------- | --------
stat/water_pump/STATUS | { "MOISTURE":"50", "PUMP_VCC":"OFF", "WATER_LEVEL":"OK" } | Published 1x per loop
cmnd/water_pump/FLUSH | any | will turn on pump for 3 seconds
cmnd/water_pump/FLUSH2 | any | will pump until target moisture reached

```
Topic: stat/water_pump/STATUS
Payload: {"MOISTURE":"57","PUMP_VCC":"OFF","WATER_LEVEL":"OK"}
```
* MOISTURE: Last reading (0 = dry, 100 = wet)
* PUMP_VCC: Pump ON or OFF?
* WATER_LEVEL: OK or WARNING

## openHAB things
```
Bridge mqtt:broker:broker "mqtt broker" [ host="openhabianpi", port=1883, secure=false, username="XXXXXX", password="XXXXXX" ]
{
    Thing topic water_pump "Water Pump" {
    Channels:
        Type number : water_pump_moisture_level "Moisture" [ stateTopic="stat/water_pump/STATUS", transformationPattern="JSONPATH:$.MOISTURE" ]
        Type switch : water_pump_power "Pump power" [ stateTopic="stat/water_pump/STATUS", transformationPattern="JSONPATH:$.PUMP_VCC" ]
        Type string : water_pump_water_level "Water level" [ stateTopic="stat/water_pump/STATUS", transformationPattern="JSONPATH:$.WATER_LEVEL" ]
        Type datetime: water_pump_last_flush "Last flush"
        Type datetime: water_pump_last_update "Last update"
    }
}
```

## openHAB items
```
Number water_pump_moisture_level "Moisture level [%d %%]" {channel="mqtt:topic:broker:water_pump:water_pump_moisture_level"}
Switch water_pump_power "Pump power" {channel="mqtt:topic:broker:water_pump:water_pump_power"}
String water_pump_water_level "Pump water level warning" {channel="mqtt:topic:broker:water_pump:water_pump_water_level"}
DateTime water_pump_last_update "Last update [%1$td.%1$tm.%1$tY / %1$tH:%1$tM]" { channel="mqtt:topic:broker:water_pump:water_pump_last_update" }
DateTime water_pump_last_flush "Last flush [%1$td.%1$tm.%1$tY / %1$tH:%1$tM]" { channel="mqtt:topic:broker:water_pump:water_pump_last_flush" }
```

## Sitemap
```
Default item=water_pump_moisture_level
Text item=water_pump_power
Default item=water_pump_water_level
Default item=water_pump_last_update
Default item=water_pump_last_flush
Chart item=water_pump_moisture_level label="Moisture last 24h" period=D refresh=1000
Chart item=water_pump_moisture_level label="Moisture last 1h" period=h refresh=1000
```

## Rules
To get a push notification in the openHAB app when the water reservoir is assumed empty:
```
rule "water pump reservoir empty"
when
    Item water_pump_water_level changed to "WARNING"
then
    sendBroadcastNotification("Water pump: Out of water")
end
```
To update timestamps of last contact and last watering:
```rule "water pump update timestamp for last flush"
when
    Item water_pump_power received command ON or
    Item water_pump_power changed to ON
then
    water_pump_last_flush.postUpdate(new DateTimeType());
end

rule "water pump update timestamp for last update"
when
    Item water_pump_power received update or
    Item water_pump_moisture_level received update or
    Item water_pump_water_level received update
then
    water_pump_last_update.postUpdate(new DateTimeType());
end
```

## Screenshot
The result as displayed in openHAB's 'Paper UI':
![openhab screenshot](/images/openhab_screenshot.JPG)
