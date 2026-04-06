#pragma once
#include <Arduino.h>
#include "SensorHub.h"
#include "pins_mega1.h"
#include "BetriebsstellenConfig.h"
#include "TrackPowerHub.h"

// Optionales Debug-Logging für BahnhofController.
// Aktivieren via build_flags: -DBHF_DEBUG
#ifdef BHF_DEBUG
  #define BHF_LOG(...) Serial.print(__VA_ARGS__)
  #define BHF_LOGLN(...) Serial.println(__VA_ARGS__)
#else
  #define BHF_LOG(...) do {} while (0)
  #define BHF_LOGLN(...) do {} while (0)
#endif

// Zustand eines einzelnen Bahnhofs
struct BahnhofState
{
    bool     occupied      = false;  // Zug im Bahnhof?
    bool     powerOn       = true;   // Stromgleis an/aus
    bool     entryLatched  = false;  // Einfahrt dieses Zyklus bereits verarbeitet?
    bool     timerRunning  = false;  // Timer aktiv?
    uint32_t timerStartMs  = 0;      // Startzeitpunkt des Timers
    uint32_t timerDuration = 0;      // Wartezeit in ms (z.B. 5000)
};

class BahnhofController
{
public:
    void begin();
    
    void setPowerHub(TrackPowerHub* hub) { m_powerHub = hub; }

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
    TrackPowerHub* m_powerHub = nullptr;

    void handleEinfahrten(const SensorHub& hub, uint32_t changed);
    void handleTimerStarts(const SensorHub& hub, uint32_t changed);
    void handleEntryResets(const SensorHub& hub, uint32_t changed);
    void handleTimers();
};