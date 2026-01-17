#pragma once
#include <Arduino.h>

// --------------------------------------------------
// Betriebsmodus der Anlage
// --------------------------------------------------
enum class BetriebsModus : uint8_t
{
    MANUELL   = 0,
    AUTOMATIK = 1
};

class ModusController
{
public:
    void begin();

    // Modus setzen (inkl. Übergangslogik)
    void setMode(BetriebsModus newMode);

    // Aktuellen Modus abfragen
    BetriebsModus mode() const { return m_mode; }

    // Komfortfunktionen
    bool isAuto() const   { return m_mode == BetriebsModus::AUTOMATIK; }
    bool isManual() const{ return m_mode == BetriebsModus::MANUELL; }

private:
    // Startup-Freigabe wie Mega2:
    // Bootet konservativ in MANUELL. AUTOMATIK wird erst durch ESP gesetzt,
    // nachdem Startup-Checklist/Selbsttests erledigt sind.
    BetriebsModus m_mode = BetriebsModus::MANUELL;
};
