#include "WeichenHub.h"

// Hinweis: Rückmeldung: LOW = ABBIEGEN, HIGH = GERADE (siehe readIstGerade)
// WEICHEN_GRUNDSTELLUNG kommt aus pins_mega1.h via WeichenHub.h

void WeichenHub::begin()
{
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        // -----------------------------
        // Grundstellung als Soll setzen
        // -----------------------------
        bool grundGerade = (WEICHEN_GRUNDSTELLUNG[i] == GERADE);
        m_state[i] = grundGerade;

        m_status[i] = WeichenStatus{};
        m_status[i].lastSollGerade = grundGerade;
        m_status[i].lastIstGerade  = readIstGerade(i);
        m_status[i].lastCheckOk   = true;
        m_status[i].everChecked  = false;

        m_cooldownUntil[i] = 0;
        m_redActive[i]     = false;

        // LOW-Level-Relais: AUS = HIGH
        pinMode(WEICHEN_PINS[i].pinG, OUTPUT);
        pinMode(WEICHEN_PINS[i].pinA, OUTPUT);
        digitalWrite(WEICHEN_PINS[i].pinG, HIGH);
        digitalWrite(WEICHEN_PINS[i].pinA, HIGH);

        // Reduktions-Relais (optional)
        if (WEICHEN_PINS[i].hasRed && WEICHEN_PINS[i].pinRed != 255)
        {
            pinMode(WEICHEN_PINS[i].pinRed, OUTPUT);
            digitalWrite(WEICHEN_PINS[i].pinRed, HIGH); // AUS
        }

        // Rückmelder
        pinMode(WEICHEN_PINS[i].pinRueck, INPUT_PULLUP);
    }

    m_qHead = m_qTail = m_qCount = 0;
    m_pulseActive = false;
    
    // Selftest reset
    m_stActive = false;
    m_stDone   = false;
    m_stState  = StState::Idle;
    m_stMask   = 0;
    m_stIndex  = 0;
    m_stPhase  = 0;
    m_stUntilMs = 0;
    m_stOk1Mask = m_stOk2Mask = m_stFailMask = 0;
}

bool WeichenHub::enqueueWeiche(uint8_t index, bool gerade)
{
    // Während Selftest keine normalen Commands annehmen (deterministisch bleiben)
    if (m_stActive) return false;
    if (index >= NUM_WEICHEN) return false;
    if (m_qCount >= WEICHE_QUEUE_SIZE) return false;

    m_q[m_qTail] = Cmd{ index, gerade };
    m_qTail = (m_qTail + 1) % WEICHE_QUEUE_SIZE;
    m_qCount++;

    // Soll-Zustand merken
    m_state[index] = gerade;
    m_status[index].lastSollGerade = gerade;

    return true;
}

bool WeichenHub::enqueueGrundstellung()
{
    // Während Selftest NICHT mischen
    if (m_stActive) return false;

    bool ok = true;
    for (uint8_t w = 0; w < NUM_WEICHEN; ++w)
    {
        const bool gerade = (WEICHEN_GRUNDSTELLUNG[w] == GERADE);
        // wenn Queue voll -> ok=false, aber wir versuchen weiter (best effort)
        if (!enqueueWeiche(w, gerade))
            ok = false;
    }
    return ok;
}


bool WeichenHub::pop(Cmd& out)
{
    if (m_qCount == 0) return false;
    out = m_q[m_qHead];
    m_qHead = (m_qHead + 1) % WEICHE_QUEUE_SIZE;
    m_qCount--;
    return true;
}

bool WeichenHub::readIstGerade(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    const WPins& p = WEICHEN_PINS[index];

    // Rückmeldung: LOW = ABBIEGEN, HIGH = GERADE
    return (digitalRead(p.pinRueck) == HIGH);
}

void WeichenHub::startPulseCustom(uint8_t index, bool gerade, uint32_t pulseMs)
{
    const WPins& p = WEICHEN_PINS[index];

    // Reduktion EIN
    if (p.hasRed && p.pinRed != 255)
    {
        digitalWrite(p.pinRed, LOW);
        m_redActive[index] = true;
    }

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Gewünschte Spule EIN
    digitalWrite(gerade ? p.pinG : p.pinA, LOW);

    m_pulseActive  = true;
    m_activeCmd    = Cmd{ index, gerade };
    m_pulseUntilMs = millis() + pulseMs;
}


void WeichenHub::startPulse(const Cmd& cmd)
{
    startPulseCustom(cmd.index, cmd.gerade, WEICHE_PULSE_MS);
}

void WeichenHub::stopPulseOnly(uint8_t index)
{
    const WPins& p = WEICHEN_PINS[index];

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Reduktion AUS
    if (p.hasRed && p.pinRed != 255)
    {
        digitalWrite(p.pinRed, HIGH);
        m_redActive[index] = false;
    }
}

void WeichenHub::stopPulseAndCheck(const Cmd& cmd)
{
    const WPins& p = WEICHEN_PINS[cmd.index];

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Reduktion AUS
    if (p.hasRed && p.pinRed != 255)
    {
        digitalWrite(p.pinRed, HIGH);
        m_redActive[cmd.index] = false;
    }

    bool istGerade  = readIstGerade(cmd.index);
    bool sollGerade = cmd.gerade;

    WeichenStatus& st = m_status[cmd.index];
    st.lastIstGerade  = istGerade;
    st.lastSollGerade = sollGerade;
    st.lastCheckOk    = (istGerade == sollGerade);
    st.everChecked    = true;

    m_cooldownUntil[cmd.index] = millis() + WEICHE_COOLDOWN_MS;
    m_pulseActive = false;
}

bool WeichenHub::startSelftest(uint16_t mask)
{
    if (m_stActive) return false;

    // Queue leeren (keine Altlasten)
    m_qHead = m_qTail = m_qCount = 0;
    m_pulseActive = false;

    m_stMask = mask;
    if (m_stMask == 0) m_stMask = 0xFFFF;

    m_stOk1Mask = 0;
    m_stOk2Mask = 0;
    m_stFailMask = 0;

    m_stIndex = 0;
    m_stPhase = 0;
    m_stState = StState::Idle;
    m_stDone = false;
    m_stActive = true;

    // Skip to first valid index
    (void)selftestPickNextIndex();

    Serial.println(F("[M1] Weichen selftest started"));
    return true;
}

bool WeichenHub::selftestPickNextIndex()
{
    while (m_stIndex < NUM_WEICHEN)
    {
        if ((m_stMask & (1u << m_stIndex)) != 0)
            return true;
        m_stIndex++;
    }
    return false;
}

void WeichenHub::selftestStartPulse(uint32_t nowMs)
{
    // Zielrichtung = immer Gegenrichtung zur aktuellen Ist-RM
    const bool istGeradeNow = readIstGerade(m_stIndex);
    m_stTargetGerade = !istGeradeNow;

    // Puls starten (500ms)
    startPulseCustom(m_stIndex, m_stTargetGerade, WEICHE_SELFTEST_PULSE_MS);
    m_stState = StState::Pulse;
    m_stUntilMs = nowMs + WEICHE_SELFTEST_PULSE_MS;

    Serial.print(F("[M1] ST pulse ON  W"));
    Serial.print(m_stIndex);
    Serial.print(F(" -> "));
    Serial.print(m_stTargetGerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" (ist="));
    Serial.print(istGeradeNow ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" phase="));
    Serial.print(m_stPhase);
    Serial.println(F(")"));
}

void WeichenHub::selftestStartSettle(uint32_t nowMs)
{
    // Puls beenden (Spulen AUS + Reduktion AUS), dann settle abwarten
    stopPulseOnly(m_stIndex);
    m_pulseActive = false;

    m_stState = StState::Settle;
    m_stUntilMs = nowMs + WEICHE_SELFTEST_SETTLE_MS;

    Serial.print(F("[M1] ST pulse OFF W"));
    Serial.print(m_stIndex);
    Serial.println(F(" -> settle"));
}

void WeichenHub::selftestEvalAndAdvance(uint32_t nowMs)
{
    const bool istGerade = readIstGerade(m_stIndex);
    const bool ok = (istGerade == m_stTargetGerade);

    const uint16_t bit = (1u << m_stIndex);
    if (m_stPhase == 0)
    {
        if (ok) m_stOk1Mask |= bit;
        else
        {
            // Wenn Toggle#1 fail, Weiche ist defekt -> Toggle#2 skippen, aber Selftest läuft weiter
            m_stFailMask |= bit;
        }
    }
    else
    {
        if (ok) m_stOk2Mask |= bit;
        else    m_stFailMask |= bit;
    }

    Serial.print(F("[M1] ST eval W"));
    Serial.print(m_stIndex);
    Serial.print(F(" phase="));
    Serial.print(m_stPhase);
    Serial.print(F(" target="));
    Serial.print(m_stTargetGerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" ist="));
    Serial.print(istGerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" ok="));
    Serial.println(ok ? F("1") : F("0"));

    // Phase advance:
    // - If phase 0 failed => skip phase 1 for this turnout (directly move to next turnout)
    if (m_stPhase == 0 && !ok)
    {
        Serial.print(F("[M1] ST skip W"));
        Serial.print(m_stIndex);
        Serial.println(F(" phase=1 (phase0 failed)"));
        m_stPhase = 0;
        m_stIndex++;
        (void)selftestPickNextIndex();
        m_stState = StState::Idle;
        return;
    }

    // Otherwise: toggle twice per turnout
    if (m_stPhase == 0)
    {
        m_stPhase = 1;
        m_stState = StState::Idle;
        return;
    }

    // phase 1 done -> next turnout
    m_stPhase = 0;
    m_stIndex++;
    (void)selftestPickNextIndex();
    m_stState = StState::Idle;
}

void WeichenHub::selftestUpdate(uint32_t nowMs)
{
    if (!m_stActive) return;

    // fertig, wenn keine Weiche mehr übrig
    if (m_stIndex >= NUM_WEICHEN)
    {
        m_stActive = false;
        m_stDone = true;
        m_stState = StState::Done;

        // Summary pro Weiche
        for (uint8_t i = 0; i < NUM_WEICHEN; i++)
        {
            if ((m_stMask & (1u << i)) == 0) continue;
            const bool ph1 = (m_stOk1Mask & (1u << i)) != 0;
            const bool ph2 = (m_stOk2Mask & (1u << i)) != 0;
            const bool fail = (m_stFailMask & (1u << i)) != 0;
            const bool overall = (!fail) && ph1 && ph2;

            Serial.print(F("[M1] ST sum W"));
            Serial.print(i);
            Serial.print(F(": PH1="));
            Serial.print(ph1 ? F("OK") : F("FAIL"));
            Serial.print(F(" PH2="));
            Serial.print(ph2 ? F("OK") : F("FAIL"));
            Serial.print(F(" overall="));
            Serial.println(overall ? F("OK") : F("FAIL"));
        }

        Serial.println(F("[M1] Weichen selftest done"));
        return;
    }

    switch (m_stState)
    {
        case StState::Idle:
            selftestStartPulse(nowMs);
            break;

        case StState::Pulse:
            if ((int32_t)(nowMs - m_stUntilMs) >= 0)
                selftestStartSettle(nowMs);
            break;

        case StState::Settle:
            if ((int32_t)(nowMs - m_stUntilMs) >= 0)
                selftestEvalAndAdvance(nowMs);
            break;

        case StState::Done:
        default:
            break;
    }
}


void WeichenHub::update()
{
    const uint32_t now = millis();

    // Selftest hat Vorrang und blockiert Queue/Normalbetrieb
    if (m_stActive)
    {
        selftestUpdate(now);
        return;
    }

    if (m_pulseActive)
    {
        if ((int32_t)(now - m_pulseUntilMs) >= 0)
            stopPulseAndCheck(m_activeCmd);
        return;
    }

    uint8_t tries = m_qCount;
    while (tries--)
    {
        Cmd cmd;
        if (!pop(cmd)) return;

        if ((int32_t)(now - m_cooldownUntil[cmd.index]) >= 0)
        {
            startPulse(cmd);
            return;
        }

        // noch im Cooldown → hinten wieder einreihen
        m_q[m_qTail] = cmd;
        m_qTail = (m_qTail + 1) % WEICHE_QUEUE_SIZE;
        m_qCount++;
    }
}

uint16_t WeichenHub::buildWeichenBits() const
{
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
        if (m_state[i]) bits |= (1U << i);
    return bits;
}

uint16_t WeichenHub::buildWeichenIstBits() const
{
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
        if (lastIstGerade(i)) bits |= (1U << i);
    return bits;
}

uint16_t WeichenHub::buildWeichenOkBits() const
{
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        // noch nie geprüft => OK, damit UI nicht rot startet
        bool ok = m_status[i].everChecked ? m_status[i].lastCheckOk : true;
        if (ok) bits |= (1U << i);
    }
    return bits;
}

uint16_t WeichenHub::buildWeichenSlowActiveBits() const
{
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
        if (m_redActive[i]) bits |= (1U << i);
    return bits;
}

bool WeichenHub::lastCheckOk(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    // noch nie geprüft => OK, damit UI nicht rot startet
    return m_status[index].everChecked ? m_status[index].lastCheckOk : true;
}

bool WeichenHub::lastIstGerade(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    return m_status[index].everChecked
        ? m_status[index].lastIstGerade
        : readIstGerade(index);
}
