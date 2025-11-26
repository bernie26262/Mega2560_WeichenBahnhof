#define VERSION "v1.1-dev"
#pragma once
#include <Arduino.h>

/* ----------------------------------------------------
 *  Grundtypen
 * ---------------------------------------------------- */

enum Richtung : uint8_t {
    GERADE   = 0,
    ABBIEGEN = 1
};

enum Betriebsmodus : uint8_t {
    MOD_AUTOMATIK = 0,
    MOD_MANUELL   = 1
};

/* ----------------------------------------------------
 *  I2C / System
 * ---------------------------------------------------- */

constexpr uint8_t I2C_SLAVE_ADDR = 0x10;
constexpr uint8_t PIN_I2C_INT    = 48;   // INT-Pin zum ESP32

/* ----------------------------------------------------
 *  Modus / LEDs / Taster
 * ---------------------------------------------------- */

constexpr uint8_t PIN_MODUS_TASTER   = 36;
constexpr uint8_t PIN_LED_AUTOMATIK  = 4;
constexpr uint8_t PIN_LED_MANUELL    = 5;

/* ----------------------------------------------------
 *  EEPROM-Bereich
 * ---------------------------------------------------- */

// 12 Weichen → 12 Bytes für Zustände
constexpr int EEPROM_WEICHEN_BASE = 0;   // Adresse für W0..W11

/* ----------------------------------------------------
 *  Sensoren
 *  LOW-aktiv, entprellt über SensorHub
 * ---------------------------------------------------- */

constexpr uint8_t NUM_SENS = 11;   // S0..S10

extern const uint8_t SENSOR_PINS[NUM_SENS];

/* ----------------------------------------------------
 *  Weichen
 * ---------------------------------------------------- */

constexpr uint8_t NUM_WEICHEN = 12;  // W0..W11

struct WPins {
    uint8_t pinG;      // Relais GERADE (low-aktiv)
    uint8_t pinA;      // Relais ABBIEGEN (low-aktiv)
    uint8_t pinRed;    // Relais Fahrspannungs-Reduktion (low-aktiv, optional)
    uint8_t pinRueck;  // Rückmelder (LOW = abbiegen)
    bool    hasRed;    // true, wenn pinRed verwendet wird
};

extern const WPins     WEICHEN_PINS[NUM_WEICHEN];
extern const Richtung  WEICHEN_GRUNDSTELLUNG[NUM_WEICHEN];

/* ----------------------------------------------------
 *  Bahnhöfe
 * ---------------------------------------------------- */

struct BahnhofConfig {
    uint8_t stromPin;         // low-aktives Relais für Bahnhofsgleis
    uint8_t sensorEinfahrt;   // Sensor-Index zum stromlos schalten
    uint8_t sensorTimerStart; // Sensor-Index zum Timerstart
    unsigned long haltezeitMs;
};

constexpr uint8_t NUM_BHF = 4;
extern const BahnhofConfig BHF_CONFIGS[NUM_BHF];

/* ----------------------------------------------------
 *  Fahrstraßen / SteuerungWeichen
 * ---------------------------------------------------- */

struct WeichenSchaltSchritt;
struct SteuerungWeichenDefinition;

extern const SteuerungWeichenDefinition STW_DEFS[];
extern const uint8_t NUM_STW_FS;
