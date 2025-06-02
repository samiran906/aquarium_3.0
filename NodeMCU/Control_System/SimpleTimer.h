#ifndef SIMPLETIMER_H
#define SIMPLETIMER_H

#include <Arduino.h>

class SimpleTimer {
  private:
    unsigned long lastTime;
    unsigned long interval;
    bool running;

  public:
    SimpleTimer(unsigned long intervalMillis);
    bool isElapsed();
    void reset();
    void stop();
    void start();
    void setInterval(unsigned long intervalMillis);
};

#endif
