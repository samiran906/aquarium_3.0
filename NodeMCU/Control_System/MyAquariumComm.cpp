#include "MyAquariumComm.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>

extern void setCommStatus(bool connected);

MyAquariumComm::MyAquariumComm() {}

void MyAquariumComm::begin(const char* host, int port, const char* path) {
  server_host = host;
  server_port = port;
  ws_path = path;

  webSocket.begin(server_host.c_str(), server_port, ws_path.c_str());
  webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    this->onWebSocketEvent(type, payload, length);
  });
  webSocket.setReconnectInterval(5000);

  Serial.println("WebSocket initialized");
}

void MyAquariumComm::loop() {
  webSocket.loop();
}

void MyAquariumComm::onActuatorCommand(void (*callback)(const String&, bool)) {
  actuatorCallback = callback;
}

void MyAquariumComm::onFlagCommand(void (*callback)(const String&, int)) {
  flagCallback = callback;
}

void MyAquariumComm::sendSensorData(float tempC) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, String("http://") + server_host + ":" + server_port + "/api/sensors");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["temperature"] = tempC;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  //Serial.print("Sensor POST response: ");
  //Serial.println(httpCode);

  http.end();
}

void MyAquariumComm::updateActuatorStatus(bool feeder, bool thermostat, bool filter, bool lights, bool CO2) {
  StaticJsonDocument<256> doc;
  doc["type"] = "actuator_status";
  doc["source"] = "device";  // <-- NEW
  JsonObject data = doc.createNestedObject("data");
  data["feeder"] = feeder;
  data["thermostat"] = thermostat;
  data["filter"] = filter;
  data["lights"] = lights;
  data["CO2"] = CO2;

  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}

void MyAquariumComm::sendFlags(bool feeder_status, uint8_t feeder_duration, uint8_t season_setting, bool reset_flag, uint8_t light_duration) {
  StaticJsonDocument<256> doc;
  doc["type"] = "flag_status";
  doc["source"] = "device";  // <-- NEW
  JsonObject data = doc.createNestedObject("data");
  data["feeder_status"] = feeder_status;
  data["feeder_duration"] = feeder_duration;
  data["season_setting"] = season_setting;
  data["reset_flag"] = reset_flag;
  data["light_duration"] = light_duration;

  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
}

void MyAquariumComm::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
      Serial.println("Failed to parse WebSocket JSON");
      return;
    }

    const char* msgType = doc["type"];
    JsonObject data = doc["data"];

    if (strcmp(msgType, "actuator_update") == 0 && actuatorCallback) {
      for (JsonPair kv : data) {
        String name = kv.key().c_str();
        bool state = kv.value().as<bool>();
        actuatorCallback(name, state);
      }
    }

    if (strcmp(msgType, "flag_update") == 0 && flagCallback) {
      for (JsonPair kv : data) {
        String name = kv.key().c_str();
        int value = kv.value().as<int>();  // Handles both bool and uint8_t
        flagCallback(name, value);
      }
    }
  }
  // NEW: handle connection status
  if (type == WStype_CONNECTED) {
    Serial.println("[WebSocket] Connected");
    setCommStatus(true);  // Notify sketch: connection established
  }

  if (type == WStype_DISCONNECTED) {
    Serial.println("[WebSocket] Disconnected");
    setCommStatus(false);  // Notify sketch: disconnected
  }
}
