#pragma once
#include <Arduino.h>
#include "pins_mega1.h"

// Active-low Relais: LOW = Strom AN, HIGH = Strom AUS
class TrackPowerHub
{
public:
    void begin();
    void setPower(uint8_t bhf, bool on);
};
