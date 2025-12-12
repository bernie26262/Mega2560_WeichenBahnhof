#include "WeichenHub.h"

void WeichenHub::begin()
{
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        m_state[i] = false;
        m_cooldownUntil[i] = 0;

        m_status[i] = WeichenStatus{};

        // LOW-Level-Relais: AUS = HIGH
        pinMode(WEICHEN_PINS[i].pinG, OUTPUT);
        pinMode(WEICHEN_PINS[i].pinA, OUTPUT);
        digitalWrite(WEICHEN_PINS[i].pinG, HIGH);
        digitalWrite(WEICHEN_PINS[i].pinA, HIGH);

        // Reduktion (optional)
        if (WEICHEN_PINS[i].hasRed && WEICHEN_PINS[i].pinRed != 255)
        {
            pinMode(WEICHEN_PINS[i].pinRed, OUTPUT);
            digitalWrite(WEICHEN_PINS[i].pinRed, HIGH);
        }

        // Rückmelder
        pinMode(WEICHEN_PINS[i].pinRueck, INPUT_PULLUP);
    }

    m_qHead = m_qTail = m_qCount = 0;
    m_pulseActive = false;
}

bool WeichenHub::enqueueWeiche(uint8_t index, bool gerade)
{
    if (index >= NUM_WEICHEN) return false;
    if (m_qCount >= WEICHE_QUEUE_SIZE) return false;

    m_q[m_qTail] = Cmd{ index, gerade };
    m_qTail = (m_qTail + 1) % WEICHE_QUEUE_SIZE;
    m_qCount++;

    // Soll-Zustand merken (für Payload)
    m_state[index] = gerade;
    m_status[index].lastSollGerade = gerade;

    return true;
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

void WeichenHub::startPulse(const Cmd& cmd)
{
    const WPins& p = WEICHEN_PINS[cmd.index];

    // Reduktion EIN (LOW)
    if (p.hasRed && p.pinRed != 255)
        digitalWrite(p.pinRed, LOW);

    // Sicher: beide Spulen AUS (HIGH)
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Gewünschte Spule EIN (LOW)
    digitalWrite(cmd.gerade ? p.pinG : p.pinA, LOW);

    m_pulseActive  = true;
    m_activeCmd    = cmd;
    m_pulseUntilMs = millis() + WEICHE_PULSE_MS;
}

void WeichenHub::stopPulseAndCheck(const Cmd& cmd)
{
    const WPins& p = WEICHEN_PINS[cmd.index];

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Reduktion AUS
    if (p.hasRed && p.pinRed != 255)
        digitalWrite(p.pinRed, HIGH);

    // --------------------------------------------------
    // Rückmeldeprüfung (NACH Impulsende!)
    // --------------------------------------------------
    bool istGerade  = readIstGerade(cmd.index);
    bool sollGerade = cmd.gerade;

    WeichenStatus& st = m_status[cmd.index];
    st.lastIstGerade  = istGerade;
    st.lastSollGerade = sollGerade;
    st.lastCheckOk    = (istGerade == sollGerade);
    st.everChecked    = true;

    // Payload-Logging
    g_payload.lastWeiche         = cmd.index;
    g_payload.lastWeicheStellung = istGerade ? 1 : 0;

    // Fehlerflag setzen, wenn Soll != Ist
    // (Bit 0 reservieren wir hier für "Weichenfehler")
    if (!st.lastCheckOk)
        g_payload.errorFlags |= 0x01;

    g_payloadDirty = true;

    // Cooldown starten
    m_cooldownUntil[cmd.index] = millis() + WEICHE_COOLDOWN_MS;
    m_pulseActive = false;
}

void WeichenHub::update()
{
    const uint32_t now = millis();

    // Läuft gerade ein Impuls?
    if (m_pulseActive)
    {
        if ((int32_t)(now - m_pulseUntilMs) >= 0)
            stopPulseAndCheck(m_activeCmd);
        return;
    }

    // Nächsten ausführbaren Auftrag suchen
    uint8_t tries = m_qCount;
    while (tries--)
    {
        Cmd cmd;
        if (!pop(cmd)) return;

        // Cooldown abgelaufen?
        if ((int32_t)(now - m_cooldownUntil[cmd.index]) >= 0)
        {
            startPulse(cmd);
            return;
        }

        // Noch im Cooldown -> hinten wieder anstellen
        m_q[m_qTail] = cmd;
        m_qTail = (m_qTail + 1) % WEICHE_QUEUE_SIZE;
        m_qCount++;
    }
}

bool WeichenHub::getWeiche(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    return m_state[index];
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
    {
        // wenn noch nie geprüft: einfach aktuellen Pin lesen
        bool ist = m_status[i].everChecked ? m_status[i].lastIstGerade : readIstGerade(i);
        if (ist) bits |= (1U << i);
    }
    return bits;
}

uint16_t WeichenHub::buildWeichenOkBits() const
{
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        // noch nie geprüft => "ok" setzen, damit UI nicht rot startet
        bool ok = m_status[i].everChecked ? m_status[i].lastCheckOk : true;
        if (ok) bits |= (1U << i);
    }
    return bits;
}

bool WeichenHub::lastCheckOk(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return true;
    return m_status[index].everChecked ? m_status[index].lastCheckOk : true;
}

bool WeichenHub::lastIstGerade(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    return m_status[index].everChecked ? m_status[index].lastIstGerade : readIstGerade(index);
}
