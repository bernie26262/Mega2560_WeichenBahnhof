#include "Weiche.h"
#include <EEPROM.h>

Weiche::Weiche()
: ziel(GERADE),
  _index(255),
  _impulsAktiv(false),
  _impulsRichtung(GERADE),
  _impulsStart(0),
  _lastImpulseEnd(0)
{}

void Weiche::configure(uint8_t index, const WPins &cfg) {
    _index = index;
    _pins  = cfg;
}

void Weiche::begin() {
    pinMode(_pins.pinG, OUTPUT);
    pinMode(_pins.pinA, OUTPUT);
    digitalWrite(_pins.pinG, HIGH);
    digitalWrite(_pins.pinA, HIGH);

    if (_pins.hasRed && _pins.pinRed != 255) {
        pinMode(_pins.pinRed, OUTPUT);
        digitalWrite(_pins.pinRed, HIGH); // aus (low-aktiv)
    }

    pinMode(_pins.pinRueck, INPUT_PULLUP);

    // EEPROM-Zustand initial einlesen
    uint8_t v = EEPROM.read(EEPROM_WEICHEN_BASE + _index);
    if (v <= 1) {
        ziel = static_cast<Richtung>(v);
    } else {
        ziel = GERADE;
    }

    updateReduktion();
}

bool Weiche::istAbzweig() const {
    return (digitalRead(_pins.pinRueck) == LOW);
}

void Weiche::setCoils(Richtung r, bool on) {
    // low-aktiv
    if (on) {
        if (r == GERADE) {
            digitalWrite(_pins.pinG, LOW);
            digitalWrite(_pins.pinA, HIGH);
        } else {
            digitalWrite(_pins.pinA, LOW);
            digitalWrite(_pins.pinG, HIGH);
        }
    } else {
        digitalWrite(_pins.pinG, HIGH);
        digitalWrite(_pins.pinA, HIGH);
    }
}

void Weiche::schalten() {
    unsigned long now = millis();

    if (_impulsAktiv) {
        // gerade noch Impuls aktiv → ignorieren
        return;
    }

    // Impuls starten
    _impulsAktiv    = true;
    _impulsRichtung = ziel;
    _impulsStart    = now;

    setCoils(_impulsRichtung, true);
}

void Weiche::updateReduktion() {
    if (!_pins.hasRed || _pins.pinRed == 255) return;

    // LOGIK: Rückmelder niedrig = abbiegen = reduzierte Fahrspannung ein
    bool abzweig = istAbzweig();
    digitalWrite(_pins.pinRed, abzweig ? LOW : HIGH);
}

void Weiche::update() {
    unsigned long now = millis();

    // Impuls-Handling
    if (_impulsAktiv) {
        if (now - _impulsStart >= _impulsDauer) {
            _impulsAktiv = false;
            setCoils(_impulsRichtung, false);
            _lastImpulseEnd = now;

            // Zustand speichern
            EEPROM.update(EEPROM_WEICHEN_BASE + _index, (uint8_t)_impulsRichtung);

            // Event senden
            pushEvent(EVT_WEICHE, _index, (uint8_t)_impulsRichtung);
        }
    }

    // Rückmelder überwachen und Reduktion setzen (auch im manuellen Betrieb!)
    updateReduktion();
}
