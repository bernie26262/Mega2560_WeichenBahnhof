#pragma once
#include <Arduino.h>
#include "Weiche.h"
#include "config.h"
#include "debug.h"

struct WeichenSchaltSchritt {
    uint8_t  weicheIndex;
    Richtung ziel;
    uint16_t schwelle; // 0=immer, n=bei Counter==n, 2+FS-Sonderfall: >=2
};

struct SteuerungWeichenDefinition {
    uint8_t  sensorIndex;

    uint8_t  resetSensors[4];
    uint8_t  numResetSensors;

    const WeichenSchaltSchritt* steps;
    uint8_t  numSteps;
};

class SteuerungWeichen {
public:
    SteuerungWeichen(
        Weiche* weichenArray,
        const SteuerungWeichenDefinition* defs,
        uint8_t numFS
    );

    void onSensorTrigger(uint8_t sensorIndex);
    void update() {} // aktuell keine Hintergrundlogik
    void resetCounter(uint8_t fsIndex);

    // NEU: Snapshot der Counter für Full-Transfer
    void getCounters(uint16_t* out, uint8_t max) const;

private:
    static constexpr uint8_t MAX_FS = 16;

    Weiche* _weichen;
    const SteuerungWeichenDefinition* _defs;
    uint8_t _numFS;

    uint16_t _counter[MAX_FS];

    void handleRouteTrigger(uint8_t fsIndex);
};
