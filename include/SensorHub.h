#pragma once
#include <Arduino.h>
#include "pins_mega1.h"

// Zug-Entprellzeit pro Sensor (erste Achse zählt)
constexpr uint32_t SENSOR_TRAIN_DEBOUNCE_MS = 4000;

class SensorHub
{
public:
    void begin();
    void update();

    bool isActive(uint8_t index) const;
    uint32_t changedMask() const;
    uint16_t buildKontaktBits() const;

private:
    uint32_t m_stateMask   = 0;
    uint32_t m_changedMask = 0;

    struct DebounceState
    {
        bool     blocked       = false;  // Sensor gesperrt (Zug läuft)
        bool     trainPresent  = false;  // aktuell belegt
        uint32_t unblockAtMs   = 0;      // Sperrzeit-Ende
    };

    DebounceState m_db[NUM_SENSORS];
};
