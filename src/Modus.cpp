#include "Modus.h"
#include "WeichenHub.h"
#include "pins_mega1.h"

// WeichenHub ist global (wie im restlichen Projekt)
extern WeichenHub weichenHub;

void ModusController::begin()
{
    // Startup-Freigabe wie Mega2:
    // Bootet in MANUELL. AUTOMATIK wird später per ESP (CMD_SET_MODE) gesetzt,
    // sobald Startup-Checklist/Selbsttests erledigt sind.
    //
    // Dadurch wird die Grundstellung (MANUELL->AUTOMATIK) garantiert erst
    // nach dem Benutzer-Flow gefahren.
    m_mode = BetriebsModus::MANUELL;
}

void ModusController::setMode(BetriebsModus newMode)
{
    // nichts tun, wenn Modus gleich bleibt
    if (newMode == m_mode)
        return;

    // --------------------------------------------------
    // Übergang: MANUELL -> AUTOMATIK
    // → alle Weichen in definierte Grundstellung fahren
    // --------------------------------------------------
    if (m_mode == BetriebsModus::MANUELL &&
        newMode == BetriebsModus::AUTOMATIK)
    {
        for (uint8_t w = 0; w < NUM_WEICHEN; ++w)
        {
            // alle Weichen in Grundstellung fahren
        (void)weichenHub.enqueueGrundstellung();
        }
    }

    // --------------------------------------------------
    // Übergang: AUTOMATIK -> MANUELL
    // → bewusst nichts tun (Zustand einfrieren)
    // --------------------------------------------------

    m_mode = newMode;
}
