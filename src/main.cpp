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
    Serial.println();
    Serial.println(F("=== Mega1 boot ==="));

    initPinsMega1();

    // Defensive I2C bus release (Mega2560: SDA=20, SCL=21)
    pinMode(20, INPUT_PULLUP);
    pinMode(21, INPUT_PULLUP);

    // Defensive I2C bus release: SDA=20, SCL=21
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

    pinMode(PIN_DATA_READY, INPUT); // DRDY idle HIGH (open-drain)
    updateDataReadyPin(mega1GetPending());

    // Prepare first payload BEFORE enabling I2C, so ESP never reads garbage
    memset(&g_payload,     0, sizeof(g_payload));
    memset(&s_lastPayload, 0, sizeof(s_lastPayload));
    g_payload.bootId = g_bootId;

    // On boot: mark STATUS/DIAG pending so the ESP can pull immediately
    mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);

    i2cSlaveBegin(I2C_ADDR_MEGA1);
}

void loop()
{
    const uint32_t now = millis();

    // Commands vom ESP verarbeiten (NICHT im ISR!)
    i2cSlaveProcessQueue();


    // Mega2-Style Log (keine ISR-Logs!): alle 1000ms und on-change
    static uint32_t lastPrint = 0;
    static uint16_t lastPend  = 0xFFFF;
    static uint32_t lastReq   = 0xFFFFFFFFUL;
    static uint32_t lastRx    = 0xFFFFFFFFUL;
    static uint8_t  lastCmd   = 0xFF;
    static uint8_t  lastLen   = 0xFF;

    if (now - lastPrint >= 1000)
    {
        lastPrint = now;

        const I2CDebugSnapshot s = i2cGetDebugSnapshot();
        const uint16_t pend = mega1GetPending();
        const uint8_t drdyPin = (uint8_t)digitalRead(PIN_DATA_READY);

        const bool changed =
            (pend != lastPend) || (s.reqCount != lastReq) || (s.rxCount != lastRx) ||
            (s.lastCmd != lastCmd) || (s.lastRxLen != lastLen);

        if (changed)
        {
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
            Serial.print(F(" len=")); Serial.println(s.lastRxLen);
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
    }

    // ---------------- Weichen Scheduler ----------------
    if (now - tWeichen >= 5)
    {
        tWeichen = now;
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

        // Change Detection -> nur bei Änderung DRDY aktivieren
        if (memcmp(&g_payload, &s_lastPayload, sizeof(g_payload)) != 0)
        {
            s_lastPayload = g_payload;
            g_payloadDirty = true;
            mega1SetPending(M1_PEND_STATUS | M1_PEND_DIAG);
        }

        // Status/Diag Snapshots für I2C onRequest vorbereiten
        i2cSlaveUpdateSnapshots();
    }
}
