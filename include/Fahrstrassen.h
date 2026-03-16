#pragma once
#include <Arduino.h>

// WICHTIG: zuerst die Anzahl der Fahrstraßen!
#include "Fahrstrassen_defs.h"

#include "types.h"
#include "SensorHub.h"
#include "WeichenHub.h"

// --------------------------------------------------
// Weichen-Schalt-Schritt einer Fahrstraße
// --------------------------------------------------
struct WeichenSchaltSchritt
{
    uint8_t  weichenIndex;
    Richtung richtung;
    uint8_t  minCount;
};

// --------------------------------------------------
// Definition einer Fahrstraße
// --------------------------------------------------
struct SteuerungWeichenDefinition
{
    const char* name;           // Sprechender Name der Fahrstraße
    uint8_t     triggerSensor;  // Auslösender Schaltgleis-Sensor
    uint8_t     resetSensors[4];// Sensoren, die Zähler/aktiv-Status zurücksetzen
    uint8_t     numReset;

    const WeichenSchaltSchritt* steps;
    uint8_t  numSteps;
};

// --------------------------------------------------
// ZENTRALE Fahrstraßen-Definition
// (Definition in src/Fahrstrassen_defs.cpp)
// --------------------------------------------------
extern const SteuerungWeichenDefinition STW_DEFS[NUM_STW_FS];

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