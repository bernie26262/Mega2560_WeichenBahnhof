#include "I2CSlave.h"
#include "I2CProtocol.h"
#include "system/system_status_payload.h"
#include "system/mega1_diag_payload.h"

#include "payload.h"
#include "SensorHub.h"
#include "WeichenHub.h"
#include "Modus.h"
#include "BahnhofController.h"
#include "TrackPowerHub.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// Globale Objekte (definiert in main.cpp)
extern WeichenHub        weichenHub;
extern SensorHub         sensorHub;
extern BahnhofController bfController;
extern TrackPowerHub     trackPowerHub;
extern ModusController   modusController;
extern Mega1StatusPayload g_payload;
extern volatile bool g_payloadDirty;

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

// Pending/DRDY Steuerung (implementiert in main.cpp)
extern void mega1SetPending(uint16_t bits);
extern void mega1ClearPending(uint16_t bits);
extern uint16_t mega1GetPending();

// Für read-only Snapshot-Kommandos (ohne 1-Byte ACK davor)
enum class NextResponse : uint8_t { NONE=0, DIAG=1, PENDING=2 };
// erweitert: zusätzlich pendingMask als read-only
// (wie Mega2)
static volatile NextResponse s_nextResponse = NextResponse::NONE;
static volatile bool s_pendingPrimed = false; // set after pending-mask read; allows clearing pending on subsequent reads
static volatile uint8_t s_diagSeq = 0;
static volatile bool s_clearSensorEdges = false; // defer edge clear to loop

// --------------------------------------------------
// Snapshots (werden im loop() gebaut, im ISR nur rausgeschrieben)
// --------------------------------------------------
static SystemStatus s_statusSnap;
static Mega1DiagV1  s_diagSnap;

// --------------------------------------------------
// Command Queue (ISR -> loop)
// --------------------------------------------------
namespace {
    struct CmdItem { uint8_t cmd; uint8_t a; uint8_t b; };
    static constexpr uint8_t CMDQ_SIZE = 8;
    static volatile uint8_t s_qHead = 0;
    static volatile uint8_t s_qTail = 0;
    static CmdItem s_q[CMDQ_SIZE];

    static bool qPush(uint8_t cmd, uint8_t a=0, uint8_t b=0)
    {
        const uint8_t next = (uint8_t)((s_qHead + 1) % CMDQ_SIZE);
        if (next == s_qTail) return false; // full
        s_q[s_qHead] = {cmd, a, b};
        s_qHead = next;
        return true;
    }

    static bool qPop(CmdItem& out)
    {
        if (s_qTail == s_qHead) return false;
        out = s_q[s_qTail];
        s_qTail = (uint8_t)((s_qTail + 1) % CMDQ_SIZE);
        return true;
    }
}


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

    // WICHTIG: KEIN Serial im ISR (Wire callbacks laufen im IRQ-Kontext)
    // Mega2-kompatibler 1-Byte ACK (nur für "write commands")
    uint8_t ack = 0;
    bool wantAck = false;


    switch (s_cmd)
    {
        case CMD_GET_STATUS:
            // Read-only: Master wird danach SystemStatus lesen.
            // -> KEIN 1-Byte ACK vorbereiten!
            s_nextResponse = NextResponse::NONE;
            return;

        case CMD_GET_DIAG:
            // Read-only Snapshot: nächste Read-Phase liefert Diagnosepaket (ohne 1-Byte ACK)
            s_nextResponse = NextResponse::DIAG;
            return;

        case CMD_SET_MODE:
            if (s_len >= 2)
            {
                // in Queue, Ausführung im loop()
                ack = qPush(CMD_SET_MODE, s_buf[1], 0) ? 1 : 0;
                wantAck = true;
            }
            break;

        case CMD_SET_WEICHE:
            if (s_len >= 3)
            {
                ack = qPush(CMD_SET_WEICHE, s_buf[1], s_buf[2]) ? 1 : 0;
                wantAck = true;
            }
            break;


        case CMD_SET_BHF_POWER:
            if (s_len >= 3)
            {
                ack = qPush(CMD_SET_BHF_POWER, s_buf[1], s_buf[2]) ? 1 : 0;
                wantAck = true;
            }
            break;
        
        case CMD_START_SELFTEST:
        {
            ack = qPush(CMD_START_SELFTEST, 0, 0) ? 1 : 0;
            wantAck = true;
            break;
        }

        case CMD_RELEASE_BHF:
            if (s_len >= 2)
            {
                ack = qPush(CMD_RELEASE_BHF, s_buf[1], 0) ? 1 : 0;
                wantAck = true;
            }
            break;

        case CMD_ACK_ERROR:
            if (s_len >= 2)
            {
                ack = qPush(CMD_ACK_ERROR, s_buf[1], 0) ? 1 : 0;
                wantAck = true;
            }
            break;
        
        case CMD_GET_PENDING_MASK:
            // Read-only: nächste Read-Phase liefert pendingMask (uint16)
            s_nextResponse = NextResponse::PENDING;
            return;


        default:
            break;
    }

    // Nur bei "write commands" ACK bereitstellen
    if (wantAck)
    {
        s_cmdResponseOk      = ack;
        s_cmdResponsePending = true;
    }
}

// --------------------------------------------------
// Master ← Slave
// --------------------------------------------------
void i2cOnRequest()
{
    g_i2cReqCount++;

       
    // WICHTIG: KEIN Serial im ISR
    // Read-only: pendingMask (uint16)
    if (s_nextResponse == NextResponse::PENDING)
    {
        s_nextResponse = NextResponse::NONE;
        const uint16_t pm = mega1GetPending();
        Wire.write((const uint8_t*)&pm, sizeof(pm));
        // Priming: only clear pending bits after the master explicitly queried the pending-mask.
        // This prevents "background" status/diag reads from clearing DRDY too quickly.
        s_pendingPrimed = (pm != 0);
        return;
    }

    // Read-only Snapshot: Diagnosepaket (<=32B)
    if (s_nextResponse == NextResponse::DIAG)
    {
        s_nextResponse = NextResponse::NONE;

        Wire.write((const uint8_t*)&s_diagSnap, sizeof(s_diagSnap));
        s_clearSensorEdges = true; // clear rise/fall AFTER the snapshot was served
        if (s_pendingPrimed)
        {
            mega1ClearPending(M1_PEND_DIAG);
            if (mega1GetPending() == 0) s_pendingPrimed = false;
        }
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
    g_lastSentVer  = s_statusSnap.version;
    g_lastSentNode = s_statusSnap.nodeId;
    g_lastSentSize = s_statusSnap.size;
    Wire.write(reinterpret_cast<const uint8_t*>(&s_statusSnap), sizeof(s_statusSnap));
    if (s_pendingPrimed)
    {
        mega1ClearPending(M1_PEND_STATUS);
        if (mega1GetPending() == 0) s_pendingPrimed = false;
    }
    return;
}

// --------------------------------------------------
// loop()-Funktionen (keine ISR)
// --------------------------------------------------
void i2cSlaveProcessQueue()
{
    CmdItem it{};
    while (qPop(it))
    {
        switch (it.cmd)
        {
            case CMD_SET_MODE:
            {
                const BetriebsModus newMode = (BetriebsModus)it.a;
                // Schutz: während Weichen-Selbsttest läuft, NICHT auf AUTOMATIK schalten
                if (newMode == BetriebsModus::AUTOMATIK && weichenHub.isSelftestActive())
                    break;
                modusController.setMode(newMode);
                g_payloadDirty = true;
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;
            }

            case CMD_SET_WEICHE:
                (void)weichenHub.enqueueWeiche(it.a, it.b != 0);
                // UI soll sofort reagieren
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;

            case CMD_SET_BHF_POWER:
                trackPowerHub.setPower(it.a, it.b != 0);
                g_payloadDirty = true;
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;

            case CMD_START_SELFTEST:
                (void)weichenHub.startSelftest();
                g_payloadDirty = true;
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;

            case CMD_RELEASE_BHF:
                bfController.manualRelease(it.a);
                g_payloadDirty = true;
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;

            case CMD_ACK_ERROR:
                g_payload.errorFlags &= ~(uint8_t)it.a;
                g_payloadDirty = true;
                mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
                break;

            default:
                break;
        }
    }
}

void i2cSlaveUpdateSnapshots()
{
    // Defer-clear sensor edge masks (must NOT run inside Wire ISR)
    if (s_clearSensorEdges)
    {
        noInterrupts();
        s_clearSensorEdges = false;
        interrupts();
        sensorHub.clearEdgeMasks();
    }

    // -------- DIAG snapshot --------
    Mega1DiagV1 d{};
    d.version = 1;
    d.flags   = 0x01;
    d.seq     = ++s_diagSeq;
    d.mode    = (uint8_t)modusController.mode();
    d.warnings = 0;

    const uint16_t mask = (NUM_WEICHEN >= 16) ? 0xFFFFu : (uint16_t)((1u << NUM_WEICHEN) - 1u);
    d.weicheIstGeradeBits  = (uint16_t)(weichenHub.buildWeichenIstBits()  & mask);
    d.weicheSollGeradeBits = (uint16_t)(weichenHub.buildWeichenBits()     & mask);
    d.weicheSlowSelectedBits = (uint16_t)(weichenHub.buildWeichenSlowSelectedBits() & mask);

    uint8_t pm = 0;
    for (uint8_t i = 0; i < BHF_COUNT; ++i)
        if (digitalRead(BHF_TRACK_POWER_PIN[i]) == HIGH)
            pm |= (1u << i);
    d.powerMask = pm;

    d.uptime16 = (uint16_t)(millis() / 100);
    d.selftestFlags = 0;
    if (weichenHub.isSelftestActive()) d.selftestFlags |= 0x01u;
    if (weichenHub.isSelftestDone())   d.selftestFlags |= 0x02u;
    const uint16_t failMask = (uint16_t)(weichenHub.selftestFailMask() & mask);
    d.selftestFailMask = failMask;
    if (failMask) d.selftestFlags |= 0x04u;
    d.selftestCurrentIdx = weichenHub.selftestCurrentIdx();
    
    // Digital sensors (S0..S23) – masks (sticky edges)
    d.sensorActiveMask = sensorHub.activeMask();
    d.sensorRiseMask   = sensorHub.riseMask();
    d.sensorFallMask   = sensorHub.fallMask();

    // -------- STATUS snapshot --------
    SystemStatus st{};
    st.version = SYSTEM_STATUS_VERSION;
    st.nodeId  = NODE_MEGA1;
    st.size    = sizeof(SystemStatus);
    st.uptimeMs = millis();
    st.bootId   = 1;
    st.flags    = SYS_OK;

    uint8_t m1WarningMask = 0;
    if ((uint16_t)(weichenHub.selftestFailMask() & mask) != 0)
        m1WarningMask |= 0x01;
    if (m1WarningMask != 0)
        st.flags |= SYS_WARNING_PRESENT;
    st.reserved = (uint16_t)m1WarningMask;
    st.safetyErrorType  = 0;
    st.safetyErrorIndex = 0;

    noInterrupts();
    memcpy(&s_diagSnap,   &d,  sizeof(d));
    memcpy(&s_statusSnap, &st, sizeof(st));
    interrupts();
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
