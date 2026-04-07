/*
 * Smart Farm IoT Dashboard - ESP8266 NodeMCU Code
 * This code connects to WiFi, reads sensors, posts data to the Flask app,
 * and retrieves relay commands to control connected devices.
 * 
 * --- PIN MAPPING ---
 * Relay1 (Generic) -> D1 (GPIO 5)
 * Relay2 (Generic) -> D2 (GPIO 4)
 * Relay3 (Generic) -> D3 (GPIO 0)
 * Pump             -> D4 (GPIO 2)
 * Light            -> D5 (GPIO 14)
 * DHT11 Sensor     -> D6 (GPIO 12)
 * Water Level      -> A0 (Analog 0)
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"
#include <ArduinoJson.h> // Make sure to install ArduinoJson library (v6 or v7)

// --- WiFi Credentials ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- Server Details ---
// IMPORTANT: Use your Render URL here, e.g., "https://your-app-name.onrender.com"
// Do not include a trailing slash
const char* serverUrl = "https://your-flask-app.onrender.com"; 

// --- Pin Definitions ---
#define RELAY1_PIN D1
#define RELAY2_PIN D2
#define RELAY3_PIN D3
#define PUMP_PIN   D4
#define LIGHT_PIN  D5

#define DHTPIN D6
#define DHTTYPE DHT11
#define WATER_SENSOR_PIN A0

DHT dht(DHTPIN, DHTTYPE);

// Timestamps for non-blocking delays
unsigned long previousMillis = 0;
const long interval = 5000; // Send/Receive data every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize Pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  // Default state: OFF (Assuming active HIGH relays. If Active LOW, change to HIGH)
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);

  dht.begin();

  // Connect to WiFi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      // 1. Read Sensors
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      int waterLevelValue = analogRead(WATER_SENSOR_PIN);
      
      // Convert analog reading (0-1024) to a percentage (0-100%)
      // Adapt these min/max values based on your specific sensor calibration
      int waterPercent = map(waterLevelValue, 0, 1023, 0, 100);
      if(waterPercent < 0) waterPercent = 0;
      if(waterPercent > 100) waterPercent = 100;

      // Check if DHT reading fails
      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        t = 0.0;
        h = 0.0;
      }

      Serial.printf("Temp: %.1f C, Humidity: %.1f %%, Water Level: %d %%\n", t, h, waterPercent);

      // 2. Set up HTTP Client
      WiFiClientSecure client;
      client.setInsecure(); // Required for HTTPS (Render) without dealing with certificates
      HTTPClient http;

      // 3. POST Sensor Data
      String dataUrl = String(serverUrl) + "/data";
      http.begin(client, dataUrl);
      http.addHeader("Content-Type", "application/json");

      // Create JSON doc for sending data
      StaticJsonDocument<200> docSend;
      docSend["temperature"] = t;
      docSend["humidity"] = h;
      docSend["water_level"] = waterPercent;
      
      String jsonPayload;
      serializeJson(docSend, jsonPayload);

      int httpResponseCode = http.POST(jsonPayload);
      if (httpResponseCode > 0) {
        Serial.printf("POST /data Response: %d\n", httpResponseCode);
      } else {
        Serial.printf("POST /data Error: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();

      // 4. GET Commands (Device States)
      String commandsUrl = String(serverUrl) + "/commands";
      http.begin(client, commandsUrl);
      
      int httpGetCode = http.GET();
      if (httpGetCode > 0) {
        String payload = http.getString();
        // Serial.println("Received commands: " + payload);
        
        StaticJsonDocument<300> docReceived;
        DeserializationError error = deserializeJson(docReceived, payload);
        
        if (!error) {
          int r1 = docReceived["relay1"];
          int r2 = docReceived["relay2"];
          int r3 = docReceived["relay3"];
          int pump = docReceived["pump"];
          int light = docReceived["light"];

          // Apply state to pins
          digitalWrite(RELAY1_PIN, r1 == 1 ? HIGH : LOW);
          digitalWrite(RELAY2_PIN, r2 == 1 ? HIGH : LOW);
          digitalWrite(RELAY3_PIN, r3 == 1 ? HIGH : LOW);
          digitalWrite(PUMP_PIN, pump == 1 ? HIGH : LOW);
          digitalWrite(LIGHT_PIN, light == 1 ? HIGH : LOW);
          
          Serial.println("Device states updated.");
        } else {
          Serial.print("JSON parsing failed: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.printf("GET /commands Error: %s\n", http.errorToString(httpGetCode).c_str());
      }
      http.end();

    } else {
      Serial.println("WiFi Disconnected. Reconnecting...");
      WiFi.reconnect();
    }
  }
}
