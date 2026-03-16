#include "Fahrstrassen.h"
#include "payload.h"
#include "Fahrstrassen_defs.h"
#include "pins_mega1.h"

#ifdef FS_DEBUG
static const char* sensorIndexToName(uint8_t idx)
{
    switch (idx)
    {
        case SENSOR_S0:  return "S0";
        case SENSOR_S1:  return "S1";
        case SENSOR_S2:  return "S2";
        case SENSOR_S3:  return "S3";
        case SENSOR_S4:  return "S4";
        case SENSOR_S5:  return "S5";
        case SENSOR_S6:  return "S6";
        case SENSOR_S7:  return "S7";
        case SENSOR_S8:  return "S8";
        case SENSOR_S9:  return "S9";
        case SENSOR_S10: return "S10";
        default:         return "S?";
    }
}

static void logFsReset(uint8_t fs, const SteuerungWeichenDefinition& def, uint8_t sensorIdx)
{
    Serial.print(F("[FS] reset fs=")); Serial.print(fs);
    Serial.print(F(" name=\"")); Serial.print(def.name); Serial.print(F("\""));
    Serial.print(F(" sensor=")); Serial.println(sensorIndexToName(sensorIdx));
}

static void logFsTrigger(uint8_t fs, const SteuerungWeichenDefinition& def, uint8_t sensorIdx, uint8_t counter)
{
    Serial.print(F("[FS] trigger fs=")); Serial.print(fs);
    Serial.print(F(" name=\"")); Serial.print(def.name); Serial.print(F("\""));
    Serial.print(F(" sensor=")); Serial.print(sensorIndexToName(sensorIdx));
    Serial.print(F(" count=")); Serial.println(counter);
}

static void logFsApplyStep(uint8_t fs, const SteuerungWeichenDefinition& def, uint8_t weiche, bool gerade, uint8_t counter)
{
    Serial.print(F("[FS] apply fs=")); Serial.print(fs);
    Serial.print(F(" name=\"")); Serial.print(def.name); Serial.print(F("\""));
    Serial.print(F(" weiche=W")); Serial.print(weiche);
    Serial.print(F(" richtung=")); Serial.print(gerade ? F("GERADE") : F("ABBIEGEN"));
    Serial.print(F(" count=")); Serial.println(counter);
}
#endif

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

    bool hasCmd[NUM_WEICHEN]     = {false};
    bool finalState[NUM_WEICHEN] = {false};

    for (uint8_t i = 0; i < def.numSteps; ++i)
    {
        const WeichenSchaltSchritt& step = def.steps[i];

        if (counter >= step.minCount) {
            hasCmd[step.weichenIndex] = true;
            finalState[step.weichenIndex] = (step.richtung == GERADE);
        }
    }

    for (uint8_t w = 0; w < NUM_WEICHEN; ++w)
    {
        if (hasCmd[w])
        {
#ifdef FS_DEBUG
            logFsApplyStep(fs, def, w, finalState[w], counter);
#endif
            weichenHub.enqueueWeiche(w, finalState[w]);
        }
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

#ifdef FS_DEBUG
                logFsReset(fs, def, rIdx);
#endif
                continue;
            }
        }

        // ---------------- Trigger-Sensor ----------------
        const uint8_t sIdx = def.triggerSensor;
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

#ifdef FS_DEBUG
            logFsTrigger(fs, def, sIdx, st.counter);
#endif
            applySteps(fs, weichenHub);
        }
    }
}