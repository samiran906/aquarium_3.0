/*////////////////////////////////////////////////////////////////
********************Aquarium Control System***********************
************************Version 0.0.1*****************************
////////////////////////////////////////////////////////////////*/
#include <DallasTemperature.h>
#include <OneWire.h>
#include <math.h>
#include <Servo.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "RTCModule.h"
#include <WiFiClient.h> 
#include <time.h>
#include "SimpleTimer.h"
#include "MyAquariumComm.h"
/*////////////////////////////////////////////////////////////////
**********************Library Inclusions**************************
////////////////////////////////////////////////////////////////*/
#define ONE_WIRE_BUS    D5
#define filter_BUS      0
#define light_BUS       5
#define autoFeeder_BUS  D3
#define co2_BUS         7
#define thermostat_BUS  2
/*////////////////////////////////////////////////////////////////
*******************NODEMCU GPIO Definitions***********************
////////////////////////////////////////////////////////////////*/
bool filterFlag = 0;
bool filterFlag10min = 0;
bool lightFlag = 0;
bool co2Flag = 0;
bool autoFeederFlag = 0;
bool thermostatFlag = 0;
bool thermostatFlag10min = 0;
bool serverSyncflag = 0;
/*////////////////////////////////////////////////////////////////
*************Variables for storing local flag values**************
////////////////////////////////////////////////////////////////*/
uint8_t cloudLightFlag = 0;
uint8_t cloudFilterFlag = 0;
uint8_t cloudCo2Flag = 0;
uint8_t cloudThermostatFlag = 0;
bool cloudResetFlag = 0;
uint8_t cloudAutoFeederFlagStatus = 0;
uint8_t cloudSeasonSetting = 0;
uint8_t cloudAutoFeederDuration = 0;
uint8_t cloudLightDuration = 7;
/*////////////////////////////////////////////////////////////////
*************Variables for storing cloud flag values**************
////////////////////////////////////////////////////////////////*/
uint8_t mins = 0;
uint8_t hours = 0;
uint8_t secs = 0;
uint8_t weekdays = 0;
uint8_t i2cData = 0xFF;

const int16_t I2C_SLAVE = 0x27;

double water_temp = 0;
/*////////////////////////////////////////////////////////////////
*************Other Global Variable Declarations******************
////////////////////////////////////////////////////////////////*/
const char* ssid = "iBall-Baton";
const char* password = "Samidha@2021";
/*////////////////////////////////////////////////////////////////
***********************WiFi Credentials***************************
////////////////////////////////////////////////////////////////*/
const char* server_host = "192.168.1.10"; //IP address of Flask server
const int server_port = 5000;
const char* ws_path = "/ws";
const char* cloudTimeURL = "http://192.168.1.10:5000/api/time";
const char* heartbeatURL = "http://192.168.1.10:5000/heartbeat";
/*////////////////////////////////////////////////////////////////
***********************Server Details*****************************
////////////////////////////////////////////////////////////////*/
OneWire oneWire(ONE_WIRE_BUS);          //I2C Bus
DallasTemperature sensors(&oneWire);    //Temp Sensor
time_t cloudtime;                       //Time Library
Servo myservo;                          //Servo
WebSocketsClient webSocket;             //Websocket
RTCModule rtc;                          //RTC Library
SimpleTimer fiveSecTimer(5000);         //5 second timer
SimpleTimer tenMinsTimer(10000);        //10 minutes timer
MyAquariumComm comm;                    //Communication Library
WiFiClient client;
HTTPClient http;
/*////////////////////////////////////////////////////////////////
***********************Object Creations***************************
////////////////////////////////////////////////////////////////*/
void dailyReset()
{
  if ((hours >= 2) && (hours < 6))
  {
    cloudAutoFeederFlagStatus = 1;
    cloudLightFlag = 1;
    cloudFilterFlag = 1;
    cloudCo2Flag = 1;
    cloudThermostatFlag = 1;
  }
}
/*////////////////////////////////////////////////////////////////
Function to Daily reset all concerned Flags at various time 
intervals.
////////////////////////////////////////////////////////////////*/
void updateWaterTemp()
{
  sensors.requestTemperatures();
  //water_temp = sensors.getTempCByIndex(0);
   water_temp = water_temp + 1.0;
  if(water_temp >= 0)
  {
    comm.sendSensorData(water_temp);  // Send sensor data
  }
}
/*////////////////////////////////////////////////////////////////
Function to request for Water Temperature from the DS18b20 sensor
using the One Wire data transfer protocol. The function also 
updates the latest temperature value to the cloud.
////////////////////////////////////////////////////////////////*/
void sendI2cData(uint8_t data)
{
    Wire.beginTransmission(I2C_SLAVE);
    Wire.write(data);
    Wire.endTransmission();
}
/*////////////////////////////////////////////////////////////////
Function to send data over the I2C bus to the PCF8574 chip. 
////////////////////////////////////////////////////////////////*/
unsigned long updateTimeCloud()
{
  http.begin(client, cloudTimeURL);  // <-- updated to use WiFiClient
  int httpCode = http.GET();

  if (httpCode == 200) 
  {
    String payload = http.getString();
    int startIndex = payload.indexOf(":") + 1;
    int endIndex = payload.indexOf("}");
    String epochStr = payload.substring(startIndex, endIndex);
    unsigned long epoch = epochStr.toInt();
    http.end();
    //Serial.println(epoch);
    return epoch;
  } 
  else 
  {
    Serial.print("Failed to get time from server, HTTP code: ");
    Serial.println(httpCode);
  }

  http.end();
  return 0;
}
/*////////////////////////////////////////////////////////////////
Function that requests the current time from cloud when connected
////////////////////////////////////////////////////////////////*/
void updateHeartbeat()
{
  http.begin(client, heartbeatURL);  // <-- updated to use WiFiClient
  http.addHeader("Content-Type", "application/json");  
  int httpResponseCode = http.POST("");  // empty payload
  if (httpResponseCode > 0) 
  {
    Serial.println("[Heartbeat] Sent successfully");
  } else 
  {
    Serial.printf("[Heartbeat] Failed, code: %d\n", httpResponseCode);
  }
  http.end();
}
/*////////////////////////////////////////////////////////////////
Function that updates heartbeat to the server
////////////////////////////////////////////////////////////////*/
void startThermostat()
{
  if(thermostatFlag == 0)
  {
    serverSyncflag = 1;
    thermostatFlag = 1;
    i2cData &= ~(1 << thermostat_BUS);
    sendI2cData(i2cData);
    Serial.println("Thermostat ON");
  }
}
/*////////////////////////////////////////////////////////////////
Function that starts the Aquarium Thermostat whenever required.
////////////////////////////////////////////////////////////////*/

void stopThermostat()
{
  if(thermostatFlag == 1)
  {
    serverSyncflag = 1;
    thermostatFlag = 0;
    i2cData |= (1 << thermostat_BUS);
    sendI2cData(i2cData);
    Serial.println("Thermostat OFF");
  }
}
/*////////////////////////////////////////////////////////////////
Function that stops the Aquarium Thermostat whenever required.
////////////////////////////////////////////////////////////////*/
void checkThermostat()
{
  if ((cloudThermostatFlag <= 1) && (thermostatFlag10min <= 1))
  {
    switch(cloudSeasonSetting)
    {
      case 0: if(lightFlag == 1)                //Heating Mode Winter
              {
                if((mins>=0) && (mins <= 10))  
                {
                  startThermostat();
                }
                else
                {
                  stopThermostat();
                }
              }
              else
              {
                if(water_temp < 26.5)
                {
                  startThermostat();   
                }
        
                if(water_temp > 27.0)
                {
                  stopThermostat();
                }
               }
              break;
      case 1: if(lightFlag == 1)              //Heating Mode Normal
              {
                stopThermostat();
              }
              else
              {
                if(water_temp < 26.5)
                {
                  startThermostat();   
                }
        
                if(water_temp > 27.0)
                {
                  stopThermostat();
                }
              }
              break;
      case 2: if(lightFlag == 1)            //Cooling Mode Normal
              {
                stopThermostat();
              }
              else
              {
                if(water_temp < 27.5)
                {
                  stopThermostat();   
                }
        
                if(water_temp > 28.5)
                {
                  startThermostat();
                }
              }
              break;
      case 3: if(lightFlag == 1)            //Cooling Mode Summer
              {
                startThermostat();
              }
              else
              {
                if(water_temp < 27.5)
                {
                  stopThermostat();   
                }
        
                if(water_temp > 28.5)
                {
                  startThermostat();
                }
              }
              break;
    }
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks if Thermostat needs to be On or Off.
////////////////////////////////////////////////////////////////*/
void startFilter()
{
  if(filterFlag == 0)
  {
    serverSyncflag = 1;
    filterFlag = 1;
    i2cData |= (1 << filter_BUS);
    sendI2cData(i2cData);
    Serial.println("Filter ON");
  }
}
/*////////////////////////////////////////////////////////////////
Function that starts the Aquarium Filter whenever required.
////////////////////////////////////////////////////////////////*/

void stopFilter()
{
  if(filterFlag == 1)
  {
    serverSyncflag = 1;
    filterFlag = 0;
    filterFlag10min = 1;
    i2cData &= ~(1 << filter_BUS);
    sendI2cData(i2cData);
    Serial.println("Filter OFF");
  }
}
/*////////////////////////////////////////////////////////////////
Function that stops the Aquarium Filter whenever required.
////////////////////////////////////////////////////////////////*/
void checkFilter()
{
  if (cloudFilterFlag <= 1)
  {
    if(filterFlag10min == 0)
    {
      startFilter();
    }
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks if the Aquarium Filter needs to be On or Off.
////////////////////////////////////////////////////////////////*/
void startLight()
{
  if (lightFlag == 0)
  {
    serverSyncflag = 1;
    lightFlag = 1;
    i2cData &= ~(1 << light_BUS);
    sendI2cData(i2cData);
    Serial.println("Light ON");
  }
}
/*////////////////////////////////////////////////////////////////
Function that starts the Aquarium Light whenever required.
////////////////////////////////////////////////////////////////*/

void stopLight()
{
  if (lightFlag == 1)
  {
    serverSyncflag = 1;
    lightFlag = 0;
    i2cData |= (1 << light_BUS);
    sendI2cData(i2cData);
    Serial.println("Light OFF");
  }
}
/*////////////////////////////////////////////////////////////////
Function that stops the Aquarium Light whenever required.
////////////////////////////////////////////////////////////////*/
void checkLight()
{
  if (cloudLightFlag <= 1)
  {
    if((hours >= 14) && (hours < (cloudLightDuration + 14)))
    {
      startLight();   
    }
    
    else
    {
      stopLight();
    }
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks if the Aquarium Light needs to be On or Off.
////////////////////////////////////////////////////////////////*/
void startCo2()
{
  if(co2Flag == 0)
  {
    serverSyncflag = 1;
    co2Flag = 1;
    i2cData &= ~(1 << co2_BUS);
    sendI2cData(i2cData);
    Serial.println("Co2 ON");
  }
}
/*////////////////////////////////////////////////////////////////
Function that starts the Aquarium CO2 whenever required.
////////////////////////////////////////////////////////////////*/

void stopCo2()
{
  if(co2Flag == 1)
  {
    serverSyncflag = 1;
    co2Flag = 0;
    i2cData |= (1 << co2_BUS);
    sendI2cData(i2cData);
    Serial.println("Co2 OFF");
  }
}
/*////////////////////////////////////////////////////////////////
Function that stops the Aquarium CO2 whenever required.
////////////////////////////////////////////////////////////////*/
void checkCo2()
{
  if (cloudCo2Flag <= 1)
  {
    if((hours >= 11) && (hours < ((cloudLightDuration - 1) + 14)))
    {
      startCo2();   
    }
    else
    {
      stopCo2();
    }
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks if CO2 needs to be On or Off.
////////////////////////////////////////////////////////////////*/
void startAutoFeeder()
{ 
  serverSyncflag = 1;
  cloudAutoFeederFlagStatus++; 
  autoFeederFlag = 1;
  comm.updateActuatorStatus(autoFeederFlag, thermostatFlag, filterFlag, lightFlag, co2Flag);
  
  fiveSecTimer.reset();
  tenMinsTimer.reset();

  myservo.attach(autoFeeder_BUS,500, 2400);
  myservo.write(167);
  delay(500); 
  myservo.write(0);
  delay(2500); 
  myservo.detach();

  autoFeederFlag = 0;
  Serial.println("Feeder ON");
}
/*////////////////////////////////////////////////////////////////
Function that starts the Aquarium AutoFeeder whenever required.
////////////////////////////////////////////////////////////////*/

void checkHoursForAutoFeeder()
{
  if((hours >= 12) && (hours < 14) && (cloudAutoFeederFlagStatus == 0))
  {
    stopFilter();
    stopThermostat();
    thermostatFlag10min = 1; 
  }
    
  if((hours >= 12) && (hours < 14) && (mins >= 1) && (cloudAutoFeederFlagStatus == 0))
  {
    startAutoFeeder();   
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks hours for auroFeeder transtion.
////////////////////////////////////////////////////////////////*/
void checkAutoFeeder()
{
  switch(cloudAutoFeederDuration)
  {
    case 0: break;
    case 1: if(weekdays == 4)
            {
              checkHoursForAutoFeeder();
            }
            break;
    case 2: if((weekdays == 1) || (weekdays == 4))
            {
              checkHoursForAutoFeeder();
            }
            break;
    case 3: if((weekdays == 1) || (weekdays == 3) || (weekdays == 5))
            {
              checkHoursForAutoFeeder();
            }
            break;
    case 4: if((weekdays == 1) || (weekdays == 3) || (weekdays == 5) || (weekdays == 0))
            {
              checkHoursForAutoFeeder();
            }
            break;
    case 5: if((weekdays == 1) || (weekdays == 2) || (weekdays == 3) || (weekdays == 4) || (weekdays == 5))
            {
              checkHoursForAutoFeeder();
            }
            break;
    case 6: if((weekdays == 1) || (weekdays == 2) || (weekdays == 3) || (weekdays == 4) || (weekdays == 5) || (weekdays == 6))
            {
              checkHoursForAutoFeeder();
            }
            break;   
    case 7: checkHoursForAutoFeeder();
            break;      
  }
}
/*////////////////////////////////////////////////////////////////
Function that checks if AutoFeeder needs to br triggered or not.
////////////////////////////////////////////////////////////////*/
void flagReset()
{
  serverSyncflag = 1;
  cloudLightFlag = 1;
  cloudFilterFlag = 1;
  cloudCo2Flag = 1;
  cloudThermostatFlag = 1;
  cloudResetFlag = 0;
  Serial.printf("Flags Reset");
}
/*////////////////////////////////////////////////////////////////
Function to reset all internal flags triggered from the cloud.
////////////////////////////////////////////////////////////////*/
void handleFlagCommand(const String& flag, int value) 
{
  Serial.printf("Flag %s set to %d\n", flag.c_str(), value);

  if (flag == "feeder_status") cloudAutoFeederFlagStatus = value;
  else if (flag == "feeder_duration") cloudAutoFeederDuration = value;
  else if (flag == "season_setting") cloudSeasonSetting = value;
  else if (flag == "reset_flag" && value == 1) 
  {
    flagReset();
  }
  else if (flag == "light_duration") cloudLightDuration = value;
}
/*////////////////////////////////////////////////////////////////
Function Callback for flag commands from Flask
////////////////////////////////////////////////////////////////*/
void handleActuatorCommand(const String& actuator, bool state) 
{
  Serial.printf("Actuator %s set to %s\n", actuator.c_str(), state ? "ON" : "OFF");

  if (actuator == "feeder") 
  {
    if (state == 1)
    {
      startAutoFeeder();
    }
  }
  else if (actuator == "thermostat")
  {
    if ((state == 1) && (thermostatFlag == 0))
    {
      cloudThermostatFlag++;
      startThermostat();
    }
    else if((state == 0) && (thermostatFlag == 1))
    {
      cloudThermostatFlag++;
      stopThermostat();
    }
  }
  else if (actuator == "filter") 
  {
   if ((state == 1) && (filterFlag == 0))
    {
      cloudFilterFlag++;
      startFilter();
    }
    else if((state == 0) && (filterFlag == 1))
    {
      cloudFilterFlag++;
      stopFilter();
    }
  }
  else if (actuator == "lights") 
  {
    if ((state == 1) && (lightFlag == 0))
    {
      cloudLightFlag++;
      startLight();
    }
    else if((state == 0) && (lightFlag == 1))
    {
      cloudLightFlag++;
      stopLight();
    }
  }
  else if (actuator == "CO2") 
  {
    if ((state == 1) && (co2Flag == 0))
    {
      cloudCo2Flag++;
      startCo2();
    }
    else if((state == 0) && (co2Flag == 1))
    {
      cloudCo2Flag++;
      stopCo2();
    }
  }
}
/*////////////////////////////////////////////////////////////////
Function Callback for actuator commands from Flask
////////////////////////////////////////////////////////////////*/
void checkStatusChange()
{
  if(serverSyncflag == 1)
  {
    comm.updateActuatorStatus(autoFeederFlag, thermostatFlag, filterFlag, lightFlag, co2Flag);
    comm.sendFlags(cloudAutoFeederFlagStatus, cloudAutoFeederDuration, cloudSeasonSetting, cloudResetFlag, cloudLightDuration);
    Serial.printf("Flags autoFeederFlag, thermostatFlag, filterFlag, lightFlag, co2Flag %02d:%02d:%02d:%02d:%02d\n",autoFeederFlag, thermostatFlag, filterFlag, lightFlag, co2Flag);
    Serial.printf("Flags cloudThermostatFlag, cloudFilterFlag, cloudLightFlag, cloudCo2Flag %02d:%02d:%02d:%02d\n",cloudThermostatFlag, cloudFilterFlag, cloudLightFlag, cloudCo2Flag);
    Serial.printf("Flags cloudAutoFeederFlagStatus, cloudSeasonSetting, cloudAutoFeederDuration, cloudResetFlag, cloudLightDuration %02d:%02d:%02d:%02d:%02d\n",cloudAutoFeederFlagStatus, cloudSeasonSetting, cloudAutoFeederDuration, cloudResetFlag, cloudLightDuration);
    serverSyncflag = 0;
  }
}
/*////////////////////////////////////////////////////////////////
Function to check NodeMcu actuators and flags status for sync with server
////////////////////////////////////////////////////////////////*/
void scheduler10Min()
{
  filterFlag10min = 0;
  thermostatFlag10min = 0;
  unsigned long epoch = updateTimeCloud();
  if (epoch > 0) 
  {
    rtc.syncTime(epoch);
  }
  comm.sendFlags(cloudAutoFeederFlagStatus, cloudAutoFeederDuration, cloudSeasonSetting, cloudResetFlag, cloudLightDuration);
  //Serial.printf("Current time is %02d:%02d:%02d:%02d\n", hours, mins, secs, weekdays); 
}
/*////////////////////////////////////////////////////////////////
Function that schedules jobs with a frequency of 10 minutes.
////////////////////////////////////////////////////////////////*/
void scheduler5Sec()
{  
    updateHeartbeat();  
    updateWaterTemp();
    checkFilter();
    checkLight();
    checkCo2();
    checkThermostat();
    dailyReset();
    checkAutoFeeder();
    rtc.getTimeComponents(hours, mins, secs, weekdays, 19800);  // 19800 = IST offset in seconds
    checkStatusChange();
}
/*////////////////////////////////////////////////////////////////
Function that schedules jobs with a frequency of 5 seconds.
////////////////////////////////////////////////////////////////*/

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  myservo.attach(autoFeeder_BUS, 500, 2400);
  myservo.write(0);   
  delay(2500);
  myservo.detach();

  pinMode(LED_BUILTIN, OUTPUT);
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // toggle LED
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  
  sensors.begin();
  
  unsigned long epoch = updateTimeCloud();
  if (epoch > 0) 
  {
    rtc.syncTime(epoch);
  }
  
  comm.onActuatorCommand(handleActuatorCommand);
  comm.onFlagCommand(handleFlagCommand);
  comm.begin(server_host, server_port, ws_path);
}
/*////////////////////////////////////////////////////////////////
Setup and Initialization function for all Functionalities.
////////////////////////////////////////////////////////////////*/

void loop() 
{
  comm.loop();
  if (fiveSecTimer.isElapsed()) 
  {
    scheduler5Sec();
  }

  if (tenMinsTimer.isElapsed()) 
  {
    scheduler10Min();
  }
}
/*////////////////////////////////////////////////////////////////
Infinite Loop function to continuosly update cloud and timers.
////////////////////////////////////////////////////////////////*/
