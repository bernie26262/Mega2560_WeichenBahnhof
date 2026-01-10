#include "I2CSlave.h"
#include "I2CProtocol.h"
#include "system/system_status_payload.h"
#include "system/mega1_diag_payload.h"

#include "payload.h"
#include "WeichenHub.h"
#include "Modus.h"
#include "BahnhofController.h"
#include "TrackPowerHub.h"

#include <Arduino.h>
#include <Wire.h>

// Globale Objekte (definiert in main.cpp)
extern WeichenHub        weichenHub;
extern BahnhofController bfController;
extern TrackPowerHub     trackPowerHub;
extern ModusController   modusController;

// --------------------------------------------------
// I2C Debug Counters (nur über Snapshot nach außen)
// --------------------------------------------------
static volatile uint32_t g_i2cReqCount = 0;
static volatile uint32_t g_i2cRxCount  = 0;
static volatile uint8_t  g_lastCmd     = 0;
static volatile uint8_t  g_lastRxLen   = 0;

// optional: für Statusdaten, um mismatch zu sehen
static volatile uint8_t  g_lastSentVer  = 0;
static volatile uint8_t  g_lastSentNode = 0;
static volatile uint16_t g_lastSentSize = 0;

// --------------------------------------------------
// Externe Controller (aus main.cpp)
// --------------------------------------------------
extern WeichenHub        weichenHub;
extern ModusController   modusController;
extern BahnhofController bfController;
extern TrackPowerHub     trackPowerHub;

// --------------------------------------------------
// Interner Command-Buffer
// --------------------------------------------------
static uint8_t s_cmd = 0;
static uint8_t s_buf[8];
static uint8_t s_len = 0;

// Optional: 1-Byte Response nach einem CMD (Mega2-Style)
static bool    s_cmdResponsePending = false;
static uint8_t s_cmdResponseOk      = 1;   // 1 = OK, 0 = FAIL


// Für read-only Snapshot-Kommandos (ohne 1-Byte ACK davor)
enum class NextResponse : uint8_t { NONE=0, DIAG=1 };
static volatile NextResponse s_nextResponse = NextResponse::NONE;
static volatile uint8_t s_diagSeq = 0;

// --------------------------------------------------
// Initialisierung
// --------------------------------------------------
void i2cSlaveBegin(uint8_t address)
{
    Wire.begin(address);
    Wire.onReceive(i2cOnReceive);
    Wire.onRequest(i2cOnRequest);
}

// --------------------------------------------------
// Master → Slave
// --------------------------------------------------
void i2cOnReceive(int len)
{
    // Zählen + letzte RX-Länge (Ereignis-Zähler, nicht "gültige CMD")
    g_i2cRxCount++;
    g_lastRxLen = (uint8_t)len;

    if (len <= 0 || (size_t)len > sizeof(s_buf))
        return;

    s_len = 0;
    while (Wire.available() && s_len < sizeof(s_buf))
        s_buf[s_len++] = Wire.read();

    if (s_len == 0)
        return;

    s_cmd = s_buf[0];
    g_lastCmd = s_cmd;

    // Ereignis-Log: nur bei gültigem CMD-Paket
    Serial.print(F("[M1] I2C RX cmd=0x"));
    Serial.print(g_lastCmd, HEX);
    Serial.print(F(" len="));
    Serial.println((unsigned)s_len);

    switch (s_cmd)
    {
        case CMD_GET_DIAG:
            // Read-only Snapshot: nächste Read-Phase liefert Diagnosepaket (ohne 1-Byte ACK)
            s_nextResponse = NextResponse::DIAG;
            return;

        case CMD_SET_MODE:
            if (s_len >= 2)
            {
                modusController.setMode((BetriebsModus)s_buf[1]);
                g_payloadDirty = true;
                digitalWrite(PIN_DATA_READY, HIGH);
            }
            break;

        case CMD_SET_WEICHE:
            if (s_len >= 3)
            {
                weichenHub.enqueueWeiche(s_buf[1], s_buf[2] != 0);
                // Prüfung erfolgt später automatisch
            }
            break;


        case CMD_SET_BHF_POWER:
            if (s_len >= 3)
            {
                const uint8_t bhf = s_buf[1];
                const bool on = (s_buf[2] != 0);
                trackPowerHub.setPower(bhf, on);
                g_payloadDirty = true;
                digitalWrite(PIN_DATA_READY, HIGH);
            }
            break;

        case CMD_RELEASE_BHF:
            if (s_len >= 2)
            {
                bfController.manualRelease(s_buf[1]);
                g_payloadDirty = true;
                digitalWrite(PIN_DATA_READY, HIGH);
            }
            break;

        case CMD_ACK_ERROR:
            if (s_len >= 2)
            {
                uint8_t mask = s_buf[1];
                g_payload.errorFlags &= ~mask;

                g_payloadDirty = true;
                digitalWrite(PIN_DATA_READY, HIGH);
            }
            break;

        default:
            break;
    }

    // Mega2-kompatibel: nach einem CMD eine 1-Byte Antwort bereitstellen.
    s_cmdResponseOk      = 1;
    s_cmdResponsePending = true;
}

// --------------------------------------------------
// Master ← Slave
// --------------------------------------------------
void i2cOnRequest()
{
    g_i2cReqCount++;

    // Ereignis-Log: erster Request (zeigt Bus-Kommunikation sofort)
    if (g_i2cReqCount == 1)
        Serial.println(F("[M1] first I2C request"));

    // Read-only Snapshot: Diagnosepaket (<=32B)
    if (s_nextResponse == NextResponse::DIAG)
    {
        s_nextResponse = NextResponse::NONE;

        Mega1DiagV1 d{};
        d.version = 1;
        d.flags   = 0x01; // valid
        d.seq     = ++s_diagSeq;
        d.mode    = (uint8_t)modusController.mode();
        d.warnings = 0;

                const uint16_t mask = (NUM_WEICHEN >= 16) ? 0xFFFFu : (uint16_t)((1u << NUM_WEICHEN) - 1u);

        d.weicheIstGeradeBits  = (uint16_t)(weichenHub.buildWeichenIstBits()  & mask);
        d.weicheSollGeradeBits = (uint16_t)(weichenHub.buildWeichenBits()     & mask);

        // Slow/Reduktion: pinRed LOW = aktiv (nur Weichen mit hasRed)
        d.weicheSlowActiveBits = (uint16_t)(weichenHub.buildWeichenSlowActiveBits() & mask);

        // Bahnhofs-Power: HIGH = AN
        uint8_t pm = 0;
        for (uint8_t i = 0; i < BHF_COUNT; ++i)
        {
            if (digitalRead(BHF_TRACK_POWER_PIN[i]) == HIGH)
                pm |= (1u << i);
        }
        d.powerMask = pm;

        d.uptime16 = (uint16_t)(millis() / 100);

        Wire.write((const uint8_t*)&d, sizeof(d));
        return;
    }

    // Mega2-Style: Wenn vorher ein CMD kam, erst 1 Byte OK/FAIL ausgeben
    if (s_cmdResponsePending)
    {
        Wire.write(&s_cmdResponseOk, 1);
        s_cmdResponsePending = false;
        return;
    }

    // Default: SystemStatus direkt ausgeben (wie Mega2)
    SystemStatus st{};
    st.version = SYSTEM_STATUS_VERSION;
    st.nodeId  = NODE_MEGA1;
    st.size    = sizeof(SystemStatus);

    st.uptimeMs = millis();
    st.bootId   = 1;

    // Mega1: keine Emergencies, nur ggf. Warnings (später)
    st.flags = SYS_OK;

    st.safetyErrorType  = 0;
    st.safetyErrorIndex = 0;

    g_lastSentVer  = st.version;
    g_lastSentNode = st.nodeId;
    g_lastSentSize = st.size;

    Wire.write(reinterpret_cast<const uint8_t*>(&st), sizeof(SystemStatus));
}

// --------------------------------------------------
// Debug Snapshot (für Serial-Ausgaben im loop())
// --------------------------------------------------
I2CDebugSnapshot i2cGetDebugSnapshot()
{
    I2CDebugSnapshot s{};
    noInterrupts();
    s.reqCount     = g_i2cReqCount;
    s.rxCount      = g_i2cRxCount;
    s.lastCmd      = g_lastCmd;
    s.lastRxLen    = g_lastRxLen;
    s.lastSentVer  = g_lastSentVer;
    s.lastSentNode = g_lastSentNode;
    s.lastSentSize = g_lastSentSize;
    interrupts();
    return s;
}
