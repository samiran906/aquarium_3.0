#include "RTCModule.h"
#include <math.h>

RTCModule::RTCModule() {
  syncMillis = 0;
  syncedEpoch = 0;
}

void RTCModule::syncTime(unsigned long epoch) {
  syncedEpoch = epoch;
  syncMillis = millis();
}

unsigned long RTCModule::now() {
  unsigned long elapsedMillis = millis() - syncMillis;
  return syncedEpoch + (elapsedMillis / 1000);
}

void RTCModule::getTimeComponents(uint8_t &hours, uint8_t &mins, uint8_t &secs, uint8_t &weekdays, int timezoneOffsetSeconds) {
  time_t nowTime = now() + timezoneOffsetSeconds;
  struct tm * timeinfo = gmtime(&nowTime);  // Use gmtime + offset

  hours = timeinfo->tm_hour;
  mins = timeinfo->tm_min;
  secs = timeinfo->tm_sec;
  weekdays = fmod((floor(nowTime/86400)+4), 7);
}
