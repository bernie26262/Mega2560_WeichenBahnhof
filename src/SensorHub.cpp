#include "SensorHub.h"

SensorHub::SensorHub()
: _count(0),
  _debounceMs(3000)
{}

void SensorHub::begin(const uint8_t* pins, uint8_t count, unsigned long debounceMs) {
    if (count > MAX_SENS) count = MAX_SENS;
    _count = count;
    _debounceMs = debounceMs;

    for (uint8_t i = 0; i < _count; i++) {
        _pins[i] = pins[i];
        pinMode(_pins[i], INPUT_PULLUP);
        _locked[i] = false;
        _lastTime[i] = 0;
    }
}
