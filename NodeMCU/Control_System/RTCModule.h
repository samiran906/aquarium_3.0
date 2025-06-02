#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <time.h>

class RTCModule {
  private:
    unsigned long syncMillis;
    unsigned long syncedEpoch;

  public:
    RTCModule();
    void syncTime(unsigned long epoch);
    unsigned long now();
    void getTimeComponents(uint8_t &hours, uint8_t &mins, uint8_t &secs, uint8_t &weekdays, int timezoneOffsetSeconds = 0);
};

#endif
