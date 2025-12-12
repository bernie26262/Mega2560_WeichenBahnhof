#pragma once
#include <Arduino.h>
#include "pins_mega1.h"
#include "Fahrstrassen_defs.h"

// ==================================================
// Status-Payload Mega1 → ESP32
// ==================================================
struct Mega1StatusPayload
{
    uint16_t bootId;

    // -------- Sensoren --------
    uint16_t kontaktBits;

    // -------- Weichen --------
    uint16_t weichenBits;       // SOLL-Stellung (1 = GERADE)
    uint16_t weichenIstBits;    // IST-Stellung  (1 = GERADE, aus Rückmelder)
    uint16_t weichenOkBits;     // 1 = OK, 0 = Fehler (Soll != Ist)

    // -------- Modus / Fahrstraße --------
    uint8_t  modus;
    int8_t   activeRoute;
    uint8_t  errorFlags;

    // -------- Fahrstraßen --------
    uint8_t  fsCounter[NUM_STW_FS];

    // -------- Debug / Trace --------
    uint8_t  lastSensorTriggered;
    uint8_t  lastFsTriggered;
    uint8_t  lastResetSensor;

    uint8_t  lastWeiche;
    uint8_t  lastWeicheStellung;

    // -------- Bahnhöfe --------
    uint8_t  bhfOccupied[BHF_COUNT];
    uint8_t  bhfPower[BHF_COUNT];
    uint8_t  bhfTimerRunning[BHF_COUNT];

    uint8_t  lastBhfEvent;
};

// ==================================================
// Globale Payloads (Definition NUR in main.cpp)
// ==================================================
extern Mega1StatusPayload g_payload;
extern volatile bool g_payloadDirty;
