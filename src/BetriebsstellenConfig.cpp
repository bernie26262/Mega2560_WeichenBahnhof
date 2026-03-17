#include "BetriebsstellenConfig.h"
#include "pins_mega1.h"

// ==================================================
// Bahnhöfe – zentrale fachliche Definition
// ==================================================
const BahnhofConfig BAHNHOF_CONFIG[BHF_COUNT] =
{
    {
        "Bhf0",
        SENSOR_S2,
        SENSOR_S5,
        5000,
        "Einfahrt ueber S2, Timerstart ueber S5"
    },
    {
        "Bhf1",
        SENSOR_S2,
        SENSOR_S3,
        5000,
        "Einfahrt ueber S2, Timerstart ueber S3"
    },
    {
        "Bhf2",
        SENSOR_S8,
        SENSOR_S9,
        5000,
        "Einfahrt ueber S8, Timerstart ueber S9"
    },
    {
        "Bhf3",
        SENSOR_S8,
        SENSOR_S10,
        5000,
        "Einfahrt ueber S8, Timerstart ueber S10"
    }
};

// ==================================================
// Fahrstraßen – zentrale fachliche Definition
//
// Legende:
// - triggerSensor: Schaltgleis, das die Fahrstraße auslöst
// - resetSensors : Schaltgleise, die Zähler/aktiv-Status zurücksetzen
// - minCount     : Schritt gilt ab diesem Auslöse-Zählerstand
// ==================================================

// --------------------------------------------------
// FS0: Zufahrt West / alternierende W8-Umschaltung
// Trigger: S0
// Reset  : S3 oder S6
// Wirkung:
//   count >= 0 : W2 gerade, W3 abbiegen
//   count >= 1 : zusätzlich W8 abbiegen
//   count >= 2 : W8 wieder gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS0_STEPS[] =
{
    { 2, GERADE,   0 }, // W2 sofort gerade
    { 3, ABBIEGEN, 0 }, // W3 sofort abbiegen
    { 8, ABBIEGEN, 1 }, // W8 ab der 1. Auslösung abbiegen
    { 8, GERADE,   2 }  // W8 ab der 2. Auslösung wieder gerade
};

// --------------------------------------------------
// FS1: Folgefahrt ab S2
// Trigger: S2
// Reset  : keiner
// Wirkung ab count >= 2:
//   W1 abbiegen, W3 gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS1_STEPS[] =
{
    { 1, ABBIEGEN, 2 }, // W1 erst ab der 2. Auslösung abbiegen
    { 3, GERADE,   2 }  // W3 erst ab der 2. Auslösung gerade
};

// --------------------------------------------------
// FS2: Folgefahrt ab S4
// Trigger: S4
// Reset  : keiner
// Wirkung:
//   W1 abbiegen, W7 gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS2_STEPS[] =
{
    { 1, ABBIEGEN, 0 }, // W1 sofort abbiegen
    { 7, GERADE,   0 }  // W7 sofort gerade
};

// --------------------------------------------------
// FS3: Folgefahrt ab S7
// Trigger: S7
// Reset  : keiner
// Wirkung:
//   W2 abbiegen, W3 gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS3_STEPS[] =
{
    { 2, ABBIEGEN, 0 }, // W2 sofort abbiegen
    { 3, GERADE,   0 }  // W3 sofort gerade
};

// --------------------------------------------------
// FS4: Bahnhof Ost / alternierende W9-Umschaltung
// Trigger: S8
// Reset  : S10
// Wirkung:
//   count >= 1 : W9 gerade
//   count >= 3 : W9 abbiegen
// --------------------------------------------------
static const WeichenSchaltSchritt FS4_STEPS[] =
{
    { 9, GERADE,   1 }, // W9 ab der 1. Auslösung gerade
    { 9, ABBIEGEN, 3 }  // W9 ab der 3. Auslösung abbiegen
};

const FahrstrassenConfig FAHRSTRASSEN_CONFIG[NUM_STW_FS] =
{
    {
        "FS0 Zufahrt West / W8 alternierend",
        SENSOR_S0,
        { SENSOR_S3, SENSOR_S6 }, 2,
        FS0_STEPS, static_cast<uint8_t>(sizeof(FS0_STEPS) / sizeof(FS0_STEPS[0])),
        "Ausloesung an der Westzufahrt; Reset nach Ueberfahrt S3 oder S6"
    },
    {
        "FS1 Folgefahrt ab S2",
        SENSOR_S2,
        { }, 0,
        FS1_STEPS, static_cast<uint8_t>(sizeof(FS1_STEPS) / sizeof(FS1_STEPS[0])),
        "Ab der 2. Ausloesung W1 abbiegen und W3 gerade"
    },
    {
        "FS2 Folgefahrt ab S4",
        SENSOR_S4,
        { }, 0,
        FS2_STEPS, static_cast<uint8_t>(sizeof(FS2_STEPS) / sizeof(FS2_STEPS[0])),
        "Sofort W1 abbiegen und W7 gerade"
    },
    {
        "FS3 Folgefahrt ab S7",
        SENSOR_S7,
        { }, 0,
        FS3_STEPS, static_cast<uint8_t>(sizeof(FS3_STEPS) / sizeof(FS3_STEPS[0])),
        "Sofort W2 abbiegen und W3 gerade"
    },
    {
        "FS4 Bahnhof Ost / W9 alternierend",
        SENSOR_S8,
        { SENSOR_S10 }, 1,
        FS4_STEPS, static_cast<uint8_t>(sizeof(FS4_STEPS) / sizeof(FS4_STEPS[0])),
        "Ostseite mit Reset an S10; W9 abhaengig vom Zaehlerstand"
    }
};