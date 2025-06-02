/*
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MyAquariumComm.h"

// WiFi credentials
const char* ssid = "iBall-Baton";
const char* password = "Samidha@2021";

// Server IP and port
const char* server_host = "192.168.1.10";  // Replace with your server IP
const int server_port = 5000;

float temp = 0.0;

// Temperature sensor setup
#define ONE_WIRE_BUS D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Communication library instance
MyAquariumComm comm;

// Actuator states
bool feeder = false;
bool thermostat = false;
bool filter = false;
bool lights = false;

// Callback for actuator commands from Flask
void handleActuatorCommand(const String& actuator, bool state) {
  Serial.printf("Actuator %s set to %s\n", actuator.c_str(), state ? "ON" : "OFF");

  if (actuator == "feeder") feeder = state;
  else if (actuator == "thermostat") thermostat = state;
  else if (actuator == "filter") filter = state;
  else if (actuator == "lights") lights = state;

  // Optionally, control actual pins here
  // digitalWrite(pin_for_actuator, state ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  sensors.begin();

  comm.onActuatorCommand(handleActuatorCommand);
  comm.begin(server_host, server_port, "/ws");
}

unsigned long lastSend = 0;
const unsigned long interval = 5000;

void loop() {
  comm.loop();  // Handle incoming WebSocket messages

  unsigned long now = millis();
  if (now - lastSend > interval) {
    sensors.requestTemperatures();
    //float temp = sensors.getTempCByIndex(0);
    
    Serial.printf("Temperature: %.2f °C\n", temp);
    temp = temp + 1.0;
    comm.sendSensorData(temp);  // Send sensor data
    //comm.updateActuatorStatus(feeder, thermostat, filter, lights);  // Report status

    lastSend = now;
  }
}

/*
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "RTCModule.h"
#include <WiFiClient.h> 
#include <time.h>

const char* ssid = "iBall-Baton";
const char* password = "Samidha@2021";
const char* serverURL = "http://192.168.1.10:5000/api/time";

int hours, mints, sec, days;

RTCModule rtc;

unsigned long fetchEpochTime() {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverURL);  // <-- updated to use WiFiClient
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    int startIndex = payload.indexOf(":") + 1;
    int endIndex = payload.indexOf("}");
    String epochStr = payload.substring(startIndex, endIndex);
    unsigned long epoch = epochStr.toInt();
    http.end();
    return epoch;
  } else {
    Serial.print("Failed to get time from server, HTTP code: ");
    Serial.println(httpCode);
  }

  http.end();
  return 0;
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  unsigned long epoch = fetchEpochTime();
  if (epoch > 0) {
    rtc.syncTime(epoch);
  }
}

void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    rtc.getTimeComponents(hours, mints, sec, days, 19800);  // 19800 = IST offset in seconds
    Serial.printf("Current time is %02d:%02d:%02d:%02d\n", hours, mints, sec, days);
    lastPrint = millis();
  }
}


#include "SimpleTimer.h"

SimpleTimer ledTimer(1000);  // 1 second interval
SimpleTimer printTimer(5000); // 5 seconds

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  if (ledTimer.isElapsed()) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // toggle LED
  }

  if (printTimer.isElapsed()) {
    Serial.println("5 seconds passed!");
  }
}
*/

#include <ESP8266WiFi.h>
#include "MyAquariumComm.h"

// ==== WiFi credentials ====
const char* ssid = "iBall-Baton";
const char* password = "Samidha@2021";

// ==== Server settings ====
const char* server_host = "192.168.1.10";  // Change this to your Flask server IP
const int server_port = 5000;
const char* ws_path = "/ws";

MyAquariumComm comm;

// Timers
unsigned long lastActuatorUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastFlagUpdate = 0;

void actuatorCallback(const String& name, bool state) {
  Serial.print("[Actuator Command] ");
  Serial.print(name);
  Serial.print(" -> ");
  Serial.println(state ? "ON" : "OFF");
}

void flagCallback(const String& name, int value) {
  Serial.print("[Flag Command] ");
  Serial.print(name);
  Serial.print(" -> ");
  Serial.println(value);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Initialize communication
  comm.begin(server_host, server_port, ws_path);
  comm.onActuatorCommand(actuatorCallback);
  comm.onFlagCommand(flagCallback);
}

void loop() {
  comm.loop();

  unsigned long now = millis();

  // Send actuator status every 10 seconds
  if (now - lastActuatorUpdate > 10000) {
    Serial.println("[Sending actuator status]");
    comm.updateActuatorStatus(true, false, true, false, true);
    lastActuatorUpdate = now;
  }

  // Send sensor data every 15 seconds
  if (now - lastSensorUpdate > 15000) {
    Serial.println("[Sending sensor data]");
    comm.sendSensorData(26.5);  // Fake temperature
    lastSensorUpdate = now;
  }

  // Send flags every 20 seconds
  if (now - lastFlagUpdate > 20000) {
    Serial.println("[Sending flag status]");
    comm.sendFlags(true, 5, 2, false);
    lastFlagUpdate = now;
  }
}
