#include "Fahrstrassen.h"
#include "payload.h"
#include "Fahrstrassen_defs.h"



// --------------------------------------------------
void Fahrstrassen::begin()
{
    for (uint8_t i = 0; i < NUM_STW_FS; ++i)
    {
        m_fsState[i].counter = 0;
        m_fsState[i].active  = false;
        g_payload.fsCounter[i] = 0;
    }
    m_activeRoute = -1;
    g_payloadDirty = true;
}

// --------------------------------------------------
uint8_t Fahrstrassen::getCounter(uint8_t fs) const
{
    if (fs >= NUM_STW_FS) return 0;
    return m_fsState[fs].counter;
}

// --------------------------------------------------
void Fahrstrassen::applySteps(uint8_t fs, WeichenHub& weichenHub)
{
    const SteuerungWeichenDefinition& def = STW_DEFS[fs];
    uint8_t counter = m_fsState[fs].counter;

    bool hasCmd[NUM_WEICHEN]   = {false};
    bool finalState[NUM_WEICHEN] = {false};

    for (uint8_t i = 0; i < def.numSteps; ++i)
    {
        const WeichenSchaltSchritt& step = def.steps[i];
        if (counter >= step.minCount)
        {
            hasCmd[step.weichenIndex] = true;
            finalState[step.weichenIndex] = (step.richtung == GERADE);
        }
    }

    for (uint8_t w = 0; w < NUM_WEICHEN; ++w)
    {
        if (hasCmd[w])
            weichenHub.enqueueWeiche(w, finalState[w]);
    }
}

// --------------------------------------------------
void Fahrstrassen::handleSensorEvents(const SensorHub& hub,
                                      WeichenHub& weichenHub)
{
    uint32_t changed = hub.changedMask();

    for (uint8_t fs = 0; fs < NUM_STW_FS; ++fs)
    {
        const SteuerungWeichenDefinition& def = STW_DEFS[fs];

        // ---------------- Reset-Sensoren ----------------
        for (uint8_t r = 0; r < def.numReset; ++r)
        {
            uint8_t rIdx = def.resetSensors[r];
            if ((changed & (1UL << rIdx)) && hub.isActive(rIdx))
            {
                m_fsState[fs].counter = 0;
                m_fsState[fs].active  = false;

                g_payload.fsCounter[fs] = 0;
                g_payload.lastResetSensor = rIdx;
                g_payloadDirty = true;

                if (m_activeRoute == (int8_t)fs)
                    m_activeRoute = -1;

                continue;
            }
        }

        // ---------------- Trigger-Sensor ----------------
        const uint8_t sIdx = def.sensorIndex;
        const uint32_t mask = (1UL << sIdx);

        if ((changed & mask) && hub.isActive(sIdx))
        {
            FSState& st = m_fsState[fs];
            st.counter++;
            st.active = true;

            m_activeRoute = fs;

            g_payload.fsCounter[fs]     = st.counter;
            g_payload.lastSensorTriggered = sIdx;
            g_payload.lastFsTriggered     = fs;
            g_payload.activeRoute         = fs;
            g_payloadDirty = true;

            applySteps(fs, weichenHub);
        }
    }
}
