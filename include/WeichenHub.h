#pragma once

#include <Arduino.h>

// Rückmelder-Logging:
// 1 = [RM]-Logs aktiv
// 0 = [RM]-Logs aus
#ifndef DEBUG_M1_RUECKMELDER_LOG
#define DEBUG_M1_RUECKMELDER_LOG 1
#endif

// Mega1-Weichen-Selbsttest-Logging:
// 1 = [M1] Selftest-Logs aktiv
// 0 = aus
#ifndef DEBUG_M1_SELFTEST_LOG
#define DEBUG_M1_SELFTEST_LOG 1
#endif

#include "pins_mega1.h"

// ==================================================
// Konfiguration
// ==================================================
#define WEICHE_QUEUE_SIZE   16
#define WEICHE_PULSE_MS     500
#define WEICHE_COOLDOWN_MS  500


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
    uint8_t selftestCurrentIdx() const { return m_stActive ? m_stIndex : 0xFF; }



    struct Cmd
    {
        uint8_t index;
        bool    gerade;
    };

    // Lifecycle
    void begin();
    void update();
    
    // Rueckmelder-Pins regelmaessig einlesen (damit Pin-Toggles sichtbar werden)
    void pollRueckmelders(uint32_t now);

    // Commands
    //
    // WICHTIG (Spulenschonung):
    // Normale Schaltauftraege erzeugen nur dann einen tatsaechlichen
    // Schaltimpuls, wenn die aktuelle Ist-Stellung von der Zielstellung
    // abweicht. Steht die Weiche bereits korrekt, wird kein Puls ausgeloest.
    // Der Selbsttest ist davon ausgenommen und nutzt eigene Pulsfunktionen.
    //
    bool enqueueWeiche(uint8_t index, bool gerade);

    // Alle Weichen auf definierte Grundstellung (siehe WEICHEN_GRUNDSTELLUNG)
    bool enqueueGrundstellung();
    

    // Diagnose / Status Builder
    uint16_t buildWeichenBits() const;            // Soll (Gerade)
    uint16_t buildWeichenIstBits() const;         // Ist (Gerade)
    uint16_t buildWeichenOkBits() const;          // OK / FAIL
    // "Slow/Reduktion ausgewählt" (typisch Abbiegen => 1)
    uint16_t buildWeichenSlowSelectedBits() const;

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

    // Reduktions-Relais Zustand (Abbiegen => true) (Diagnose)
    bool           m_redActive[NUM_WEICHEN];

    // Rueckmelder-Poll-Rate
    uint32_t       m_lastRueckPollMs = 0;
    
    // Rueckmelder Debug: Rohwerte aller Rueckmelder (HIGH=1 / LOW=0)
    // - Log bei Aenderung + periodisch (1s)
    uint16_t       m_lastRueckRawBits = 0xFFFF;
    uint32_t       m_lastRueckLogMs   = 0;

    // --------------------------------------------------
    // Selbsttest-State
    // --------------------------------------------------
    enum class StState : uint8_t { Idle = 0, Done };
    bool     m_stActive = false;
    bool     m_stDone   = false;
    StState  m_stState  = StState::Idle;
    uint16_t m_stMask   = 0;
    uint8_t  m_stIndex  = 0;
    uint8_t  m_stPhase  = 0;   // 0 = Toggle#1, 1 = Toggle#2
    // Legacy serial-selftest vars (kept for compatibility; not used in pipelined mode)
    bool     m_stTargetGerade = false;
    uint32_t m_stUntilMs = 0;
    uint16_t m_stOk1Mask  = 0;
    uint16_t m_stOk2Mask  = 0;
    uint16_t m_stFailMask = 0;

    // --------------------------------------------------
    // Selbsttest (pipelined)
    // --------------------------------------------------
    struct StEvalItem {
        uint8_t  index;
        bool     targetGerade;
        uint32_t evalAtMs;
    };

    // pro gepulster Weiche ein Eval-Item (Ringpuffer; max NUM_WEICHEN Items)
    StEvalItem m_stEvalQ[NUM_WEICHEN];
    uint8_t    m_stEvalHead  = 0;
    uint8_t    m_stEvalTail  = 0;
    uint8_t    m_stEvalCount = 0;

    // letzter gepulster Befehl (für Logging/Eval)
    uint8_t    m_stPulseIndex = 0;
    bool       m_stPulseTargetGerade = false;


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

    // pipelined selftest helpers
    void selftestEvalQReset();
    bool selftestEvalQPush(uint8_t index, bool targetGerade, uint32_t evalAtMs);
    bool selftestEvalQPeek(StEvalItem& out) const;
    bool selftestEvalQPop();

    void selftestStartNextPulse(uint32_t nowMs);
    void selftestPulseFinished(uint32_t nowMs);
    void selftestEvalDue(uint32_t nowMs);
};
