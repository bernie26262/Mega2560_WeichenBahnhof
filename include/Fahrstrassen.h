#pragma once
#include <Arduino.h>

#include "BetriebsstellenConfig.h"
#include "SensorHub.h"
#include "WeichenHub.h"

// --------------------------------------------------
// Fahrstraßen-Controller
// --------------------------------------------------
class Fahrstrassen
{
public:
    void begin();

    void handleSensorEvents(const SensorHub& hub,
                            WeichenHub& weichenHub);

    uint8_t getCounter(uint8_t fs) const;
    int8_t  activeRoute() const { return m_activeRoute; }

private:
    struct FSState
    {
        uint8_t counter = 0;
        bool    active  = false;
    };

    FSState m_fsState[NUM_STW_FS];
    int8_t  m_activeRoute = -1;

    void applySteps(uint8_t fsIndex, WeichenHub& weichenHub);
};