#pragma once
#include <Arduino.h>
#include "types.h"
#include "BetriebsstellenConfig.h"

// ---------------------------------------------------------------------------
//  Mega1 Pin-/Index-Konstanten
// ---------------------------------------------------------------------------

// Data-Ready-Pin Richtung ESP32
constexpr uint8_t PIN_DATA_READY = 51;

// Anzahl aller logisch verwendeten Sensor-Indizes.
// Wichtig: Einzelne Sensoren können mehrere logische Funktionen haben
// (z.B. Fahrstraße + Bahnhof-Timerstart). Die fachliche Zuordnung erfolgt
// zentral über die Bahnhofs-Konfiguration weiter unten.
constexpr uint8_t NUM_SENSORS = 24;

// SENSOR_PINS[index] -> Arduino-Pin oder -1 (ungenutzt)
extern const int8_t SENSOR_PINS[NUM_SENSORS];

// Symbolische Indizes für Schaltgleise / Kontakte (S0–S23, soweit belegt
enum SensorIndex : uint8_t
{
    SENSOR_S0  = 0,  // Pin 22 - Fahrstraße
    SENSOR_S1  = 1,  // Pin 28 - Fahrstraße
    SENSOR_S2  = 2,  // Pin 23 - Bhf0/1 Einfahrt
    SENSOR_S3  = 3,  // Pin 33 - Fahrstraße
    SENSOR_S4  = 4,  // Pin 24 - Fahrstraße
    SENSOR_S5  = 5,  // Pin 32 - Fahrstraße
    SENSOR_S6  = 6,  // Pin 25 - Fahrstraße
    SENSOR_S7  = 7,  // Pin 31 - Fahrstraße
    SENSOR_S8  = 8,  // Pin 27 - Bhf2/3 Einfahrt
    SENSOR_S9  = 9,  // Pin 35 - Fahrstraße
    SENSOR_S10 = 10, // Pin 34 - S10 Fahrstraße
    SENSOR_S18 = 18, // Pin 29 - Zusatzkontakt
    SENSOR_S19 = 19, // Pin 26 - Zusatzkontakt
    SENSOR_S22 = 22, // Pin 30 - Zusatzkontakt
    SENSOR_S23 = 23  // Pin 36 - Zusatzkontakt
};

// ---------------------------------------------------------------------------
//  Weichen-Pins
// ---------------------------------------------------------------------------

struct WPins
{
    uint8_t pinG;      // Gerade
    uint8_t pinA;      // Abzweig
    uint8_t pinRed;    // Reduktions-Pin (oder 255 falls keiner)
    uint8_t pinRueck;  // Rückmeldekontakt
    bool    hasRed;    // true, wenn Reduktions-Pin verwendet wird
};

constexpr uint8_t NUM_WEICHEN = 12;

extern const WPins WEICHEN_PINS[NUM_WEICHEN];

// ----------------------------------------------------
// Bahnhof – Stromgleis (LOW-LEVEL-Relais)
// LOW  = Strom AUS
// HIGH = Strom AN
// ----------------------------------------------------

constexpr uint8_t BHF_TRACK_POWER_PIN[BHF_COUNT] =
{
    A8,   // Bhf0
    A9,   // Bhf1
    A10,  // Bhf2
    A11   // Bhf3
};

// --------------------------------------------------
// Weichen-Grundstellung
// --------------------------------------------------

const Richtung WEICHEN_GRUNDSTELLUNG[NUM_WEICHEN] =
{
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


// Hilfsfunktion: Pins initialisieren
void initPinsMega1();