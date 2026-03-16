#include "pins_mega1.h"

// ---------------------------------------------------------------------------
// Sensor-Mapping (Index -> Pin)
// -1 bedeutet "kein Sensor an diesem Index"
// ---------------------------------------------------------------------------

const int8_t SENSOR_PINS[NUM_SENSORS] =
{
    // 0  bis 10: Schaltgleise S0–S10
    22, // 0  - S0  Fahrstraße
    28, // 1  - S1  Fahrstraße
    23, // 2  - S2  Bhf0/1 Einfahrt
    33, // 3  - S3  Fahrstraße
    24, // 4  - S4  Fahrstraße
    32, // 5  - S5  Fahrstraße
    25, // 6  - S6  Fahrstraße
    31, // 7  - S7  Fahrstraße
    27, // 8  - S8  Bhf2/3 Einfahrt
    35, // 9  - S9  Fahrstraße
    34, // 10 - S10 Fahrstraße

    // 11–23: aktuell unbenutzt
    -1, // 11
    -1, // 12
    -1, // 13
    -1, // 14
    -1, // 15
    -1, // 16
    -1, // 17
    -1, // 18
    -1, // 19
    -1, // 20
    -1, // 21
    -1, // 22
    -1  // 23
};

const BahnhofSensorConfig BHF_SENSOR_CONFIG[BHF_COUNT] =
{
    { SENSOR_S2, SENSOR_S5  }, // Bhf 0: Einfahrt S2, Timerstart S5
    { SENSOR_S2, SENSOR_S3  }, // Bhf 1: Einfahrt S2, Timerstart S3
    { SENSOR_S8, SENSOR_S9  }, // Bhf 2: Einfahrt S8, Timerstart S9
    { SENSOR_S8, SENSOR_S10 }  // Bhf 3: Einfahrt S8, Timerstart S10
};

// Weichen-Pins gemäß deiner Vorgabe
const WPins WEICHEN_PINS[NUM_WEICHEN] =
{
    // pinG, pinA, pinRed, pinRueck, hasRed
    {5,  6,  255, 38, false}, // W0
    {8,  9,  255, 39, false}, // W1
    {10, 11, 255, 40, false}, // W2
    {12, 7,  255, 41, false}, // W3 (A von D13 -> D7)
    {14, 15, 255, 42, false}, // W4
    {16, 17, 255, 43, false}, // W5

    {18, 19, A12, 44, true},  // W6
    {2,  3,  A13, 45, true},  // W7
    {A0, A1, A14, 46, true},  // W8
    {A2, A3, A15, 47, true},  // W9
    {A4, A5,  50, 48, true},  // W10 (Reduktions-Pin 50)
    {A6, A7,  50, 49, true}   // W11 (Reduktions-Pin 50, shared mit W10)
};

void initPinsMega1()
{
    // DataReady
    pinMode(PIN_DATA_READY, OUTPUT);
    digitalWrite(PIN_DATA_READY, LOW);

    // Sensoren als INPUT_PULLUP (alle gültigen Indizes > -1)
    for (uint8_t i = 0; i < NUM_SENSORS; ++i)
    {
        if (SENSOR_PINS[i] >= 0)
        {
            pinMode(SENSOR_PINS[i], INPUT_PULLUP);
        }
    }

    // Weichen-Pins
    for (uint8_t i = 0; i < NUM_WEICHEN; ++i)
    {
        pinMode(WEICHEN_PINS[i].pinG, OUTPUT);
        pinMode(WEICHEN_PINS[i].pinA, OUTPUT);
        digitalWrite(WEICHEN_PINS[i].pinG, HIGH);
        digitalWrite(WEICHEN_PINS[i].pinA, HIGH);

        if (WEICHEN_PINS[i].hasRed && WEICHEN_PINS[i].pinRed != 255)
        {
            pinMode(WEICHEN_PINS[i].pinRed, OUTPUT);
            digitalWrite(WEICHEN_PINS[i].pinRed, HIGH); // Reduktion aus
        }

        pinMode(WEICHEN_PINS[i].pinRueck, INPUT_PULLUP);
    }
}
