#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_AHTX0.h>
#include <WiFiClient.h>
#include <Adafruit_ADS1X15.h>

int led = 13;
int smoke_sensor = A0;  //Connected to A0 pin of NodeMCU
float temp_smoke_sensor;

Adafruit_BMP280 bmp;  // use I2C interface
Adafruit_AHTX0 aht;
Adafruit_Sensor* bmp_temp = bmp.getTemperatureSensor();
Adafruit_Sensor* bmp_pressure = bmp.getPressureSensor();
Adafruit_ADS1015 ads;

// Replace with your network credentials
const char* ssid = "****";
const char* password = "*****";

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "********";
const char* requests = "/";

WiFiClient client;

void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);
  pinMode(smoke_sensor, INPUT);
  while (!Serial) delay(100);  // wait for native usb
  Serial.println(F("BMP280 Sensor event test"));

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("......");
  }
  unsigned status;
  //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
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
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("......");
  }
  temp_smoke_sensor = analogRead(smoke_sensor); // 1023.0 * ((21400 - 15300) + 15300);

  sensors_event_t temp_event, pressure_event;
  bmp_temp->getEvent(&temp_event);
  bmp_pressure->getEvent(&pressure_event);

  sensors_event_t humidity, temp;

  adc0 = ads.readADC_SingleEnded(0);
  volts0 = ads.computeVolts(adc0);
  float h2_sensor = volts0; // 5.0 * ((21400 - 15300) + 15300);

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
  //  Serial.println("");

  // Serial.println();
  // Create a JSON payload with the sensor data
  String payload = "{";
  payload += "\"Temperature\":" + String(temp_event.temperature) + ",";
  payload += "\"Pressure\":" + String(pressure_event.pressure) + ",";
  payload += "\"Gas\":" + String(temp_smoke_sensor) + ",";
  payload += "\"H2\":" + String(h2_sensor) + ",";
  payload += "\"Humidity\":" + String(humidity.relative_humidity);
  payload += "}";

  // Send the POST request
  HTTPClient http;
  int httpResponseCode;
  if (http.begin(client, serverName)) {
    //http.addHeader("Content-Type", "application/json");
    Serial.println("Successful");
    int httpResponseCode = http.POST(payload);
    //httpResponseCode = http.GET();
  }

  // Check the response
  if (httpResponseCode >= 0) {
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println(response);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Error sending HTTP request");
  }
  http.end();
  delay(5000);  // Wait for 5 seconds before sending the next request
}