#pragma once
#include <Arduino.h>
#include "EventQueue.h"
#include "debug.h"

class SensorHub {
public:
    SensorHub();

    void begin(const uint8_t* pins, uint8_t count, unsigned long debounceMs);

    template<typename F>
    void update(F onTrigger) {
        if (_count == 0) return;
        unsigned long now = millis();

        for (uint8_t i = 0; i < _count; i++) {
            bool raw = (digitalRead(_pins[i]) == LOW); // LOW-aktiv
            if (raw) {
                if (!_locked[i]) {
                    _locked[i]   = true;
                    _lastTime[i] = now;

                    DBGLN(String("Sensor S") + i + " TRIGGER");
                    pushEvent(EVT_SENSOR, i, 1);
                    onTrigger(i);
                } else {
                    _lastTime[i] = now;
                }
            }

            if (_locked[i] && (now - _lastTime[i] >= _debounceMs)) {
                _locked[i] = false;
            }
        }
    }

private:
    static constexpr uint8_t MAX_SENS = 32;

    uint8_t  _pins[MAX_SENS];
    uint8_t  _count;
    unsigned long _debounceMs;

    bool    _locked[MAX_SENS];
    unsigned long _lastTime[MAX_SENS];
};
