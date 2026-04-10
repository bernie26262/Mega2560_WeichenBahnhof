#pragma once
#include <Arduino.h>
#include "pins_mega1.h"

// Zug-Entprellzeit pro Sensor (erste Achse zählt)
constexpr uint32_t SENSOR_TRAIN_DEBOUNCE_MS = 1500;
// Glitch-Filter: Rohwert muss so lange stabil sein, bevor wir ihn als "gültig" werten.
// (Schützt gegen kurze EMV-Spikes / Übersprechen bei offenen/hochohmigen Leitungen)
constexpr uint32_t SENSOR_GLITCH_MS = 20;

#ifndef DEBUG_M1_SENSOR_S0_LOG
#define DEBUG_M1_SENSOR_S0_LOG 0
#endif

class SensorHub
{
public:
    void begin();
    void update();

    bool isActive(uint8_t index) const;
    uint32_t changedMask() const;
    void clearChangedMask();
    uint32_t buildKontaktBits() const;


    // -------------------------------------------------
    // Digitale Sensoren – Diagnose (read-only)
    // Konvention: "aktiv" = logischer 1-Wert = Pin ist LOW (INPUT_PULLUP)
    // Rise/Fall sind logische Flanken bezogen auf "aktiv":
    //   rise: 0 -> 1  (wurde aktiv)
    //   fall: 1 -> 0  (wurde inaktiv)
    //
    // Rise/Fall-Masken sind "sticky since last DIAG read" und werden nach
    // erfolgreichem DIAG-Read via clearEdgeMasks() zurückgesetzt.
    // -------------------------------------------------
    uint32_t activeMask() const;
    
    // Returns true once when any sensor-relevant DIAG data changed since last check.
    // Intended for main loop to raise M1_PEND_DIAG (so ESP polls CMD_GET_DIAG).
    bool consumeDiagDirty();

    uint32_t riseMask() const;
    uint32_t fallMask() const;
    void clearEdgeMasks();


private:
    uint32_t m_stateMask   = 0;
    uint32_t m_changedMask = 0;
    uint32_t m_riseMask    = 0;
    uint32_t m_fallMask    = 0;
    
    // Change-detect for DIAG pending generation (keeps DRDY quiet when nothing changes)
    uint32_t m_lastActiveForPending = 0;
    uint32_t m_lastRiseForPending   = 0;
    uint32_t m_lastFallForPending   = 0;

    struct DebounceState
    {
        bool     blocked       = false;  // Sensor gesperrt (Zug läuft)
        bool     trainPresent  = false;  // aktuell belegt
        uint32_t unblockAtMs   = 0;      // Sperrzeit-Ende
        
        // Rohwert-Stabilisierung
        bool     rawInit       = false;  // Baseline gesetzt?
        bool     rawLast       = false;  // letzter Rohwert (active?)
        uint32_t rawSinceMs    = 0;      // seit wann ist rawLast stabil?
    };

    DebounceState m_db[NUM_SENSORS];
};
