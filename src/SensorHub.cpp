#include "SensorHub.h"

void SensorHub::begin()
{
    m_stateMask   = 0;
    m_changedMask = 0;
    m_riseMask    = 0;
    m_fallMask    = 0;

    for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    {
        m_db[i] = DebounceState{};
        // rawInit bleibt false -> Baseline wird im ersten update() gesetzt
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
        const bool rawActive = (digitalRead((uint8_t)pin) == LOW);

        // ---- Baseline / Glitch-Filter (Rohwert muss stabil sein) ----
        // 1) Beim allerersten Sample: Baseline setzen, KEINE Flanken auslösen.
        if (!m_db[i].rawInit)
        {
            m_db[i].rawInit    = true;
            m_db[i].rawLast    = rawActive;
            m_db[i].rawSinceMs = now;

            // Baseline als "aktueller Zustand" übernehmen
            m_db[i].trainPresent = rawActive;
            if (rawActive) m_stateMask |= (1UL << i);
            else          m_stateMask &= ~(1UL << i);
            continue;
        }

        // 2) Wenn Rohwert kippt: Timer neu starten und erst nach Stabilität auswerten
        if (rawActive != m_db[i].rawLast)
        {
            m_db[i].rawLast    = rawActive;
            m_db[i].rawSinceMs = now;
            continue;
        }
        // 3) Noch nicht lange genug stabil? dann ignorieren
        if ((uint32_t)(now - m_db[i].rawSinceMs) < SENSOR_GLITCH_MS)
        {
            continue;
        }

        // Ab hier gilt: rawStable ist ein “sauberer” Rohwert
        const bool rawStableActive = m_db[i].rawLast;

        // Sperre ggf. aufheben
        if (m_db[i].blocked && (int32_t)(now - m_db[i].unblockAtMs) >= 0)
        {
            m_db[i].blocked = false;
        }

        // ENTRY: erste Achse (HIGH -> LOW)
        if (rawStableActive && !m_db[i].trainPresent)
        {
            m_db[i].trainPresent = true;

            if (!m_db[i].blocked)
            {
                m_stateMask   |= (1UL << i);
                m_changedMask |= (1UL << i);
                m_riseMask    |= (1UL << i); // logical rise: inactive -> active

                m_db[i].blocked     = true;
                m_db[i].unblockAtMs = now + SENSOR_TRAIN_DEBOUNCE_MS;
            }
        }

        // EXIT: letzte Achse (LOW -> HIGH), NO trigger
        if (!rawStableActive && m_db[i].trainPresent)
        {
            m_db[i].trainPresent = false;
            m_stateMask &= ~(1UL << i);
            m_fallMask  |= (1UL << i); // logical fall: active -> inactive
        }
    }
}

bool SensorHub::isActive(uint8_t index) const
{
    if (index >= NUM_SENSORS) return false;
    return (m_stateMask & (1UL << index)) != 0;
}

 
uint32_t SensorHub::activeMask() const
{
    return m_stateMask;
}

uint32_t SensorHub::riseMask() const
{
    return m_riseMask;
}

uint32_t SensorHub::fallMask() const
{
    return m_fallMask;
}

void SensorHub::clearEdgeMasks()
{
    m_riseMask = 0;
    m_fallMask = 0;
    
    // Keep pending-change detector in sync: clearing edges must NOT trigger a new DIAG pending.
    m_lastRiseForPending = 0;
    m_lastFallForPending = 0;
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

bool SensorHub::consumeDiagDirty()
{
    const uint32_t a = m_stateMask;
    const uint32_t r = m_riseMask;
    const uint32_t f = m_fallMask;

    const bool changed =
        (a != m_lastActiveForPending) ||
        (r != m_lastRiseForPending) ||
        (f != m_lastFallForPending);

    if (changed)
    {
        m_lastActiveForPending = a;
        m_lastRiseForPending   = r;
        m_lastFallForPending   = f;
    }
    return changed;
}