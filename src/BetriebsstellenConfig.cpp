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
        SENSOR_S19,
        { SENSOR_S1, SENSOR_S6 }, 2,
        5000,
        "Einfahrt ueber S2, Timerstart ueber S19, Reset ueber S1 oder S6"
    },
    {
        "Bhf1",
        SENSOR_S2,
        SENSOR_S18,
        { SENSOR_S1, SENSOR_S6 }, 2,
        5000,
        "Einfahrt ueber S2, Timerstart ueber S18, Reset ueber S1 oder S6"
    },
    {
        "Bhf2",
        SENSOR_S8,
        SENSOR_S22,
        { SENSOR_S10, 255 }, 1,
        5000,
        "Einfahrt ueber S8, Timerstart ueber S22, Reset ueber S10"
    },
    {
        "Bhf3",
        SENSOR_S8,
        SENSOR_S23,
        { SENSOR_S10, 255 }, 1,
        5000,
        "Einfahrt ueber S8, Timerstart ueber S23, Reset ueber S10"
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
//                zusätzlich: wenn W0 ist=gerade, dann W1 gerade
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
// FS1: Folgefahrt ab S2, alternierend ungerade/gerade
// Trigger: S2
// Reset  : S6
// Wirkung:
// Wird in Fahrstrassen.cpp per Sonderlogik umgesetzt.
// WICHTIG: Bei S2-getriggerter FS1 muss W5 als erste Weiche geschaltet werden.
//   ungerade count : W5 A, danach W1 A, W2 G, W3 G, W6 G, W7 G, W8 G
//   gerade count   : W5 G
// --------------------------------------------------
static const WeichenSchaltSchritt FS1_STEPS[] = {};

// --------------------------------------------------
// FS2: Folgefahrt ab S4
// Trigger: S4
// Reset  : keiner
// Wirkung:
//   W0 abbiegen, W1 abbiegen, W7 gerade, W8 gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS2_STEPS[] =
{
    { 0, ABBIEGEN, 0 }, // W0 sofort abbiegen
    { 1, ABBIEGEN, 0 }, // W1 sofort abbiegen
    { 7, GERADE,   0 }, // W7 sofort gerade
    { 8, GERADE,   0 }  // W8 sofort gerade
};

// --------------------------------------------------
// FS3: Folgefahrt ab S7
// Trigger: S7
// Reset  : keiner
// Wirkung:
//   W0 abbiegen, W2 abbiegen, W3 gerade, W6 abbiegen, W7 abbiegen
// --------------------------------------------------
static const WeichenSchaltSchritt FS3_STEPS[] =
{
    { 0, ABBIEGEN, 0 }, // W0 sofort abbiegen
    { 2, ABBIEGEN, 0 }, // W2 sofort abbiegen
    { 3, GERADE,   0 }, // W3 sofort gerade
    { 6, ABBIEGEN, 0 }, // W6 sofort abbiegen
    { 7, ABBIEGEN, 0 }  // W7 sofort abbiegen
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

// --------------------------------------------------
// FS5: Fahrstraße ab S18
// Trigger: S18
// Reset  : keiner
// Wirkung:
//   W0 gerade
// --------------------------------------------------
static const WeichenSchaltSchritt FS5_STEPS[] =
{
    { 0, GERADE, 0 } // W0 sofort gerade
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
        "FS1 Folgefahrt ab S2 / odd-even",
        SENSOR_S2,
        { SENSOR_S6 }, 1,
        FS1_STEPS, static_cast<uint8_t>(sizeof(FS1_STEPS) / sizeof(FS1_STEPS[0])),
        "Ungerade count: W5 A zuerst, danach W1 A, W2 G, W3 G, W6 G, W7 G, W8 G; gerade count: W5 G; Reset an S6"
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
    },
    {
        "FS5 Folgefahrt ab S18",
        SENSOR_S18,
        { }, 0,
        FS5_STEPS, static_cast<uint8_t>(sizeof(FS5_STEPS) / sizeof(FS5_STEPS[0])),
        "Sofort W0 gerade"
    }
};