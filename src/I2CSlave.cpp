#include "I2CSlave.h"
#include "I2CProtocol.h"

#include "payload.h"
#include "WeichenHub.h"
#include "Modus.h"
#include "BahnhofController.h"

// --------------------------------------------------
// Externe Controller (aus main.cpp)
// --------------------------------------------------
extern WeichenHub        weichenHub;
extern ModusController   modusController;
extern BahnhofController bfController;

// --------------------------------------------------
// Interner Command-Buffer
// --------------------------------------------------
static uint8_t s_cmd = 0;
static uint8_t s_buf[8];
static uint8_t s_len = 0;

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
    if (len <= 0 || (size_t)len > sizeof(s_buf))
        return;

    s_len = 0;
    while (Wire.available() && s_len < sizeof(s_buf))
        s_buf[s_len++] = Wire.read();

    s_cmd = s_buf[0];

    switch (s_cmd)
    {
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

        case CMD_RELEASE_BHF:
            if (s_len >= 2)
            {
                // ⚠️ Namensabgleich:
                // Bitte ggf. anpassen, falls Methode anders heißt
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
}

// --------------------------------------------------
// Master ← Slave
// --------------------------------------------------
void i2cOnRequest()
{
    if (s_cmd == CMD_GET_STATUS)
    {
        Wire.write(
            reinterpret_cast<uint8_t*>(&g_payload),
            sizeof(Mega1StatusPayload)
        );

        g_payloadDirty = false;
        digitalWrite(PIN_DATA_READY, LOW);
    }
    else
    {
        Wire.write((uint8_t)0xAA); // ACK
    }

    s_cmd = 0;
}
