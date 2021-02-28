#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

// Pins
const int sensor_pin = 0; //A0
const int sensor_vcc = 12; //D6
const int pump_vcc = 14; //D5
const int no_water_pin = 5; //D1
const int ota_pin = D7; //D3

// setttings / variables
const int sensor_readings = 30;
const int moisture_threshold = 40; //100 = wet, 0 = dry
const int deep_sleep_duration = 1; //minutes
int pump_count = 0; //how many flushes of ~3 seconds have been made during loop
const int pump_count_max = 10; //if >50 seconds / 5x pumped, water level probably empty

int sensor_value;
int sensor_value_last;

// WIFI
#define WLAN_SSID       "XXXXXXX"
#define WLAN_PASS       "XXXXXXX"
WiFiClient client;

//MQTT SERVER
// Thank you https://github.com/adafruit/Adafruit_MQTT_Library/blob/master/examples/adafruitio_anon_time_esp8266/adafruitio_anon_time_esp8266.ino
#define AIO_SERVER      "XXXXXXX"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "XXXXXXX"
#define AIO_KEY         "XXXXXXX"
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish water_pump_status = Adafruit_MQTT_Publish(&mqtt, "stat/water_pump/STATUS"); 
Adafruit_MQTT_Subscribe water_pump_flush = Adafruit_MQTT_Subscribe(&mqtt, "cmnd/water_pump/FLUSH"); // force 3 seconds flush
Adafruit_MQTT_Subscribe water_pump_flush2 = Adafruit_MQTT_Subscribe(&mqtt, "cmnd/water_pump/FLUSH2"); // flush until threshold

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT connected!");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Hello");  
  
  // Connect to WiFi access point.
  // Thank you https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/client-examples.html
  Serial.print("Connecting to ");
  Serial.print(WLAN_SSID);
  WiFi.hostname("water-pump");
  wifi_station_set_hostname("water-pump");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect (true);
  
  // Configure OTA module
  // thank you https://github.com/esp8266/Arduino/blob/master/libraries/ArduinoOTA/examples/BasicOTA/BasicOTA.ino
  ArduinoOTA.setHostname("water-pump");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  // Setup MQTT subscriptions
  mqtt.subscribe(&water_pump_flush); // force 3 second flush
  mqtt.subscribe(&water_pump_flush2); // flush until threshold

  // Set pin modes for pump and sensors
  pinMode(sensor_vcc, OUTPUT);
  pinMode(pump_vcc, OUTPUT);
  pinMode(no_water_pin, OUTPUT);
  pinMode(ota_pin, INPUT);
  digitalWrite(sensor_vcc, LOW);
  digitalWrite(no_water_pin, LOW);
}

void loop() { 
  Serial.println("Loop begins");
  
  // if OTA requested by button press, give us some time
  ArduinoOTA.handle();
  if (digitalRead(ota_pin) == LOW)
  {
    Serial.println("OTA: 3 minutes starting now...");
    for (int i = 0; i <= 180; i++)
    {
      digitalWrite(no_water_pin, HIGH);
      delay(200);
      digitalWrite(no_water_pin, LOW);
      ArduinoOTA.handle();    
      delay(1000);
    }
    Serial.println("It's over... ");
  }  

  // connect to MQTT
  MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  
  while ((subscription = mqtt.readSubscription(5000))) {
      if (subscription == &water_pump_flush) {
        pump_set_power(true);
        delay(3000);
        pump_set_power(false);
      }
      if (subscription == &water_pump_flush2) {
        flush_until_threshold();
      }      
  }

  // get & output moisture level
  sensor_value_last = get_moisture();
  Serial.println("Moisture: " + String(sensor_value_last) + "% (threshold: <" + String(moisture_threshold) + "%)");

  //Uncomment this if you want automatic pumping
  //if (sensor_value_last < moisture_threshold) {
    //flush_until_threshold();
  //}

  //Send readings to mqtt server
  publish_status();
  delay(10000);

  //If working on a battery you may want to enable deep sleep here
  //Serial.println("That's it for now. See you in " + String(deep_sleep_duration) + " minute(s).");
  //system_deep_sleep(deep_sleep_duration * 60000000); // n min
}

void publish_status() {
  // publish status to MQTT server
  water_pump_status.publish(get_status().c_str());
}

String get_status() {
  // puts together status in JSON format
  String status_string;
  status_string = "{";
  status_string += "\"MOISTURE\":\"" + String(sensor_value_last) + "\"";
  status_string += ",";
  if(digitalRead(pump_vcc) == HIGH) {
    status_string += "\"PUMP_VCC\":\"ON\"";
  } else {
    status_string += "\"PUMP_VCC\":\"OFF\"";
  }
  status_string += ",";
  if (pump_count >= pump_count_max) {
    status_string += "\"WATER_LEVEL\":\"WARNING\"";
  } else {
    status_string += "\"WATER_LEVEL\":\"OK\"";
  }
  status_string += "}";

  return status_string;
}

void pump_set_power(boolean power) {
  if(power) {
    if(pump_count < pump_count_max) {
      digitalWrite(pump_vcc, HIGH);
      pump_count += 1;
    } else {
      pump_set_power(false);
    }
  } else {
    digitalWrite(pump_vcc, LOW);
    if(pump_count < pump_count_max) {
      pump_count = 0;
    } else {
      digitalWrite(no_water_pin, HIGH);
    }
  }
  publish_status();
}

void flush_until_threshold() {
  // turn pump on in small intervals until threshold reached
  while(sensor_value_last < moisture_threshold and pump_count < pump_count_max) {  
    Serial.println("Moisture: " + String(sensor_value_last) + "%, WATERING");

    // pump for a few seconds
    pump_set_power(true);
    pump_count += 1;
    delay(3000);
    pump_set_power(false);

    // wait for water to sink in, then measure again
    delay(3000);
    sensor_value_last = get_moisture();
  }
  Serial.println("Moisture: " + String(sensor_value_last) + "%, DONE");
}

int get_moisture() {
  sensor_value = 0;
  
  // switch on moisture sensor
  digitalWrite(sensor_vcc, HIGH);
  delay(100);

  // get x readings
  for(int i = 0; i<sensor_readings; i++) {
    analogRead(sensor_pin);
    sensor_value += analogRead(sensor_pin);
    delay(50);
  }

  // switch off moisture sensor
  digitalWrite(sensor_vcc, LOW);

  // build average of readings and return it
  sensor_value = round(sensor_value / sensor_readings); 
  sensor_value = round(100 - (sensor_value * 100) / 1023);
  return sensor_value;
}
