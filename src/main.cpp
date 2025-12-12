#include <Arduino.h>

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


static void updateDataReadyPin()
{
    digitalWrite(PIN_DATA_READY, g_payloadDirty ? HIGH : LOW);
}

// Diese Funktion kann später von der I2C-Slave-ISR aufgerufen werden,
// wenn das Payload an den Master übertragen wurde.
void markPayloadTransmitted()
{
    g_payloadDirty = false;
    updateDataReadyPin();
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup()
{
    initPinsMega1();

    randomSeed(analogRead(0));
    g_bootId = (uint16_t)random(1, 65000);

    sensorHub.begin();
    weichenHub.begin();
    bfController.begin();
    fahrstrassen.begin();
    modusController.begin();
    trackPowerHub.begin();
    
    pinMode(PIN_DATA_READY, OUTPUT);
    digitalWrite(PIN_DATA_READY, LOW);

    i2cSlaveBegin(I2C_ADDR_MEGA1);


    memset(&g_payload,     0, sizeof(g_payload));
    memset(&s_lastPayload, 0, sizeof(s_lastPayload));
    g_payload.bootId = g_bootId;
}

void loop()
{
    const uint32_t now = millis();

    static uint32_t tSensors   = 0;
    static uint32_t tLogic     = 0;
    static uint32_t tWeichen   = 0;
    static uint32_t tPayload   = 0;

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
        g_payload.kontaktBits     = sensorHub.buildKontaktBits();

        g_payload.weichenBits    = weichenHub.buildWeichenBits();     // SOLL
        g_payload.weichenIstBits = weichenHub.buildWeichenIstBits();  // IST
        g_payload.weichenOkBits  = weichenHub.buildWeichenOkBits();   // OK/FAIL

        g_payload.activeRoute    = fahrstrassen.activeRoute();
        g_payload.modus          = (uint8_t)modusController.mode();

        g_payloadDirty = true;

    }
}
