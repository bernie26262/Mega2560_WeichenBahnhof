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
// Selbsttest (Weichen)
// ==================================================
// Impuls und settle nach deiner Definition:
// - Puls 500ms
// - settle 500ms, dann RM prüfen
#define WEICHE_SELFTEST_PULSE_MS   500
#define WEICHE_SELFTEST_SETTLE_MS  500

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
        // --------------------------------------------------
    // Weichen-Selbsttest (deterministisch, queue-free)
    // --------------------------------------------------
    bool startSelftest(uint16_t mask = 0xFFFF);
    bool isSelftestActive() const { return m_stActive; }
    bool isSelftestDone()   const { return m_stDone; }
    void clearSelftestDone() { m_stDone = false; }
    uint16_t selftestFailMask() const { return m_stFailMask; }


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

    // Alle Weichen auf definierte Grundstellung (siehe WEICHEN_GRUNDSTELLUNG)
    bool enqueueGrundstellung();
    

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
    // Selbsttest-State
    // --------------------------------------------------
    enum class StState : uint8_t { Idle = 0, Pulse, Settle, Done };
    bool     m_stActive = false;
    bool     m_stDone   = false;
    StState  m_stState  = StState::Idle;
    uint16_t m_stMask   = 0;
    uint8_t  m_stIndex  = 0;
    uint8_t  m_stPhase  = 0;   // 0 = Toggle#1, 1 = Toggle#2
    bool     m_stTargetGerade = false;
    uint32_t m_stUntilMs = 0;
    uint16_t m_stOk1Mask  = 0;
    uint16_t m_stOk2Mask  = 0;
    uint16_t m_stFailMask = 0;


    // --------------------------------------------------
    // Interne Helfer
    // --------------------------------------------------
    bool pop(Cmd& out);
    void startPulse(const Cmd& cmd);
    void stopPulseAndCheck(const Cmd& cmd);
    void stopPulseOnly(uint8_t index);
    void startPulseCustom(uint8_t index, bool gerade, uint32_t pulseMs);
    bool readIstGerade(uint8_t index) const;

    // Selftest intern
    void selftestUpdate(uint32_t nowMs);
    bool selftestPickNextIndex();
    void selftestStartPulse(uint32_t nowMs);
    void selftestStartSettle(uint32_t nowMs);
    void selftestEvalAndAdvance(uint32_t nowMs);
};
