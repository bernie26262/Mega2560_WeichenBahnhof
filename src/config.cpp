#include "config.h"
#include "SteuerungWeichen.h"
#include <EEPROM.h>

/* ----------------------------------------------------
 *  Sensor-Pins (Beispiel)
 *  S0..S10 → Pins 22..32
 * ---------------------------------------------------- */

const uint8_t SENSOR_PINS[NUM_SENS] = {
    22, // S0
    23, // S1
    24, // S2
    25, // S3
    26, // S4
    27, // S5
    28, // S6
    29, // S7
    30, // S8
    31, // S9
    32  // S10
};

/* ----------------------------------------------------
 *  Weichen-Pins (Beispiel)
 *  W0..W11
 *  W6..W11 haben Fahrspannungs-Relais
 * ---------------------------------------------------- */

const WPins WEICHEN_PINS[NUM_WEICHEN] = {
    // pinG, pinA, pinRed, pinRueck, hasRed
    {40, 41, 255, 33, false}, // W0
    {42, 43, 255, 34, false}, // W1
    {44, 45, 255, 35, false}, // W2
    {46, 47, 255, 36, false}, // W3
    {52, 53, 255, 37, false}, // W4
    {A0, A1, 255, 38, false}, // W5

    {A2, A3, A8,  39, true},  // W6
    {A4, A5, A9,  40, true},  // W7
    {A6, A7, A10, 41, true},  // W8
    {A12,A13,A11, 42, true},  // W9
    {A14,A15, A9, 43, true},  // W10
    {A6, A7,  A8, 44, true}   // W11
};

/* Grundstellung: alle GERADE (Beispiel) */

const Richtung WEICHEN_GRUNDSTELLUNG[NUM_WEICHEN] = {
    GERADE, GERADE, GERADE, GERADE,
    GERADE, GERADE, GERADE, GERADE,
    GERADE, GERADE, GERADE, GERADE
};

/* ----------------------------------------------------
 *  Bahnhöfe (Beispiel-Konfiguration)
 *  Bhf0..Bhf3, wie von dir beschrieben:
 *   Bhf0: Strom A8,  Einfahrtsensor S0, Timerstart S7
 *   Bhf1: Strom A9,  Einfahrtsensor S4, Timerstart S8
 *   Bhf2: Strom A10, Einfahrtsensor S6, Timerstart S9
 *   Bhf3: Strom A11, Einfahrtsensor S6, Timerstart S10
 * ---------------------------------------------------- */

const BahnhofConfig BHF_CONFIGS[NUM_BHF] = {
    { A8,  0,  7,  8000 },  // Bhf0
    { A9,  4,  8,  8000 },  // Bhf1
    { A10, 6,  9,  8000 },  // Bhf2
    { A11, 6, 10,  8000 }   // Bhf3
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
    { 8, {10},  1, FS4_STEPS, (uint8_t)(sizeof(FS4_STEPS)/sizeof(FS4_STEPS[0])) }
};

const uint8_t NUM_STW_FS = sizeof(STW_DEFS) / sizeof(STW_DEFS[0]);
