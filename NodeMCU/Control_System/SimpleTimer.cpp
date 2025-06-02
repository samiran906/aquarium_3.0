#include "SimpleTimer.h"

SimpleTimer::SimpleTimer(unsigned long intervalMillis) {
  interval = intervalMillis;
  lastTime = millis();
  running = true;
}

bool SimpleTimer::isElapsed() {
  if (!running) return false;
  if (millis() - lastTime >= interval) {
    lastTime = millis();  // auto-reset
    return true;
  }
  return false;
}

void SimpleTimer::reset() {
  lastTime = millis();
}

void SimpleTimer::stop() {
  running = false;
}

void SimpleTimer::start() {
  if (!running) {
    running = true;
    lastTime = millis();
  }
}

void SimpleTimer::setInterval(unsigned long intervalMillis) {
  interval = intervalMillis;
}
