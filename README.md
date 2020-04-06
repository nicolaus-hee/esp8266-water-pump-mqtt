# esp8266-water-pump-mqtt
Avoid herb plants from running dry. Water them automatically with a small pump whenever needed. Monitor moisture readings and receive notifications when water reservoir is empty, both through [openHAB](https://openhab.org).

[ picture ]

# What you need
* ESP8266 (I used a [NodeMCU v2](https://www.amazon.de/gp/product/B0754HWZSQ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Moisture sensor (I used [this one](https://www.amazon.de/gp/product/B07CNRJN8W/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Water pump (I used [this one](https://www.amazon.de/Homengineer-Tauchpumpe-Brunnen-Bew%C3%A4sserung-Raspberry/dp/B07PGQNKKC/))
* Relay (I used [this one](https://www.amazon.de/gp/product/B07CNR7K9B/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1))
* Wiring, resistors, LEDs, a power source (I use [a 5.5mm terminal](https://www.amazon.de/gp/product/B009PH1J5Y/ref=ppx_yo_dt_b_asin_title_o01_s00?ie=UTF8&psc=1) wired to a power bank) etc.

# Breadboard prototype
[ picture ]

* LED (solid): Pump ran more than x seconds without reaching target moisture level; empty water container?
* LED (blink): Board is in OTA update mode
* Button left: Reset
* Button right: OTA mode; hold down during boot will turn delay 'loop' by 3 minutes so you have time for starting the sketch upload

# What the code does
* Connect to WiFi & MQTT
* Read moisture level (n times, build average)
* If moisture < threshold, start water pump
* Stop pump when moisture threshold reached
* Publish readings & pump status changes to MQTT topics
* Go into deep sleep, wake up after n minutes and repeat

# openHAB integration
## Channels & items
[ OH items definition ]

## Sitemap
[ OH sitemap definition ]

## Rules
[ OH rules ]

# To do
- [ ] Low voltage warning
- [ ] Remote interaction (e.g. flush, change settings)
