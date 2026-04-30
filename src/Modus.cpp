#include "Modus.h"

void ModusController::begin()
{
    // Startup-Freigabe wie Mega2:
    // Bootet in MANUELL. AUTOMATIK wird später per ESP (CMD_SET_MODE) gesetzt,
    // sobald Startup-Checklist/Selbsttests erledigt sind.
    m_mode = BetriebsModus::MANUELL;
}

void ModusController::setMode(BetriebsModus newMode)
{
    // nichts tun, wenn Modus gleich bleibt
    if (newMode == m_mode)
        return;

    // --------------------------------------------------
    // Übergang: MANUELL -> AUTOMATIK
    // → bewusst nichts tun. Auto ist nur noch ein Moduswechsel.
    //    Grundstellung/Zähler-Reset erfolgt ausschließlich über Auto Reset.
    // --------------------------------------------------

    // --------------------------------------------------
    // Übergang: AUTOMATIK -> MANUELL
    // → bewusst nichts tun (Zustand einfrieren)
    // --------------------------------------------------

    m_mode = newMode;
}
