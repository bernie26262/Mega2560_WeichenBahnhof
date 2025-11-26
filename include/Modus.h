#pragma once
#include <Arduino.h>
#include "config.h"
#include "Weiche.h"
#include "EventQueue.h"
#include "debug.h"


class Modus {
public:
    Modus(uint8_t tPin, uint8_t ledA, uint8_t ledM, Weiche* w, uint8_t n);

    void begin();
    void update();

    Betriebsmodus current() const { return _state; }

private:
    // MODE
    Betriebsmodus _state;

    // PINS
    uint8_t _tasterPin;
    uint8_t _ledAuto;
    uint8_t _ledMan;

    // WEICHEN
    Weiche* _weichen;
    uint8_t _numWeichen;

    // TASTER / Klick-Logik
    unsigned long _lastPress;
    unsigned long _lastRelease;
    bool          _lastState;
    uint8_t       _clickCount;

    // BLINK
    bool          _blinkEnabled;
    unsigned long _blinkTimer;
    bool          _blinkState;

    // RECOVERY / GRUNDSTELLUNG Sequenz
    bool          _recoveryActive;
    bool          _recoveryIsGrund;
    uint8_t       _recoveryIndex;
    unsigned long _recoveryNextTime;
    unsigned long _recoveryInterval; // Abstand zwischen zwei Weichen

    void setModus(Betriebsmodus m);
    void handleClick();

    void blinkUpdate();
    void recoveryUpdate();

    void applyGrundstellung();
    void applyRecoveryFromEEPROM();
};
