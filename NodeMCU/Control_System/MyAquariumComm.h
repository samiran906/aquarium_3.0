#ifndef MY_AQUARIUM_COMM_H
#define MY_AQUARIUM_COMM_H

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

class MyAquariumComm {
  public:
    MyAquariumComm();

    void begin(const char* host, int port, const char* path);
    void loop();
    void sendSensorData(float temperature);
    void updateActuatorStatus(bool feeder, bool thermostat, bool filter, bool lights, bool CO2);
    void sendFlags(bool feeder_status, uint8_t feeder_duration, uint8_t season_setting, bool reset_flag, uint8_t light_duration);
    void onActuatorCommand(void (*callback)(const String&, bool));
    void onFlagCommand(void (*callback)(const String&, int));

  private:
    WebSocketsClient webSocket;
    String server_host;
    int server_port;
    String ws_path;
    void (*actuatorCallback)(const String&, bool) = nullptr;
    void (*flagCallback)(const String&, int) = nullptr;

    void onWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);
};

#endif
