#include "WeichenHub.h"

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
}

bool WeichenHub::enqueueWeiche(uint8_t index, bool gerade)
{
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

    // Reduktion EIN
    if (p.hasRed && p.pinRed != 255)
    {
        digitalWrite(p.pinRed, LOW);
        m_redActive[cmd.index] = true;
    }

    // Spulen AUS
    digitalWrite(p.pinG, HIGH);
    digitalWrite(p.pinA, HIGH);

    // Gewünschte Spule EIN
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

void WeichenHub::update()
{
    const uint32_t now = millis();

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

bool WeichenHub::lastIstGerade(uint8_t index) const
{
    if (index >= NUM_WEICHEN) return false;
    return m_status[index].everChecked
        ? m_status[index].lastIstGerade
        : readIstGerade(index);
}
