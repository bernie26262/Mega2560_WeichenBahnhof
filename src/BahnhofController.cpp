#include "BahnhofController.h"
#include "pins_mega1.h"

void BahnhofController::begin()
{
    // Alle Bahnhöfe initialisieren
    for (uint8_t i = 0; i < BHF_COUNT; ++i)
    {
        m_bhf[i] = BahnhofState{};
        m_bhf[i].powerOn       = true;   // zu Beginn Strom an
        m_bhf[i].timerRunning  = false;
        m_bhf[i].timerDuration = BAHNHOF_CONFIG[i].timerDurationMs;
    }
    m_lastEventBhf = 255;
    m_errorFlags   = 0;
}

void BahnhofController::update(const SensorHub& hub)
{
    uint32_t changed = hub.changedMask();

    if (changed != 0)
    {
        handleEntryResets(hub, changed);
        handleEinfahrten(hub, changed);
        handleTimerStarts(hub, changed);
    }

    handleTimers();
}

void BahnhofController::handleEntryResets(const SensorHub& hub, uint32_t changed)
{
    for (uint8_t bhf = 0; bhf < BHF_COUNT; ++bhf)
    {
        BahnhofState& st = m_bhf[bhf];
        const BahnhofConfig& cfg = BAHNHOF_CONFIG[bhf];

        for (uint8_t i = 0; i < cfg.numResetSensors; ++i)
        {
            const uint8_t sIdx = cfg.resetSensors[i];
            if (sIdx >= 32) continue;

            const uint32_t mask = (1UL << sIdx);
            if ((changed & mask) && hub.isActive(sIdx))
            {
                st.entryLatched = false;

#ifdef BHF_DEBUG
                BHF_LOG(F("[BHF] entryReset bhf=")); BHF_LOG(bhf);
                BHF_LOG(F(" sensor=S")); BHF_LOG(sIdx);
                BHF_LOGLN(F(" entryLatched=0"));
#endif

                m_lastEventBhf = bhf;
                break;
            }
        }
    }
}

void BahnhofController::handleEinfahrten(const SensorHub& hub, uint32_t changed)
{
    // Bei Einfahrt über S2: Bhf0 & Bhf1 → Strom aus, occupied = true
    // Bei Einfahrt über S8: Bhf2 & Bhf3 → Strom aus, occupied = true

   for (uint8_t bhf = 0; bhf < BHF_COUNT; ++bhf)
    {
        uint8_t sIdx = BAHNHOF_CONFIG[bhf].einfahrtSensorIndex;
        if (sIdx >= 32) continue;

        uint32_t mask = (1UL << sIdx);

        if ((changed & mask) && hub.isActive(sIdx))
        {
            BahnhofState& st = m_bhf[bhf];
            if (st.entryLatched)
            {
#ifdef BHF_DEBUG
                BHF_LOG(F("[BHF] einfahrt ignored bhf=")); BHF_LOG(bhf);
                BHF_LOG(F(" sensor=S")); BHF_LOG(sIdx);
                BHF_LOGLN(F(" reason=entryLatched"));
#endif
                continue;
            }

            st.occupied = true;
            st.entryLatched = true;
            st.powerOn  = false;      // Stromgleis AUS bei Einfahrt
            if (m_powerHub) {
                m_powerHub->setPower(bhf, false);
            }

#ifdef BHF_DEBUG
            BHF_LOG(F("[BHF] einfahrt bhf=")); BHF_LOG(bhf);
            BHF_LOG(F(" sensor=S")); BHF_LOG(sIdx);
            BHF_LOG(F(" powerOn=")); BHF_LOG(st.powerOn ? F("1") : F("0"));
            BHF_LOG(F(" occupied=")); BHF_LOGLN(st.occupied ? F("1") : F("0"));
#endif

            m_lastEventBhf = bhf;
        }
    }
}

void BahnhofController::handleTimerStarts(const SensorHub& hub, uint32_t changed)
{
    uint32_t now = millis();

    for (uint8_t bhf = 0; bhf < BHF_COUNT; ++bhf)
    {
        uint8_t sIdx = BAHNHOF_CONFIG[bhf].timerStartSensorIndex;
        if (sIdx >= 32) continue;

        uint32_t mask = (1UL << sIdx);

        if ((changed & mask) && hub.isActive(sIdx))
        {
            BahnhofState& st = m_bhf[bhf];

            st.timerRunning = true;
            st.timerStartMs = now;
            // Timerstart darf bei jeder Überfahrt neu gesetzt werden.

#ifdef BHF_DEBUG
            BHF_LOG(F("[BHF] timerStart bhf=")); BHF_LOG(bhf);
            BHF_LOG(F(" sensor=S")); BHF_LOG(sIdx);
            BHF_LOG(F(" durationMs=")); BHF_LOG(st.timerDuration);
            BHF_LOGLN(F(" restart=1"));
#endif

            m_lastEventBhf = bhf;
        }
    }
}

void BahnhofController::handleTimers()
{
    uint32_t now = millis();

    for (uint8_t bhf = 0; bhf < BHF_COUNT; ++bhf)
    {
        BahnhofState& st = m_bhf[bhf];

        if (st.timerRunning)
        {
            if ((now - st.timerStartMs) >= st.timerDuration)
           {
                // Timer ist abgelaufen:
                st.timerRunning = false;
                st.powerOn      = true;   // Stromgleis wieder AN
                if (m_powerHub) {
                    m_powerHub->setPower(bhf, true);
                }
                // entryLatched bleibt bewusst unverändert; Reset erfolgt über Sensorsignal.

#ifdef BHF_DEBUG
                BHF_LOG(F("[BHF] timerDone bhf=")); BHF_LOG(bhf);
                BHF_LOG(F(" durationMs=")); BHF_LOG(st.timerDuration);
                BHF_LOG(F(" powerOn=")); BHF_LOGLN(st.powerOn ? F("1") : F("0"));
#endif

                m_lastEventBhf = bhf;

               // Hinweis: occupied bleibt TRUE, d.h. Zug steht (oder stand) dort.
                // Freigabelogik (Block/Fahrstraße) kann später ergänzt werden.
            }
        }
    }
}

void BahnhofController::manualRelease(uint8_t bhf)
{
    if (bhf >= BHF_COUNT) return;
    BahnhofState& st = m_bhf[bhf];

    st.powerOn      = true;
    st.timerRunning = false;
    st.entryLatched = false;
    if (m_powerHub) {
        m_powerHub->setPower(bhf, true);
    }

#ifdef BHF_DEBUG
    BHF_LOG(F("[BHF] manualRelease bhf=")); BHF_LOG(bhf);
    BHF_LOG(F(" powerOn=")); BHF_LOGLN(st.powerOn ? F("1") : F("0"));
#endif

    m_lastEventBhf = bhf;

    // occupied bleibt unangetastet – kann über separate Logik/Fahrstraße
    // oder späteren Ausfahrtsmelder wieder auf false gesetzt werden.
}