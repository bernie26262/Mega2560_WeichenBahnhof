#include <Arduino.h>
#include <util/atomic.h>

#include "pins_mega1.h"
#include "SensorHub.h"
#include "WeichenHub.h"
#include "BahnhofController.h"
#include "Fahrstrassen.h"
#include "Modus.h"
#include "payload.h"
#include "TrackPowerHub.h"
#include "I2CSlave.h"
#include "I2CProtocol.h"

// --------------------------------------------------
// Debug: Trace input-driven changes (Weichen IST, PowerMask)
// Set to 0 to silence.
#ifndef DEBUG_M1_TRACE_INPUTS
#define DEBUG_M1_TRACE_INPUTS 0
#endif

// Boot-/Crash-Diagnose: Reset-Ursache aus MCUSR loggen
#ifndef DEBUG_M1_RESET_CAUSE
#define DEBUG_M1_RESET_CAUSE 0
#endif

// Periodisches M1-I2C/DRDY-Statuslog.
// 1 = aktiv, 0 = aus. Für ruhige Test-Envs standardmäßig aus.
#ifndef DEBUG_M1_I2C_HEARTBEAT
#define DEBUG_M1_I2C_HEARTBEAT 0
#endif

// Boot-Logs (Banner + [M1BOOT])
#ifndef DEBUG_M1_BOOT_LOG
#define DEBUG_M1_BOOT_LOG 1
#endif

static void logResetCauseIfEnabled()
{
#if DEBUG_M1_RESET_CAUSE
    const uint8_t mcusr = MCUSR;
    MCUSR = 0;

    Serial.print(F("[M1RST] MCUSR=0x"));
    Serial.print(mcusr, HEX);
    Serial.print(F(" cause="));

    bool any = false;
    if (mcusr & _BV(PORF))  { Serial.print(F("POR "));  any = true; }
    if (mcusr & _BV(EXTRF)) { Serial.print(F("EXTR ")); any = true; }
    if (mcusr & _BV(BORF))  { Serial.print(F("BOR "));  any = true; }
    if (mcusr & _BV(WDRF))  { Serial.print(F("WDR "));  any = true; }
    if (mcusr & _BV(JTRF))  { Serial.print(F("JTRF ")); any = true; }
    if (!any) Serial.print(F("none"));
    Serial.println();
#else
    MCUSR = 0;
#endif
}


// --------------------------------------------------
// DataReady / Pending (Mega1 -> ESP)
// DRDY ist active LOW, open-drain emuliert:
//  - LOW: OUTPUT + LOW
//  - HIGH: INPUT (high-Z), Pullup zieht hoch
// --------------------------------------------------
volatile uint16_t g_pendingMask = 0;

static inline void drdyAssertLow()
{
    pinMode(PIN_DATA_READY, OUTPUT);
    digitalWrite(PIN_DATA_READY, LOW);
}

static inline void drdyReleaseHigh()
{
    pinMode(PIN_DATA_READY, INPUT); // high-Z
}

static void updateDataReadyPin(uint16_t pendingNow)
{
    if (pendingNow)
        drdyAssertLow();
    else
        drdyReleaseHigh();
}

void mega1SetPending(uint16_t bits)
{
    uint16_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        g_pendingMask |= bits;
        now = g_pendingMask;
    }
    updateDataReadyPin(now);
}

void mega1ClearPending(uint16_t bits)
{
    uint16_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        g_pendingMask &= (uint16_t)~bits;
        now = g_pendingMask;
    }
    updateDataReadyPin(now);
}

uint16_t mega1GetPending()
{
    uint16_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        now = g_pendingMask;
    }
    return now;
}

// Globale Objekte
SensorHub         sensorHub;
WeichenHub        weichenHub;
BahnhofController bfController;
Fahrstrassen      fahrstrassen;
ModusController   modusController;
TrackPowerHub     trackPowerHub;

Mega1StatusPayload g_payload;
static Mega1StatusPayload s_lastPayload; // für Change-Detection

volatile bool g_payloadDirty = false;
uint16_t g_bootId = 0;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------


// Diese Funktion kann später von der I2C-Slave-ISR aufgerufen werden,
// wenn das Payload an den Master übertragen wurde.
void markPayloadTransmitted()
{
    // Legacy hook: wird nach erfolgreichem STATUS-Read verwendet
    mega1ClearPending(M1_PEND_STATUS);
}


// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(200);
    
    #if DEBUG_M1_BOOT_LOG
    Serial.println();
    Serial.println(F("=== Mega1 boot ==="));
    #endif
    logResetCauseIfEnabled();

    // Startup-Checklist Flags:
    // selftestDone muss nach jedem Mega1-Boot wieder "0" sein,
    // damit das ESP/UI den Selftest deterministisch anfordern und beobachten kann.
    weichenHub.clearSelftestDone();


    initPinsMega1();

    // Defensive I2C bus release (Mega2560: SDA=20, SCL=21)
    pinMode(20, INPUT_PULLUP);
    pinMode(21, INPUT_PULLUP);

    randomSeed(analogRead(0));
    g_bootId = (uint16_t)random(1, 65000);

    sensorHub.begin();
    weichenHub.begin();
    bfController.begin();
    fahrstrassen.begin();
    modusController.begin();
    trackPowerHub.begin();
    bfController.setPowerHub(&trackPowerHub);

    // Force first stable DIAG after inputs are settled
    mega1SetPending(M1_PEND_DIAG);

    pinMode(PIN_DATA_READY, INPUT); // DRDY idle HIGH (open-drain)
    updateDataReadyPin(mega1GetPending());

    // Prepare first payload BEFORE enabling I2C, so ESP never reads garbage
    memset(&g_payload,     0, sizeof(g_payload));
    memset(&s_lastPayload, 0, sizeof(s_lastPayload));
    g_payload.bootId = g_bootId;

    // On boot: mark STATUS/DIAG pending so the ESP can pull immediately
    mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);

    i2cSlaveBegin(I2C_ADDR_MEGA1);

    #if DEBUG_M1_BOOT_LOG
    // One-shot boot log: helps field-debug (no ISR logs)
    {
        const uint16_t pend = mega1GetPending();
        const uint8_t drdyPin = (uint8_t)digitalRead(PIN_DATA_READY);
        Serial.print(F("[M1BOOT] bootId=")); Serial.print(g_bootId);
        Serial.print(F(" addr=0x"));
        if (I2C_ADDR_MEGA1 < 0x10) Serial.print('0');
        Serial.print(I2C_ADDR_MEGA1, HEX);
        Serial.print(F(" i2c=READY drdyPin=")); Serial.print(drdyPin);
        Serial.print(F(" pending=0x")); Serial.println(pend, HEX);
    }
    #endif
}

void loop()
{
    const uint32_t now = millis();

    // Commands vom ESP verarbeiten (NICHT im ISR!)
    i2cSlaveProcessQueue();


    // Mega2-Style Log (keine ISR-Logs!): alle 1000ms und on-change
    static uint32_t lastPrint = 0;
    static uint32_t lastHeartbeat = 0;
    static uint16_t lastPend  = 0xFFFF;
    static uint32_t lastReq   = 0xFFFFFFFFUL;
    static uint32_t lastRx    = 0xFFFFFFFFUL;
    static uint8_t  lastCmd   = 0xFF;
    static uint8_t  lastLen   = 0xFF;

    if (DEBUG_M1_I2C_HEARTBEAT && (now - lastPrint >= 1000))
    {
        lastPrint = now;

        const I2CDebugSnapshot s = i2cGetDebugSnapshot();
        const uint16_t pend = mega1GetPending();
        const uint8_t drdyPin = (uint8_t)digitalRead(PIN_DATA_READY);

        const bool changed =
            (pend != lastPend) ||
            (s.reqCount != lastReq) || (s.rxCount != lastRx) ||
            (s.lastCmd != lastCmd) || (s.lastRxLen != lastLen);

        const bool heartbeat = (now - lastHeartbeat) >= 10000;

        if (changed || heartbeat)
        {
            if (heartbeat) lastHeartbeat = now;

            lastPend = pend; lastReq = s.reqCount; lastRx = s.rxCount;
            lastCmd  = s.lastCmd; lastLen = s.lastRxLen;

            Serial.print(F("[M1] DRDY pin=")); Serial.print(drdyPin);
            Serial.print(F(" pending=0x"));
            if (pend < 0x1000) Serial.print('0');
            if (pend < 0x0100) Serial.print('0');
            if (pend < 0x0010) Serial.print('0');
            Serial.print(pend, HEX);

            Serial.print(F(" rx=")); Serial.print(s.rxCount);
            Serial.print(F(" req=")); Serial.print(s.reqCount);
            Serial.print(F(" lastCmd=0x"));
            if (s.lastCmd < 0x10) Serial.print('0');
            Serial.print(s.lastCmd, HEX);
            Serial.print(F(" len=")); Serial.print(s.lastRxLen);
            #if DEBUG_M1_TRACE_INPUTS
            // Zusatz: relevante Zustände, die UI/ESP interessieren
            Serial.print(F(" ist=0x"));
            Serial.print((uint16_t)g_payload.weichenIstBits, HEX);
            Serial.print(F(" soll=0x"));
            Serial.print((uint16_t)g_payload.weichenBits, HEX);
            Serial.print(F(" ok=0x"));
            Serial.print((uint16_t)g_payload.weichenOkBits, HEX);
            Serial.print(F(" pwr=0x"));
            uint8_t pm=0; for(uint8_t i=0;i<BHF_COUNT;++i) if(digitalRead(BHF_TRACK_POWER_PIN[i])==HIGH) pm|=(1u<<i);
            Serial.print(pm, HEX);
            #endif
            Serial.println();
        }
    }
    static uint32_t tSensors = 0;
    static uint32_t tLogic   = 0;
    static uint32_t tWeichen = 0;
    static uint32_t tPayload = 0;

    // ---------------- Sensoren ----------------
    if (now - tSensors >= 5)
    {
        tSensors = now;
        sensorHub.update();

        if (sensorHub.consumeDiagDirty())
        {
            i2cSlaveUpdateSnapshots();      // <-- NEU: Snapshot sofort auf "jetzt" bringen
            mega1SetPending(M1_PEND_DIAG);  // <-- dann erst DRDY triggern
        }
    }

    // ---------------- Logik (nur AUTO) ----------------
    if (now - tLogic >= 10)
    {
        tLogic = now;

        if (modusController.isAuto())
        {
            fahrstrassen.handleSensorEvents(sensorHub, weichenHub);
            bfController.update(sensorHub);
        }

        // Wichtig:
        // changedMask ist für die Logik sticky und wird erst NACH der
        // Verarbeitung gelöscht. Dadurch gehen zwischen Sensor-Polling
        // und Logik-Tick keine akzeptierten Sensor-Events mehr verloren.
        sensorHub.clearChangedMask();
    }

    // ---------------- Weichen Scheduler ----------------
    if (now - tWeichen >= 5)
    {
        tWeichen = now;
        weichenHub.pollRueckmelders(now);
        weichenHub.update();
    }

    // ---------------- Payload / Status ----------------
    if (now - tPayload >= 50)
    {
        tPayload = now;

        g_payload.kontaktBits     = sensorHub.buildKontaktBits();

        g_payload.weichenBits     = weichenHub.buildWeichenBits();     // SOLL
        g_payload.weichenIstBits  = weichenHub.buildWeichenIstBits();  // IST
        g_payload.weichenOkBits   = weichenHub.buildWeichenOkBits();   // OK/FAIL

        g_payload.activeRoute     = fahrstrassen.activeRoute();
        g_payload.modus           = (uint8_t)modusController.mode();

        // Collect pending bits and set them once per loop -> avoids race with I2C onRequest
        uint16_t pendAdd = 0;


        // Change Detection (relevant fields only) -> nur bei Änderung DRDY aktivieren
        // WICHTIG: Keine DIAG-Pending-Bits hier setzen. DIAG soll nur bei echten
        // Diagnose-/Kommandostatus-Änderungen ausgelöst werden (sonst bleibt DRDY dauerhaft LOW).
        const bool statusChanged =
            (g_payload.kontaktBits    != s_lastPayload.kontaktBits)    ||
            (g_payload.weichenBits    != s_lastPayload.weichenBits)    ||
            (g_payload.weichenIstBits != s_lastPayload.weichenIstBits) ||
            (g_payload.weichenOkBits  != s_lastPayload.weichenOkBits)  ||
            (g_payload.activeRoute    != s_lastPayload.activeRoute)    ||
            (g_payload.modus          != s_lastPayload.modus);

        if (statusChanged)
         
        {
            
#if DEBUG_M1_TRACE_INPUTS
            Serial.print(F("[M1] statusChanged: kontakt=0x")); Serial.print(g_payload.kontaktBits, HEX);
            Serial.print(F(" ist=0x")); Serial.print(g_payload.weichenIstBits, HEX);
            Serial.print(F(" soll=0x")); Serial.print(g_payload.weichenBits, HEX);
            Serial.print(F(" ok=0x")); Serial.print(g_payload.weichenOkBits, HEX);
            Serial.print(F(" route=")); Serial.print(g_payload.activeRoute);
            Serial.print(F(" mode=")); Serial.println(g_payload.modus);
#endif
// Nur die relevanten Felder übernehmen, damit Debug-/Counter-Felder (falls vorhanden)
            // nicht ständig einen "Change" auslösen.
            s_lastPayload.kontaktBits    = g_payload.kontaktBits;
            s_lastPayload.weichenBits    = g_payload.weichenBits;
            s_lastPayload.weichenIstBits = g_payload.weichenIstBits;
            s_lastPayload.weichenOkBits  = g_payload.weichenOkBits;
            s_lastPayload.activeRoute    = g_payload.activeRoute;
            s_lastPayload.modus          = g_payload.modus;
            g_payloadDirty = true;
            pendAdd |= M1_PEND_STATUS;
            #if DEBUG_M1_TRACE_INPUTS
            {
                const uint16_t pm = mega1GetPending();
                const uint8_t  dr = (uint8_t)digitalRead(PIN_DATA_READY);
                Serial.print(F("[M1] setPending STATUS -> pm=0x")); Serial.print(pm, HEX);
                Serial.print(F(" drdy=")); Serial.println(dr);
            }
            #endif
        }

        // ---------------- DIAG Pending (nur bei relevanter Änderung) ----------------
        // Ziel: DIAG nur "on change" (Selftest start/step/done/fail), damit DRDY nicht dauerhaft LOW bleibt,
        // aber das Overlay zuverlässig endet.
        static uint8_t  s_lastStFlags = 0xFF;
        static uint16_t s_lastStFail  = 0xFFFF;
        static uint8_t  s_lastStIdx   = 0xFF;
        static uint16_t s_lastIstBits = 0xFFFF;
        static uint16_t s_lastSollBits= 0xFFFF;
        static uint16_t s_lastSlowBits= 0xFFFF;
        static uint8_t  s_lastPwrMask = 0xFF;
        static uint8_t  s_lastMode    = 0xFF;

        uint8_t stFlags = 0;
        if (weichenHub.isSelftestActive()) stFlags |= 0x01u; // running
        if (weichenHub.isSelftestDone())   stFlags |= 0x02u; // done

        const uint16_t mask = (NUM_WEICHEN >= 16) ? 0xFFFFu : (uint16_t)((1u << NUM_WEICHEN) - 1u);
        const uint16_t stFail = (uint16_t)(weichenHub.selftestFailMask() & mask);
        const uint8_t  stIdx  = (uint8_t)weichenHub.selftestCurrentIdx(); // 0xFF wenn inaktiv

        // --- NEW: force DIAG update on selftest end ---
        if ((s_lastStIdx != 0xFF) && (stIdx == 0xFF))
        {
            pendAdd |= M1_PEND_DIAG;
        }

        // Weichen-/Power/Mode-Felder, die im DIAG-Snapshot landen (und von der UI genutzt werden)
        const uint16_t istBits  = (uint16_t)(weichenHub.buildWeichenIstBits()        & mask);
        const uint16_t sollBits = (uint16_t)(weichenHub.buildWeichenBits()           & mask);
        const uint16_t slowBits = (uint16_t)(weichenHub.buildWeichenSlowSelectedBits() & mask);

        uint8_t pwrMask = 0;
        for (uint8_t i = 0; i < BHF_COUNT; ++i)
            if (digitalRead(BHF_TRACK_POWER_PIN[i]) == HIGH)
                pwrMask |= (1u << i);

        const uint8_t modeNow = (uint8_t)modusController.mode();

        const bool diagChanged =
            (stFlags != s_lastStFlags) ||
            (stFail  != s_lastStFail)  ||
            (stIdx   != s_lastStIdx)   ||
            (istBits != s_lastIstBits) ||
            (sollBits!= s_lastSollBits)||
            (slowBits!= s_lastSlowBits)||
            (pwrMask != s_lastPwrMask) ||
            (modeNow != s_lastMode);

        if (diagChanged)
        {
#if DEBUG_M1_TRACE_INPUTS
            Serial.print(F("[M1] diagChanged: stFlags=0x")); Serial.print(stFlags, HEX);
            Serial.print(F(" fail=0x")); Serial.print(stFail, HEX);
            Serial.print(F(" idx=")); Serial.print(stIdx);
            Serial.print(F(" ist=0x")); Serial.print(istBits, HEX);
            Serial.print(F(" soll=0x")); Serial.print(sollBits, HEX);
            Serial.print(F(" slowSel=0x")); Serial.print(slowBits, HEX);
            Serial.print(F(" pwr=0x")); Serial.print(pwrMask, HEX);
            Serial.print(F(" mode=")); Serial.println(modeNow);
#endif            
            s_lastStFlags = stFlags;
            s_lastStFail  = stFail;
            s_lastStIdx   = stIdx;
            s_lastIstBits = istBits;
            s_lastSollBits= sollBits;
            s_lastSlowBits= slowBits;
            s_lastPwrMask = pwrMask;
            s_lastMode    = modeNow;
            pendAdd |= M1_PEND_DIAG;
        }

        // Status/Diag Snapshots für I2C onRequest vorbereiten
        // IMPORTANT: update snapshots BEFORE setting pending/DRDY.
        // Otherwise the ESP may read the *previous* snapshot ("one frame late").
        i2cSlaveUpdateSnapshots();

        // Apply pending bits AFTER snapshots are updated (prevents ISR race between STATUS and DIAG)
        if (pendAdd)
        {
            mega1SetPending(pendAdd);
#if DEBUG_M1_TRACE_INPUTS
            const uint16_t pm = mega1GetPending();
            const uint8_t  dr = (uint8_t)digitalRead(PIN_DATA_READY);
            Serial.print(F("[M1] setPending COMBINED=0x")); Serial.print(pendAdd, HEX);
            Serial.print(F(" -> pm=0x")); Serial.print(pm, HEX);
            Serial.print(F(" drdy=")); Serial.println(dr);
#endif

        }
    }
}
