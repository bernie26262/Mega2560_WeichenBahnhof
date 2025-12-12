#pragma once
#include <Arduino.h>
#include "pins_mega1.h"

// LOW-Level-Relais: LOW = Strom AUS, HIGH = Strom AN
class TrackPowerHub
{
public:
    void begin();
    void setPower(uint8_t bhf, bool on);
};
