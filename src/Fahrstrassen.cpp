#include "Fahrstrassen.h"
#include "payload.h"
#include "BetriebsstellenConfig.h"
#include "pins_mega1.h"

#ifndef FS_DEBUG_FS0_ONLY
#define FS_DEBUG_FS0_ONLY 0
#endif

 #ifdef FS_DEBUG
static bool shouldLogFs(uint8_t fs)
{
#if FS_DEBUG_FS0_ONLY
    return fs == 0;
#else
    (void)fs;
    return true;
#endif
}


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
        case SENSOR_S18: return "S18";
        default:         return "S?";
    }
}

static void logFsReset(uint8_t fs, const FahrstrassenConfig& def, uint8_t sensorIdx)
{
    if (!shouldLogFs(fs)) return;
    Serial.print(F("[FS] reset fs=")); Serial.print(fs);
    Serial.print(F(" name=\"")); Serial.print(def.name); Serial.print(F("\""));
    Serial.print(F(" sensor=")); Serial.println(sensorIndexToName(sensorIdx));
}

static void logFsTrigger(uint8_t fs, const FahrstrassenConfig& def, uint8_t sensorIdx, uint8_t counter)
{
    if (!shouldLogFs(fs)) return;
    Serial.print(F("[FS] trigger fs=")); Serial.print(fs);
    Serial.print(F(" name=\"")); Serial.print(def.name); Serial.print(F("\""));
    Serial.print(F(" sensor=")); Serial.print(sensorIndexToName(sensorIdx));
    Serial.print(F(" count=")); Serial.println(counter);
}

static void logFsApplyStep(uint8_t fs, const FahrstrassenConfig& def, uint8_t weiche, bool gerade, uint8_t counter)
{
    if (!shouldLogFs(fs)) return;
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
void Fahrstrassen::setCounter(uint8_t fs, uint8_t value)
{
    if (fs >= NUM_STW_FS) return;
    m_fsState[fs].counter = value;
    m_fsState[fs].active  = (value != 0);
    g_payload.fsCounter[fs] = value;
    if (value == 0 && m_activeRoute == (int8_t)fs)
        m_activeRoute = -1;
    g_payloadDirty = true;
}

// --------------------------------------------------
void Fahrstrassen::resetAllCounters()
{
    for (uint8_t fs = 0; fs < NUM_STW_FS; ++fs)
        setCounter(fs, 0);
    m_activeRoute = -1;
    g_payload.activeRoute = -1;
    g_payloadDirty = true;
}

// --------------------------------------------------
void Fahrstrassen::applySteps(uint8_t fs, WeichenHub& weichenHub)
{
    const FahrstrassenConfig& def = FAHRSTRASSEN_CONFIG[fs];
    uint8_t counter = m_fsState[fs].counter;

    // --------------------------------------------------
    // FS1: Trigger S2, alternierend odd/even
    // odd  count: W5 A, danach W1 A, W2 G, W3 G, W6 G, W7 G, W8 G
    //              (W5 muss hier fachlich zwingend als erste Weiche geschaltet werden)
    // even count: W5 G
    //
    // Hinweis:
    // Das vorhandene minCount-Modell ist monoton (counter >= minCount)
    // und kann echtes Odd/Even-Umschalten nicht ausdrücken.
    // Daher hier bewusst als kleine fachliche Sonderlogik.
    // --------------------------------------------------
    if (fs == 1)
    {
        const bool odd = (counter & 0x01u) != 0;

        if (odd)
        {
#ifdef FS_DEBUG
            logFsApplyStep(fs, def, 5, false, counter); // W5 ABBIEGEN (muss zuerst)
            logFsApplyStep(fs, def, 1, false, counter); // W1 ABBIEGEN
            logFsApplyStep(fs, def, 2, true,  counter); // W2 GERADE
            logFsApplyStep(fs, def, 3, true,  counter); // W3 GERADE
            logFsApplyStep(fs, def, 6, true,  counter); // W6 GERADE
            logFsApplyStep(fs, def, 7, true,  counter); // W7 GERADE
            logFsApplyStep(fs, def, 8, true,  counter); // W8 GERADE
#endif
            // Wichtig: W5 muss bei S2-getriggerter FS1 als erster Befehl in die Queue,
            // damit ihre Umschaltung vor allen nachfolgenden Weichen angestoßen wird.
            weichenHub.enqueueWeiche(5, false);
            weichenHub.enqueueWeiche(1, false);
            weichenHub.enqueueWeiche(2, true);
            weichenHub.enqueueWeiche(3, true);
            weichenHub.enqueueWeiche(6, true);
            weichenHub.enqueueWeiche(7, true);
            weichenHub.enqueueWeiche(8, true);
        }
        else
        {
#ifdef FS_DEBUG
            logFsApplyStep(fs, def, 5, true, counter); // W5 GERADE
#endif
            weichenHub.enqueueWeiche(5, true);
        }
        return;
    }

    // --------------------------------------------------
    // FS0: Zusatzregel
    // Bei jeder Überfahrt S0 prüfen:
    // Wenn W0 IST gerade ist, dann W1 gerade schalten.
    // --------------------------------------------------
    if (fs == 0)
    {
        if (weichenHub.lastIstGerade(0))
        {
#ifdef FS_DEBUG
            logFsApplyStep(fs, def, 1, true, counter); // W1 GERADE
#endif
            weichenHub.enqueueWeiche(1, true);
        }
    }
    
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
        const FahrstrassenConfig& def = FAHRSTRASSEN_CONFIG[fs];

        // ---------------- Reset-Sensoren ----------------
        for (uint8_t r = 0; r < def.numReset; ++r)
        {
            uint8_t rIdx = def.resetSensors[r];
            if ((changed & (1UL << rIdx)) && hub.isActive(rIdx))
            {
                setCounter(fs, 0);
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