#pragma once
#include <Arduino.h>
#include "SensorHub.h"
#include "pins_mega1.h"

// Zustand eines einzelnen Bahnhofs
struct BahnhofState
{
    bool     occupied      = false;  // Zug im Bahnhof?
    bool     powerOn       = true;   // Stromgleis an/aus
    bool     timerRunning  = false;  // Timer aktiv?
    uint32_t timerStartMs  = 0;      // Startzeitpunkt des Timers
    uint32_t timerDuration = 0;      // Wartezeit in ms (z.B. 5000)
};

class BahnhofController
{
public:
    void begin();

    // Aufruf aus loop(): Sensorereignisse + Timer verarbeiten
    void update(const SensorHub& hub);

    // Zustand eines Bahnhofs (0..BHF_COUNT-1)
    const BahnhofState& state(uint8_t bhf) const { return m_bhf[bhf]; }

    // Fehlerflags (für spätere Erweiterung)
    uint8_t errorFlags() const { return m_errorFlags; }

    // Manuelle Freigabe aus WebUI: Strom wieder einschalten, Timer stoppen
    void manualRelease(uint8_t bhf);

    // Letzter Bahnhof, bei dem ein Ereignis passiert ist (0..3, 255 = keiner)
    uint8_t lastEventBhf() const { return m_lastEventBhf; }

private:
    BahnhofState m_bhf[BHF_COUNT];
    uint8_t      m_errorFlags  = 0;
    uint8_t      m_lastEventBhf = 255; // 255 = kein Event

    void handleEinfahrten(const SensorHub& hub, uint32_t changed);
    void handleTimerStarts(const SensorHub& hub, uint32_t changed);
    void handleTimers();
};
