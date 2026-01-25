#pragma once
#include <Arduino.h>
#include <stdint.h>

struct I2CDebugSnapshot
{
    uint32_t reqCount;
    uint32_t rxCount;
    uint8_t  lastCmd;
    uint8_t  lastRxLen;
    uint8_t  lastSentVer;
    uint8_t  lastSentNode;
    uint16_t lastSentSize;
};

void i2cSlaveBegin(uint8_t address);

// Optional: nur nötig, wenn du diese Funktionen irgendwo direkt aufrufst.
// (Wire ruft sie intern über die Callback-Registrierung auf.)
void i2cOnReceive(int len);
void i2cOnRequest();

// Verarbeitung der aus I2C empfangenen Commands (läuft im loop(), NICHT im ISR!)
void i2cSlaveProcessQueue();

// Snapshot-Erzeugung für Status/Diag (läuft im loop(), NICHT im ISR!)
void i2cSlaveUpdateSnapshots();

// ISR-sichere Snapshot-Abfrage (für Serial-Debug im loop())
I2CDebugSnapshot i2cGetDebugSnapshot();
