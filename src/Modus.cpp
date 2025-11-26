#include "Modus.h"
#include <EEPROM.h>

Modus::Modus(uint8_t tPin, uint8_t ledA, uint8_t ledM, Weiche* w, uint8_t n)
: _state(MOD_AUTOMATIK),
  _tasterPin(tPin),
  _ledAuto(ledA),
  _ledMan(ledM),
  _weichen(w),
  _numWeichen(n),
  _lastPress(0),
  _lastRelease(0),
  _lastState(HIGH),
  _clickCount(0),
  _blinkEnabled(false),
  _blinkTimer(0),
  _blinkState(false),
  _recoveryActive(false),
  _recoveryIsGrund(true),
  _recoveryIndex(0),
  _recoveryNextTime(0),
  _recoveryInterval(600)
{}

void Modus::begin() {
    pinMode(_tasterPin, INPUT_PULLUP);
    pinMode(_ledAuto, OUTPUT);
    pinMode(_ledMan, OUTPUT);

    digitalWrite(_ledAuto, HIGH);
    digitalWrite(_ledMan, LOW);
}

void Modus::setModus(Betriebsmodus m) {
    _state = m;

    digitalWrite(_ledAuto, (m == MOD_AUTOMATIK) ? HIGH : LOW);
    digitalWrite(_ledMan,  (m == MOD_MANUELL)   ? HIGH : LOW);

    pushEvent(EVT_MODUS_CHANGE, 0, (uint8_t)m);
}

void Modus::handleClick() {
    if (_clickCount == 1) {
        // einfacher Klick → Modus wechseln
        if (_state == MOD_AUTOMATIK) {
            setModus(MOD_MANUELL);
        } else {
            setModus(MOD_AUTOMATIK);
            applyGrundstellung();
        }
    } else if (_clickCount == 2) {
        // Doppelklick → Recovery aus EEPROM, immer AUTOMATIK
        setModus(MOD_AUTOMATIK);
        applyRecoveryFromEEPROM();
    }
}

void Modus::applyGrundstellung() {
    _recoveryActive    = true;
    _recoveryIsGrund   = true;
    _recoveryIndex     = 0;
    _recoveryNextTime  = 0;
    _blinkEnabled      = true;
    _blinkTimer        = millis();
    _blinkState        = false;

    DBGLN("Modus: Grundstellung wird geladen (sequenziell)");
    // Counter der Fahrstraßen werden NICHT verändert
}

void Modus::applyRecoveryFromEEPROM() {
    _recoveryActive    = true;
    _recoveryIsGrund   = false;
    _recoveryIndex     = 0;
    _recoveryNextTime  = 0;
    _blinkEnabled      = true;
    _blinkTimer        = millis();
    _blinkState        = false;

    DBGLN("Modus: Recovery aus EEPROM (sequenziell)");
    // Counter der Fahrstraßen werden NICHT verändert
}

void Modus::blinkUpdate() {
    if (!_blinkEnabled) return;

    unsigned long now = millis();
    if (now - _blinkTimer >= 200) {
        _blinkTimer = now;
        _blinkState = !_blinkState;
        digitalWrite(_ledAuto, _blinkState ? HIGH : LOW);
    }
}

void Modus::recoveryUpdate() {
    if (!_recoveryActive) return;

    unsigned long now = millis();
    if (now < _recoveryNextTime) return;

    if (_recoveryIndex >= _numWeichen) {
        // fertig
        _recoveryActive  = false;
        _blinkEnabled    = false;
        digitalWrite(_ledAuto, (_state == MOD_AUTOMATIK) ? HIGH : LOW);

        pushEvent(EVT_RECOVERY_DONE, 0, _recoveryIsGrund ? 1 : 2);
        DBGLN("Modus: Recovery/Grundstellung abgeschlossen");
        return;
    }

    uint8_t wIndex = _recoveryIndex++;

    Richtung r;
    if (_recoveryIsGrund) {
        r = WEICHEN_GRUNDSTELLUNG[wIndex];
    } else {
        uint8_t v = EEPROM.read(EEPROM_WEICHEN_BASE + wIndex);
        if (v > 1) v = 0;
        r = static_cast<Richtung>(v);
    }

    _weichen[wIndex].ziel = r;
    _weichen[wIndex].schalten();

    _recoveryNextTime = now + _recoveryInterval;
}

void Modus::update() {
    // Taster-Logik
    bool cur = digitalRead(_tasterPin);
    unsigned long now = millis();

    if (_lastState == HIGH && cur == LOW) {
        _lastPress = now;
    }

    if (_lastState == LOW && cur == HIGH) {
        unsigned long dt = now - _lastPress;
        if (dt > 20 && dt < 400) {
            _clickCount++;
        }
        _lastRelease = now;
    }

    if (_clickCount > 0 && (now - _lastRelease) > 300) {
        handleClick();
        _clickCount = 0;
    }

    _lastState = cur;

    blinkUpdate();
    recoveryUpdate();
}
