#include "Modus.h"
#include "WeichenHub.h"
#include "pins_mega1.h"

// WeichenHub ist global (wie im restlichen Projekt)
extern WeichenHub weichenHub;

void ModusController::begin()
{
    // Default beim Boot: AUTOMATIK
    // m_mode initialisieren, damit setMode() sicher arbeitet.
    m_mode = BetriebsModus::MANUELL;
    setMode(BetriebsModus::AUTOMATIK);
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
            // Richtung -> bool (explizit, typsicher)
            bool gerade = (WEICHEN_GRUNDSTELLUNG[w] == GERADE);
            weichenHub.enqueueWeiche(w, gerade);
        }
    }

    // --------------------------------------------------
    // Übergang: AUTOMATIK -> MANUELL
    // → bewusst nichts tun (Zustand einfrieren)
    // --------------------------------------------------

    m_mode = newMode;
}
