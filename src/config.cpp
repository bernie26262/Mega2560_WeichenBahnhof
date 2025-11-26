#include "config.h"
#include "SteuerungWeichen.h"
#include <EEPROM.h>

/* ----------------------------------------------------
 *  Sensor-Pins (Beispiel)
 *  S0..S10 → Pins 22..32
 * ---------------------------------------------------- */

const uint8_t SENSOR_PINS[NUM_SENS] = {
    22, // S0
    28, // S1
    23, // S2
    33, // S3
    24, // S4
    32, // S5
    25, // S6
    31, // S7
    27, // S8
    35, // S9
    34  // S10
};

/* ----------------------------------------------------
 *  Weichen-Pins (Beispiel)
 *  W0..W11
 *  W6..W11 haben Fahrspannungs-Relais
 * ---------------------------------------------------- */

const WPins WEICHEN_PINS[NUM_WEICHEN] = {
    // pinG, pinA, pinRed, pinRueck, hasRed
    {5,  6,  255, 38, false}, // W0
    {8,  9,  255, 39, false}, // W1
    {10, 11, 255, 40, false}, // W2
    {12, 13, 255, 41, false}, // W3
    {14, 15, 255, 42, false}, // W4
    {16, 17, 255, 43, false}, // W5

    {18, 19, A12, 44, true},  // W6
    {2,  3,  A13, 45, true},  // W7
    {A0, A1, A14, 46, true},  // W8
    {A2, A3, A15, 47, true},  // W9
    {A4 ,A5 , 50, 48, true},  // W10
    {A6, A7,  50, 49, true}   // W11
};

/* Grundstellung:  */

const Richtung WEICHEN_GRUNDSTELLUNG[NUM_WEICHEN] = {
    GERADE,         // W0
    GERADE,         // W1
    GERADE,         // W2
    GERADE,         // W3
    GERADE,         // W4
    ABBIEGEN,       // W5
    ABBIEGEN,       // W6
    GERADE,         // W7
    GERADE,         // W8
    GERADE,         // W9
    ABBIEGEN,       // W10
    ABBIEGEN        // W11
};

/* ----------------------------------------------------
 *  Bahnhöfe (Beispiel-Konfiguration)
 *  Bhf0..Bhf3, wie von dir beschrieben:
 *   Bhf0: Strom A8,  Einfahrtsensor S5,  Timerstart S5
 *   Bhf1: Strom A9,  Einfahrtsensor S3,  Timerstart S3
 *   Bhf2: Strom A10, Einfahrtsensor S9,  Timerstart S9
 *   Bhf3: Strom A11, Einfahrtsensor S10, Timerstart S10
 * ---------------------------------------------------- */

const BahnhofConfig BHF_CONFIGS[NUM_BHF] = {
    { A8,  32,  32,  8000 },  // Bhf0
    { A9,  33,  33,  8000 },  // Bhf1
    { A10, 35,  35,  8000 },  // Bhf2
    { A11, 34,  34,  8000 }   // Bhf3
};

/* ----------------------------------------------------
 *  Fahrstraßen-Definitionen
 *  → FS0, FS1, FS2, FS4 wie zuvor besprochen
 * ---------------------------------------------------- */

// --------- FS0 : Sensor S0, Reset S3 & S6 ----------

static const WeichenSchaltSchritt FS0_STEPS[] = {
    { 2, GERADE,   0 },  // W2 immer GERADE
    { 3, ABBIEGEN, 0 },  // W3 immer ABBIEGEN

    { 8, ABBIEGEN, 1 },  // W8: erste Überfahrt → ABBIEGEN
    { 8, GERADE,   2 }   // W8: ab zweiter Überfahrt → GERADE (>=2)
};

// --------- FS1 : Sensor S2 ----------

static const WeichenSchaltSchritt FS1_STEPS[] = {
    // W1 ABBIEGEN bei 2,4,6
    { 1, ABBIEGEN, 2 },
    { 1, ABBIEGEN, 4 },
    { 1, ABBIEGEN, 6 },

    // W3 GERADE bei 2,4,6
    { 3, GERADE, 2 },
    { 3, GERADE, 4 },
    { 3, GERADE, 6 },

    // W5 alternierend
    { 5, GERADE,   1 },
    { 5, ABBIEGEN, 2 },
    { 5, GERADE,   3 },
    { 5, ABBIEGEN, 4 },
    { 5, GERADE,   5 },
    { 5, ABBIEGEN, 6 },

    // W7 bei geraden Zählern
    { 7, GERADE, 2 },
    { 7, GERADE, 4 },
    { 7, GERADE, 6 },

    // W8 bei geraden Zählern
    { 8, GERADE, 2 },
    { 8, GERADE, 4 },
    { 8, GERADE, 6 }
};

// --------- FS2 : Sensor S4 ----------

static const WeichenSchaltSchritt FS2_STEPS[] = {
    { 1, ABBIEGEN, 0 },  // immer
    { 7, GERADE,   0 }   // immer
};

// --------- FS2 : Sensor S4 ----------

static const WeichenSchaltSchritt FS3_STEPS[] = {
    { 2, ABBIEGEN, 0 },  // immer
    { 3, GERADE,   0 }   // immer
};

// --------- FS4 : Sensor S8, Reset S10 ----------

static const WeichenSchaltSchritt FS4_STEPS[] = {
    { 9, GERADE,   1 },   // 1. Überfahrt
    { 9, ABBIEGEN, 3 }    // 3. Überfahrt
};

// --------- Gesamte SteuerungWeichen-Definitionen ---------

const SteuerungWeichenDefinition STW_DEFS[] = {
    // sensorIndex, resetSensors[], numReset, steps, numSteps
    { 0, {3,6}, 2, FS0_STEPS, (uint8_t)(sizeof(FS0_STEPS)/sizeof(FS0_STEPS[0])) },
    { 2, { },   0, FS1_STEPS, (uint8_t)(sizeof(FS1_STEPS)/sizeof(FS1_STEPS[0])) },
    { 4, { },   0, FS2_STEPS, (uint8_t)(sizeof(FS2_STEPS)/sizeof(FS2_STEPS[0])) },
    { 7, { },   0, FS3_STEPS, (uint8_t)(sizeof(FS3_STEPS)/sizeof(FS3_STEPS[0])) },
    { 8, {10},  1, FS4_STEPS, (uint8_t)(sizeof(FS4_STEPS)/sizeof(FS4_STEPS[0])) }
};

const uint8_t NUM_STW_FS = sizeof(STW_DEFS) / sizeof(STW_DEFS[0]);
