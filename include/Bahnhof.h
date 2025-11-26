#pragma once
#include <Arduino.h>
#include "config.h"
#include "EventQueue.h"
#include "debug.h"

class Bahnhof {
public:
    Bahnhof();

    void configure(const BahnhofConfig &cfg, uint8_t index);
    void begin();

    void handleSensor(uint8_t sIdx, Betriebsmodus mode);
    void update(Betriebsmodus mode);

private:
    uint8_t _index;
    uint8_t _stromPin;
    uint8_t _sensorEinfahrt;
    uint8_t _sensorTimerStart;
    unsigned long _haltezeit;

    bool _timerAktiv;
    unsigned long _timerStart;

    void setStrom(bool an);
};
