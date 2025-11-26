#pragma once
#include <Arduino.h>
#include "config.h"
#include "EventQueue.h"
#include "debug.h"

class Weiche {
public:
    Richtung ziel; // Sollstellung (wird von Steuerung gesetzt)

    Weiche();

    void configure(uint8_t index, const WPins &cfg);
    void begin();
    void update();

    void schalten();         // startet Impuls, non-blocking
    bool istAbzweig() const; // true, wenn Rückmelder LOW

private:
    uint8_t _index;
    WPins   _pins;

    bool    _impulsAktiv;
    Richtung _impulsRichtung;
    unsigned long _impulsStart;
    const unsigned long _impulsDauer = 500;   // ms
    const unsigned long _schutzPause = 1000;  // ms
    unsigned long _lastImpulseEnd;

    void setCoils(Richtung r, bool on);
    void updateReduktion();
};
