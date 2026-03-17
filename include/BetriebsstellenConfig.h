#pragma once
#include <Arduino.h>
#include "types.h"
// ==================================================
// Zentrale fachliche Anlagen-Konfiguration Mega1
//
// Hier liegt bewusst NUR die Betriebslogik:
// - Bahnhofs-Zuordnung (Einfahrt, Timerstart, Timerdauer)
// - Fahrstraßen-Zuordnung (Trigger, Reset, Schaltfolgen)
//
// Die physische Verdrahtung (Pins, Sensor-Index -> Arduino-Pin,
// Weichen-Pins) bleibt in pins_mega1.*
// ==================================================

// --------------------------------------------------
// Bahnhöfe
// --------------------------------------------------
constexpr uint8_t BHF_COUNT = 4;

struct BahnhofConfig
{
    const char* name;
    uint8_t     einfahrtSensorIndex;
    uint8_t     timerStartSensorIndex;
    uint32_t    timerDurationMs;
    const char* kommentar;
};

extern const BahnhofConfig BAHNHOF_CONFIG[BHF_COUNT];

// --------------------------------------------------
// Fahrstraßen
// --------------------------------------------------
constexpr uint8_t NUM_STW_FS = 5;

struct WeichenSchaltSchritt
{
    uint8_t  weichenIndex;
    Richtung richtung;
    uint8_t  minCount;
};

struct FahrstrassenConfig
{
    const char* name;
    uint8_t     triggerSensor;
    uint8_t     resetSensors[4];
    uint8_t     numReset;
    const WeichenSchaltSchritt* steps;
    uint8_t     numSteps;
    const char* kommentar;
};

extern const FahrstrassenConfig FAHRSTRASSEN_CONFIG[NUM_STW_FS];