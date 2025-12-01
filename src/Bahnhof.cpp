#include "Bahnhof.h"

Bahnhof::Bahnhof()
: _index(255),
  _stromPin(255),
  _sensorEinfahrt(255),
  _sensorTimerStart(255),
  _haltezeit(0),
  _timerAktiv(false),
  _timerStart(0),
  _stromAn(false)
{}

void Bahnhof::configure(const BahnhofConfig &cfg, uint8_t index) {
    _index            = index;
    _stromPin         = cfg.stromPin;
    _sensorEinfahrt   = cfg.sensorEinfahrt;
    _sensorTimerStart = cfg.sensorTimerStart;
    _haltezeit        = cfg.haltezeitMs;
}

void Bahnhof::begin() {
    if (_stromPin == 255) return;
    pinMode(_stromPin, OUTPUT);
    digitalWrite(_stromPin, HIGH); // Relais aus (low-aktiv) => Strom AUS
    _stromAn = false;
}

void Bahnhof::setStrom(bool an) {
    if (_stromPin == 255) return;
    digitalWrite(_stromPin, an ? LOW : HIGH); // low-aktiv
    _stromAn = an;
    pushEvent(EVT_BHF, _index, an ? 1 : 0);
}

void Bahnhof::handleSensor(uint8_t sIdx, Betriebsmodus mode) {
    if (mode != MOD_AUTOMATIK) return;

    if (sIdx == _sensorEinfahrt) {
        // Zug fährt in Bahnhof → stromlos
        DBGLN(String("Bhf") + _index + ": Einfahrt, Strom AUS");
        setStrom(false);
    }

    if (sIdx == _sensorTimerStart) {
        // Timer für Abfahrt starten
        _timerAktiv = true;
        _timerStart = millis();
        DBGLN(String("Bhf") + _index + ": Timerstart");
    }
}

void Bahnhof::update(Betriebsmodus mode) {
    if (_stromPin == 255) return;

    if (mode == MOD_MANUELL) {
        // im manuellen Modus: immer Strom AN
        if (!_timerAktiv) {
            setStrom(true);
        }
        _timerAktiv = false;
        return;
    }

    if (_timerAktiv) {
        unsigned long now = millis();
        if (now - _timerStart >= _haltezeit) {
            DBGLN(String("Bhf") + _index + ": Haltezeit abgelaufen, Strom AN");
            setStrom(true);
            _timerAktiv = false;
        }
    }
}
