#include "WeichenHub.h"
#include <Arduino.h>

static inline const char* gaFromHigh(bool high)
{
    return high ? "G" : "A"; // HIGH = Gerade, LOW = Abbiegen
}

// Hinweis: Rückmeldung: LOW = ABBIEGEN, HIGH = GERADE (siehe readIstGerade)
// WEICHEN_GRUNDSTELLUNG kommt aus pins_mega1.h via WeichenHub.h

static constexpr uint32_t RUECK_POLL_MS = 20;

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
        m_status[i].lastCheckOk   = true;
        m_status[i].everChecked  = false;

        m_cooldownUntil[i] = 0;

        // LOW-Level-Relais: AUS = HIGH
        pinMode(WEICHEN_PINS[i].pinG, OUTPUT);
        pinMode(WEICHEN_PINS[i].pinA, OUTPUT);
        digitalWrite(WEICHEN_PINS[i].pinG, HIGH);
        digitalWrite(WEICHEN_PINS[i].pinA, HIGH);

        // Rückmelder zuerst konfigurieren, dann Ist lesen
        pinMode(WEICHEN_PINS[i].pinRueck, INPUT_PULLUP);
        const bool istGeradeNow     = readIstGerade(i);
        m_status[i].lastIstGerade  = istGeradeNow;

        // Reduktions-Relais Zustand aus IST ableiten (Abbiegen => true)
        const bool hasRed = (WEICHEN_PINS[i].hasRed && WEICHEN_PINS[i].pinRed != 255);
        m_redActive[i] = hasRed ? (!istGeradeNow) : false;

        // Reduktions-Relais (optional)
        if (WEICHEN_PINS[i].hasRed && WEICHEN_PINS[i].pinRed != 255)
        {
            pinMode(WEICHEN_PINS[i].pinRed, OUTPUT);
            // LOW-Level-Relais: LOW = Reduktion aktiv (Abbiegen)
            digitalWrite(WEICHEN_PINS[i].pinRed, istGeradeNow ? HIGH : LOW);
        }
        
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
    
    // Pipelined selftest: eval-queue & pulse state reset
    selftestEvalQReset();
    m_stPulseIndex = 0;
    m_stPulseTargetGerade = false;

    m_stOk1Mask = m_stOk2Mask = m_stFailMask = 0;
}

bool WeichenHub::enqueueWeiche(uint8_t index, bool gerade)
{
    // Während Selftest keine normalen Commands annehmen (deterministisch bleiben)
    if (m_stActive) return false;
    if (index >= NUM_WEICHEN) return false;
    
    // --------------------------------------------------
    // Schutz gegen doppelte gleiche Zielaufträge
    // --------------------------------------------------
    // Fall A: identischer Auftrag ist gerade aktiv gepulst
    if (m_pulseActive &&
        m_activeCmd.index == index &&
        m_activeCmd.gerade == gerade)
    {
#ifdef DEBUG_SERIAL
        Serial.print(F("[M1WH] skip duplicate (active) W"));
        Serial.print(index);
        Serial.print(F(" -> "));
        Serial.println(gerade ? F("GERADE") : F("ABBIEGEN"));
#endif
        return true;
    }

    // Fall B: identischer Auftrag steht bereits in der Queue
    for (uint8_t i = 0; i < m_qCount; ++i)
    {
        const uint8_t pos = (m_qHead + i) % WEICHE_QUEUE_SIZE;
        if (m_q[pos].index == index && m_q[pos].gerade == gerade)
        {
#ifdef DEBUG_SERIAL
            Serial.print(F("[M1WH] skip duplicate (queued) W"));
            Serial.print(index);
            Serial.print(F(" -> "));
            Serial.println(gerade ? F("GERADE") : F("ABBIEGEN"));
#endif
            return true;
        }
    }

    if (m_qCount >= WEICHE_QUEUE_SIZE) return false;

#ifdef DEBUG_SERIAL
    Serial.print(F("[M1WH] enqueue W"));
    Serial.print(index);
    Serial.print(F(" -> "));
    Serial.println(gerade ? F("GERADE") : F("ABBIEGEN"));
#endif

    m_q[m_qTail] = Cmd{ index, gerade };
    m_qTail = (m_qTail + 1) % WEICHE_QUEUE_SIZE;
    m_qCount++;

    // Soll-Zustand merken
    m_state[index] = gerade;
    m_status[index].lastSollGerade = gerade;

    // Reduktions-Relais passend zum Sollzustand setzen (falls vorhanden)
    if (WEICHEN_PINS[index].hasRed && WEICHEN_PINS[index].pinRed != 255)
    {
        // LOW-Level-Relais: LOW = Reduktion aktiv (Abbiegen)
        digitalWrite(WEICHEN_PINS[index].pinRed, gerade ? HIGH : LOW);
        m_redActive[index] = (!gerade);
    }

    return true;
}

bool WeichenHub::enqueueGrundstellung()
{
    // Während Selftest NICHT mischen
    if (m_stActive) return false;

    bool ok = true;
    for (uint8_t w = 0; w < NUM_WEICHEN; ++w)
    {
        const bool gerade    = (WEICHEN_GRUNDSTELLUNG[w] == GERADE);
        const bool istGerade = readIstGerade(w);

#ifdef DEBUG_SERIAL
        Serial.print(F("[M1WH] Grundstellung W"));
        Serial.print(w);
        Serial.print(F(" ist="));
        Serial.print(istGerade ? F("GERADE") : F("ABBIEGEN"));
        Serial.print(F(" soll="));
        Serial.print(gerade ? F("GERADE") : F("ABBIEGEN"));
#endif

        // Bereits korrekt gestellt -> kein unnötiger Puls.
        if (istGerade == gerade)
        {
#ifdef DEBUG_SERIAL
            Serial.println(F(" -> skip"));
#endif
            m_state[w] = gerade;
            m_status[w].lastSollGerade = gerade;
            m_status[w].lastIstGerade  = istGerade;
            continue;
        }

#ifdef DEBUG_SERIAL
        Serial.println(F(" -> enqueue"));
#endif

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
 #ifdef DEBUG_SERIAL
    Serial.print(F("[M1WH] PULSE START W"));
    Serial.print(index);
    Serial.print(F(" coil="));
    Serial.print(gerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" t="));
    Serial.println(millis());
 #endif
    const WPins& p = WEICHEN_PINS[index];

    // Reduktions-Relais passend zum Zielzustand setzen (Abbiegen => LOW)
    if (p.hasRed && p.pinRed != 255)
    {
        // LOW-Level-Relais: LOW = Reduktion aktiv (Abbiegen)
        digitalWrite(p.pinRed, gerade ? HIGH : LOW);
        m_redActive[index] = (!gerade);
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

    // Reduktions-Relais bleibt im Zustand entsprechend IST/SOLL (nicht pulsen)
}

void WeichenHub::stopPulseAndCheck(const Cmd& cmd)
{
 #ifdef DEBUG_SERIAL
    Serial.print(F("[M1WH] PULSE STOP W"));
    Serial.print(cmd.index);
    Serial.print(F(" target="));
    Serial.println(cmd.gerade ? F("GERADE") : F("ABBIEGEN"));
 #endif
    const WPins& p = WEICHEN_PINS[cmd.index];

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Reduktions-Relais bleibt im Zustand entsprechend IST/SOLL (nicht pulsen)

    bool istGerade  = readIstGerade(cmd.index);
    bool sollGerade = cmd.gerade;

    // Reduktions-Relais auf IST synchronisieren (falls vorhanden)
    if (p.hasRed && p.pinRed != 255)
    {
        digitalWrite(p.pinRed, istGerade ? HIGH : LOW);
        m_redActive[cmd.index] = (!istGerade);
    }

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

    // pipelined selftest reset
    selftestEvalQReset();
    m_stPulseIndex = 0;
    m_stPulseTargetGerade = false;

    // Skip to first valid index
    (void)selftestPickNextIndex();

    Serial.println(F("[M1] Weichen selftest started"));
    return true;
}

bool WeichenHub::selftestPickNextIndex()
{
    while (m_stIndex < NUM_WEICHEN)
    {
        const uint16_t bit = (1u << m_stIndex);
        if ((m_stMask & bit) == 0)
        {
            m_stIndex++;
            continue;
        }

        // In Phase 1 nur Weichen testen, die Phase 0 bestanden haben
        if (m_stPhase == 1 && ((m_stFailMask & bit) != 0))
        {
            m_stIndex++;
            continue;
        }
        return true;
    }
    return false;
}

// --------------------------------------------------
// Selbsttest (pipelined) – Eval-Queue
// --------------------------------------------------
void WeichenHub::selftestEvalQReset()
{
    m_stEvalHead = m_stEvalTail = m_stEvalCount = 0;
}

bool WeichenHub::selftestEvalQPush(uint8_t index, bool targetGerade, uint32_t evalAtMs)
{
    if (m_stEvalCount >= NUM_WEICHEN) return false;
    m_stEvalQ[m_stEvalTail] = StEvalItem{ index, targetGerade, evalAtMs };
    m_stEvalTail = (m_stEvalTail + 1) % NUM_WEICHEN;
    m_stEvalCount++;
    return true;
}

bool WeichenHub::selftestEvalQPeek(StEvalItem& out) const
{
    if (m_stEvalCount == 0) return false;
    out = m_stEvalQ[m_stEvalHead];
    return true;
}

bool WeichenHub::selftestEvalQPop()
{
    if (m_stEvalCount == 0) return false;
    m_stEvalHead = (m_stEvalHead + 1) % NUM_WEICHEN;
    m_stEvalCount--;
    return true;
}

// --------------------------------------------------
// Selbsttest (pipelined) – Pulse/Eval Ablauf
// --------------------------------------------------
void WeichenHub::selftestStartNextPulse(uint32_t nowMs)
{
    // Zielrichtung = immer Gegenrichtung zur aktuellen Ist-RM
    const bool istGeradeNow = readIstGerade(m_stIndex);
    m_stPulseTargetGerade = !istGeradeNow;
    m_stPulseIndex = m_stIndex;

    // Puls starten (500ms)
    startPulseCustom(m_stPulseIndex, m_stPulseTargetGerade, WEICHE_SELFTEST_PULSE_MS);
    // startPulseCustom setzt m_pulseActive/m_pulseUntilMs/m_activeCmd

    Serial.print(F("[M1] ST pulse ON  W"));
    Serial.print(m_stPulseIndex);
    Serial.print(F(" -> "));
    Serial.print(m_stPulseTargetGerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" (ist="));
    Serial.print(istGeradeNow ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" phase="));
    Serial.print(m_stPhase);
    Serial.println(F(")"));

    // Nächste Weiche für den nächsten Puls vorbereiten (Settle läuft parallel)
    m_stIndex++;
    (void)selftestPickNextIndex();
}

void WeichenHub::selftestPulseFinished(uint32_t nowMs)
{
    // Puls beenden (Spulen AUS)
    stopPulseOnly(m_stPulseIndex);
    m_pulseActive = false;

    Serial.print(F("[M1] ST pulse OFF W"));
    Serial.print(m_stPulseIndex);
    Serial.println(F(" -> queued for settle/eval"));

    // Eval nach Settle-Zeit in Queue einhängen
    (void)selftestEvalQPush(m_stPulseIndex, m_stPulseTargetGerade, nowMs + WEICHE_SELFTEST_SETTLE_MS);
}

void WeichenHub::selftestEvalDue(uint32_t nowMs)
{
    // Es können mehrere Evals in einem Loop fällig sein
    StEvalItem it{};
    while (selftestEvalQPeek(it))
    {
        if ((int32_t)(nowMs - it.evalAtMs) < 0)
            break;

        const bool istGerade = readIstGerade(it.index);

        // Reduktions-Relais auf IST synchronisieren (falls vorhanden)
        const WPins& p = WEICHEN_PINS[it.index];
        if (p.hasRed && p.pinRed != 255)
        {
            digitalWrite(p.pinRed, istGerade ? HIGH : LOW);
            m_redActive[it.index] = (!istGerade);
        }

        const bool ok = (istGerade == it.targetGerade);
        const uint16_t bit = (1u << it.index);

        if (m_stPhase == 0)
        {
            if (ok) m_stOk1Mask |= bit;
            else    m_stFailMask |= bit;
        }
        else
        {
            if (ok) m_stOk2Mask |= bit;
            else    m_stFailMask |= bit;
        }

        Serial.print(F("[M1] ST eval W"));
        Serial.print(it.index);
        Serial.print(F(" phase="));
        Serial.print(m_stPhase);
        Serial.print(F(" target="));
        Serial.print(it.targetGerade ? F("GERADE") : F("ABBIEGEN"));
        Serial.print(F(" ist="));
        Serial.print(istGerade ? F("GERADE") : F("ABBIEGEN"));
        Serial.print(F(" ok="));
        Serial.println(ok ? F("1") : F("0"));

        (void)selftestEvalQPop();
    }
}

void WeichenHub::selftestUpdate(uint32_t nowMs)
{
    if (!m_stActive) return;

    // 1) Puls beenden, wenn fällig
    if (m_pulseActive)
    {
        if ((int32_t)(nowMs - m_pulseUntilMs) >= 0)
            selftestPulseFinished(nowMs);
    }

    // 2) Fällige Evals abarbeiten (läuft parallel zu Puls anderer Weichen)
    selftestEvalDue(nowMs);

    // 3) Wenn kein Puls aktiv: nächsten Puls starten, oder Phase wechseln, oder fertig
    if (!m_pulseActive)
    {
        // Gibt es noch Weichen zu pulsen in dieser Phase?
        if (m_stIndex < NUM_WEICHEN)
        {
            (void)selftestPickNextIndex();
            if (m_stIndex < NUM_WEICHEN)
            {
                selftestStartNextPulse(nowMs);
                return;
            }
        }

        // Keine weiteren Pulse in dieser Phase. Erst fertig, wenn alle Evals abgearbeitet.
        if (m_stEvalCount > 0)
            return;

        // Phase 0 -> Phase 1
        if (m_stPhase == 0)
        {
            m_stPhase = 1;
            m_stIndex = 0;
            selftestEvalQReset();
            (void)selftestPickNextIndex();
            Serial.println(F("[M1] ST phase 0 done -> phase 1"));
            return;
        }

        // Phase 1 fertig -> Gesamttest fertig
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

void WeichenHub::pollRueckmelders(uint32_t now)
{
    // Nicht während Selftest oder Spulenpuls live nachziehen:
    // dort werden IST-Werte gezielt an definierten Punkten gelesen.
    if (m_stActive || m_pulseActive) return;

    // 20ms Poll reicht völlig, entprellt quasi nebenbei
    if ((uint32_t)(now - m_lastRueckPollMs) < 20) return;
    m_lastRueckPollMs = now;

    // Rohwerte sammeln: HIGH=1, LOW=0
    uint16_t rawBits = 0;


    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        const auto& p = WEICHEN_PINS[i];
        const bool high = (digitalRead(p.pinRueck) == HIGH); // HIGH=GERADE (pullup), LOW=ABBIEGEN
        if (high) rawBits |= (1U << i);

        const bool istNow = high;

        if (istNow != m_status[i].lastIstGerade)
        {
            m_status[i].lastIstGerade = istNow;

            // Reduktions-Relais folgt IST, aber nur falls vorhanden
            const bool hasRed = (p.hasRed && p.pinRed != 255);
            if (hasRed)
            {
                // LOW-Level-Relais: LOW = Reduktion aktiv (Abbiegen)
                digitalWrite(p.pinRed, istNow ? HIGH : LOW);
                m_redActive[i] = (!istNow);
            }
            else
            {
                m_redActive[i] = false;
            }
        }
    }

    
    // -------------------------------------------------
    // Rueckmelder Debug: Log bei Aenderung + periodisch
    // -------------------------------------------------
    const bool changed  = (rawBits != m_lastRueckRawBits);
    const bool periodic = ((uint32_t)(now - m_lastRueckLogMs) >= 1000);

    if (changed || periodic)
    {
        m_lastRueckLogMs = now;
        const uint16_t diff = rawBits ^ m_lastRueckRawBits;

        Serial.print(F("[RM] t="));
        Serial.print(now);
        Serial.print(F(" raw=0x"));
        Serial.print(rawBits, HEX);

        if (changed)
        {
            Serial.print(F(" diff=0x"));
            Serial.print(diff, HEX);
            Serial.print(F(" {"));

            for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
            {
                if ((diff >> i) & 1U)
                {
                    const bool h = ((rawBits >> i) & 1U) != 0;
                    Serial.print(F(" W"));
                    Serial.print(i);
                    Serial.print(F("="));
                    Serial.print(h ? F("H") : F("L"));
                    Serial.print(F("("));
                    Serial.print(h ? F("G") : F("A"));
                    Serial.print(F(")"));
                }
            }
            Serial.print(F(" }"));
        }

        Serial.println();
        m_lastRueckRawBits = rawBits;
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

uint16_t WeichenHub::buildWeichenSlowSelectedBits() const
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
