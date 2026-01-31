#include "SensorHub.h"

void SensorHub::begin()
{
    m_stateMask   = 0;
    m_changedMask = 0;

    for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    {
        m_db[i] = DebounceState{};
    }
}

void SensorHub::update()
{
    m_changedMask = 0;
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    {
        const int8_t pin = SENSOR_PINS[i];
        if (pin < 0) {
            // Unbenutzter Sensor-Slot -> garantiert keine Aktivität, kein Change
            // (wichtig: kein digitalRead(-1)!)
            if (m_db[i].trainPresent) {
                m_db[i].trainPresent = false;
            }
            continue;
        }

        // LOW = Sensor aktiv
        bool rawActive = (digitalRead((uint8_t)pin) == LOW);

        // Sperre ggf. aufheben
        if (m_db[i].blocked && (int32_t)(now - m_db[i].unblockAtMs) >= 0)
        {
            m_db[i].blocked = false;
        }

        // ENTRY: erste Achse (HIGH -> LOW)
        if (rawActive && !m_db[i].trainPresent)
        {
            m_db[i].trainPresent = true;

            if (!m_db[i].blocked)
            {
                m_stateMask   |= (1UL << i);
                m_changedMask |= (1UL << i);

                m_db[i].blocked     = true;
                m_db[i].unblockAtMs = now + SENSOR_TRAIN_DEBOUNCE_MS;
            }
        }

        // EXIT: letzte Achse (LOW -> HIGH), NO trigger
        if (!rawActive && m_db[i].trainPresent)
        {
            m_db[i].trainPresent = false;
            m_stateMask &= ~(1UL << i);
        }
    }
}

bool SensorHub::isActive(uint8_t index) const
{
    if (index >= NUM_SENSORS) return false;
    return (m_stateMask & (1UL << index)) != 0;
}

uint32_t SensorHub::changedMask() const
{
    return m_changedMask;
}
uint16_t SensorHub::buildKontaktBits() const
{
    // Kontaktbits für S0..S10 (Bit 0 = S0, ...)
    uint16_t bits = 0;

    const uint8_t maxBits = 11; // S0..S10
    for (uint8_t i = 0; i < maxBits && i < NUM_SENSORS; ++i)
    {
        if (isActive(i))
            bits |= (1U << i);
    }
    return bits;
}