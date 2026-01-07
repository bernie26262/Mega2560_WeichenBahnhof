#pragma once

#include <Arduino.h>
#include "pins_mega1.h"

// ==================================================
// Konfiguration
// ==================================================
#define WEICHE_QUEUE_SIZE   8
#define WEICHE_PULSE_MS     120
#define WEICHE_COOLDOWN_MS  300

// ==================================================
// Status-Struktur je Weiche
// ==================================================
struct WeichenStatus
{
    bool lastIstGerade;
    bool lastSollGerade;
    bool lastCheckOk;
    bool everChecked;
};

// ==================================================
// WeichenHub
// ==================================================
class WeichenHub
{
public:
    struct Cmd
    {
        uint8_t index;
        bool    gerade;
    };

    // Lifecycle
    void begin();
    void update();

    // Commands
    bool enqueueWeiche(uint8_t index, bool gerade);

    // Diagnose / Status Builder
    uint16_t buildWeichenBits() const;            // Soll (Gerade)
    uint16_t buildWeichenIstBits() const;         // Ist (Gerade)
    uint16_t buildWeichenOkBits() const;          // OK / FAIL
    uint16_t buildWeichenSlowActiveBits() const;  // Reduktion aktiv (pinRed LOW)

    // Einzelabfragen
    bool lastCheckOk(uint8_t index) const;
    bool lastIstGerade(uint8_t index) const;

private:
    // --------------------------------------------------
    // Queue
    // --------------------------------------------------
    Cmd     m_q[WEICHE_QUEUE_SIZE];
    uint8_t m_qHead = 0;
    uint8_t m_qTail = 0;
    uint8_t m_qCount = 0;

    // --------------------------------------------------
    // Aktiver Impuls
    // --------------------------------------------------
    bool     m_pulseActive = false;
    Cmd      m_activeCmd;
    uint32_t m_pulseUntilMs = 0;

    // --------------------------------------------------
    // Zustände je Weiche
    // --------------------------------------------------
    bool           m_state[NUM_WEICHEN];        // Sollzustand
    uint32_t       m_cooldownUntil[NUM_WEICHEN];
    WeichenStatus  m_status[NUM_WEICHEN];

    // Reduktions-Relais aktiv (Debug / Diagnose)
    bool           m_redActive[NUM_WEICHEN];

    // --------------------------------------------------
    // Interne Helfer
    // --------------------------------------------------
    bool pop(Cmd& out);
    void startPulse(const Cmd& cmd);
    void stopPulseAndCheck(const Cmd& cmd);
    bool readIstGerade(uint8_t index) const;
};
