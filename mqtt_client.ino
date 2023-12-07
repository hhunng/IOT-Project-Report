#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_AHTX0.h>
#include <WiFiClient.h>
#include <Adafruit_ADS1X15.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

int led = 13;
int smoke_sensor = A0;  //Connected to A0 pin of NodeMCU
float temp_smoke_sensor;

Adafruit_BMP280 bmp;  // use I2C interface
Adafruit_AHTX0 aht;
Adafruit_Sensor* bmp_temp = bmp.getTemperatureSensor();
Adafruit_Sensor* bmp_pressure = bmp.getPressureSensor();
Adafruit_ADS1015 ads;

// MQTT Topics
#define MQTT_PUB_TEMP "esp/bme680/temperature"
#define MQTT_PUB_HUM  "esp/bme680/humidity"
#define MQTT_PUB_PRES "esp/bme680/pressure"
#define MQTT_PUB_GAS  "esp/bme680/gas"
#define MQTT_PUB_HIDRO "esp/bme680/hidro"

// Replace with your network credentials
const char* ssid = "****";
const char* password = "****";

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "http://****:8080/api/v1/data/";

// Server Mosquitto MQTT Broker
#define MQTT_HOST IPAddress(*, *, *, *)

// Mosquitto port number
#define MQTT_PORT 1883

WiFiClient client;

//MQTT client and timer
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

//WiFi connection handler
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 10000;        // Interval at which to publish sensor readings

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}


void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);
  pinMode(smoke_sensor, INPUT);
  while (!Serial) delay(100);  // wait for native usb
  Serial.println(F("BMP280 Sensor event test"));

 /* Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("......");
  } */
  
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  mqttClient.setCredentials("hunglp", "phihung21522115");
  connectToWifi();
  unsigned status;

  aht.begin();
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1)
      ;
  }
  status = bmp.begin();
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                     "try a different address!"));
    Serial.print("SensorID was: 0x");
    Serial.println(bmp.sensorID(), 16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  bmp_temp->printSensorDetails();
}

void loop() {
  int led_status = digitalRead(led);
  int16_t adc0, adc1, adc2, adc3;
  float volts0, volts1, volts2, volts3;
  /*while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("......");
  }*/
  unsigned long currentMillis = millis();
  
  // Every X number of seconds (interval = 10 seconds) 
  // it publishes a new MQTT message
  if (currentMillis - previousMillis >= interval) {
    // Save the last time a new reading was published
    previousMillis = currentMillis;
    
	temp_smoke_sensor = analogRead(smoke_sensor);

    sensors_event_t temp_event, pressure_event;
    bmp_temp->getEvent(&temp_event);
    bmp_pressure->getEvent(&pressure_event);

    sensors_event_t humidity, temp;

    adc0 = ads.readADC_SingleEnded(0);
    volts0 = ads.computeVolts(adc0);
    float h2_sensor = volts0;

    aht.getEvent(&humidity, &temp);  // populate temp and humidity objects with fresh data

    Serial.print(F("Temperature(*C)="));
    Serial.print(temp_event.temperature);
    Serial.print(",");
    Serial.print(F("Pressure(hPa)="));
    Serial.print(pressure_event.pressure);
    Serial.print(",");
    Serial.print("Current_Gas_Level:");
    Serial.print(temp_smoke_sensor);
    Serial.print(",");
    Serial.print("Current_H2_Level:");
    Serial.print(h2_sensor);
    Serial.print(",");
    Serial.print("Humidity(%rH):");
    Serial.println(humidity.relative_humidity);
	
    // Publish an MQTT message on topic esp/bme680/temperature
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_TEMP, 1, true, String(temp_event.temperature).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_TEMP, packetIdPub1);
    Serial.printf("Message: %.2f \n", temp_event.temperature);

    // Publish an MQTT message on topic esp/bme680/humidity
    uint16_t packetIdPub2 = mqttClient.publish(MQTT_PUB_HUM, 1, true, String(humidity.relative_humidity).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_HUM, packetIdPub2);
    Serial.printf("Message: %.2f \n", humidity.relative_humidity);

    // Publish an MQTT message on topic esp/bme680/pressure
    uint16_t packetIdPub3 = mqttClient.publish(MQTT_PUB_PRES, 1, true, String(pressure_event.pressure).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_PRES, packetIdPub3);
    Serial.printf("Message: %.2f \n", pressure_event.pressure);

    // Publish an MQTT message on topic esp/bme680/gas
    uint16_t packetIdPub4 = mqttClient.publish(MQTT_PUB_GAS, 1, true, String(temp_smoke_sensor).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_GAS, packetIdPub4);
    Serial.printf("Message: %.2f \n", temp_smoke_sensor);
	
	// Publish an MQTT message on topic esp/bme680/hidro
    uint16_t packetIdPub4 = mqttClient.publish(MQTT_PUB_GAS, 1, true, String(h2_sensor).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_HIDRO, packetIdPub4);
    Serial.printf("Message: %.2f \n", h2_sensor);
  }
}
