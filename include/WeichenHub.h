#pragma once
#include <Arduino.h>
#include "pins_mega1.h"
#include "payload.h"

// --------------------------------------------------
// LOW-Level-Relais:
// LOW  = Relais EIN (Spule zieht an)
// HIGH = Relais AUS
//
// Rückmeldung:
// pinRueck: LOW = ABBIEGEN, HIGH = GERADE
// Prüfung erfolgt NACH Impulsende.
// --------------------------------------------------

constexpr uint8_t  WEICHE_QUEUE_SIZE   = 24;
constexpr uint16_t WEICHE_PULSE_MS     = 500;
constexpr uint16_t WEICHE_COOLDOWN_MS  = 1000;

class WeichenHub
{
public:
    void begin();

    // Auftrag in Queue legen (non-blocking)
    bool enqueueWeiche(uint8_t index, bool gerade);

    // Scheduler, MUSS zyklisch in loop() aufgerufen werden
    void update();

    // letzter Soll-Zustand (für Payload/GUI)
    bool getWeiche(uint8_t index) const;

    // Bitmaske aller Soll-Weichenstellungen (Bit i = Weiche i gerade)
    uint16_t buildWeichenBits() const;

    // letzte Iststellung aus Rückmelder (Bit i = Weiche i gerade)
    uint16_t buildWeichenIstBits() const;

    // Bitmaske: 1 = letzte Prüfung OK, 0 = Fehler (Bit i)
    uint16_t buildWeichenOkBits() const;

    bool lastCheckOk(uint8_t index) const;
    bool lastIstGerade(uint8_t index) const;

private:
    struct Cmd
    {
        uint8_t index;
        bool    gerade;
    };

    struct WeichenStatus
    {
        bool lastSollGerade = false;
        bool lastIstGerade  = false;
        bool lastCheckOk    = true;
        bool everChecked    = false;
    };

    // FIFO-Queue
    Cmd     m_q[WEICHE_QUEUE_SIZE];
    uint8_t m_qHead  = 0;
    uint8_t m_qTail  = 0;
    uint8_t m_qCount = 0;

    // Logische Soll-Zustände
    bool     m_state[NUM_WEICHEN] = {false};

    // Status inkl. Rückmeldung
    WeichenStatus m_status[NUM_WEICHEN];

    // Cooldown-Zeitpunkte pro Weiche
    uint32_t m_cooldownUntil[NUM_WEICHEN] = {0};

    // Aktiver Impuls
    bool     m_pulseActive   = false;
    Cmd      m_activeCmd{};
    uint32_t m_pulseUntilMs  = 0;

    bool pop(Cmd& out);

    void startPulse(const Cmd& cmd);
    void stopPulseAndCheck(const Cmd& cmd);

    bool readIstGerade(uint8_t index) const;
};
