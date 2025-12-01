// src/MegaI2C.cpp
#include "MegaI2C.h"

#include <Wire.h>
#include "config.h"
#include "EventQueue.h"
#include "Weiche.h"
#include "Bahnhof.h"
#include "Modus.h"
#include "i2c_packets.h"

// Diese Objekte kommen aus main.cpp
extern Weiche weichen[NUM_WEICHEN];
extern Bahnhof bahnhoefe[NUM_BHF];
extern Modus modusController;

// Protokollversion (kannst du später erhöhen, wenn sich das Layout ändert)
static constexpr uint8_t PROTO_VERSION = 1;

// DELTA-Gruppierung "light": max. so viele Events pro Paket,
// dass wir sicher im 32-Byte-I2C-Buffer bleiben.
static constexpr uint8_t MAX_DELTA_EVENTS_PER_PACKET = 8;

// Letztes vom ESP empfangenes Kommando
static volatile uint8_t g_lastCommand = I2C_CMD_NOP;

// --- Hilfsfunktionen -------------------------------------------------

// Liefert Bitmaske der aktuellen Weichenstellung aus Rückmeldern:
// 0 = GERADE, 1 = ABBIEGEN, bitweise W0..W11
static void buildWeichenBits(uint8_t &lo, uint8_t &hi) {
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; i++) {
        bool abzweig = weichen[i].istAbzweig();
        if (abzweig) {
            bits |= (1u << i);
        }
    }
    lo = bits & 0xFF;
    hi = (bits >> 8) & 0xFF;
}

// Liefert Bitmaske der Bahnhofs-Stromzustände
// Wir lesen direkt die Relais-Pins aus BHF_CONFIGS:
//   LOW = Relais AN = Strom AUS? oder AN?
// In deiner bisherigen Logik (Bahnhof::setStrom):
//   digitalWrite(_stromPin, an ? LOW : HIGH); // low-aktiv
// → LOW  = Strom AN
// → HIGH = Strom AUS
static uint8_t buildBahnhofBits() {
    uint8_t bits = 0;
    for (uint8_t i = 0; i < NUM_BHF; i++) {
        uint8_t pin = BHF_CONFIGS[i].stromPin;
        if (pin == 255) continue;
        pinMode(pin, INPUT_PULLUP); // zur Sicherheit (sollte bereits OUTPUT sein)
        int level = digitalRead(pin);
        bool stromAn = (level == LOW); // low-aktiv = Strom AN
        if (stromAn) {
            bits |= (1u << i);
        }
    }
    return bits;
}

// Befüllt FullState-Payload mit aktuellem Zustand
static void buildFullState(I2C_FullStatePayload &p) {
    p.protoVersion = PROTO_VERSION;

    p.numWeichen = NUM_WEICHEN;
    buildWeichenBits(p.weichenStateLo, p.weichenStateHi);

    p.numBahnhof   = NUM_BHF;
    p.bhfStromBits = buildBahnhofBits();

    // Modus: 0 = AUTOMATIK, 1 = MANUELL (entspricht deinem enum)
    p.modus = static_cast<uint8_t>(modusController.current());
}

// --- I2C ISR-Callbacks -----------------------------------------------

static void onI2CReceive(int numBytes) {
    if (numBytes <= 0) return;

    // Erstes Byte ist das Kommando
    uint8_t cmd = Wire.read();
    g_lastCommand = cmd;

    // Übrige Bytes (falls vorhanden) ignorieren wir vorerst
    while (Wire.available()) {
        (void)Wire.read();
    }
}

static void onI2CRequest() {
    uint8_t buf[32];
    uint8_t idx = 0;

    I2C_Command cmd = static_cast<I2C_Command>(g_lastCommand);

    if (cmd == I2C_CMD_GET_FULL) {
        // --- FULL Snapshot ------------------------------------------------
        I2C_FullStatePayload payload;
        buildFullState(payload);

        buf[0] = I2C_PKT_FULL;
        buf[1] = sizeof(I2C_FullStatePayload); // payloadLen

        // Payload direkt dahinter kopieren
        memcpy(&buf[2], &payload, sizeof(I2C_FullStatePayload));
        idx = 2 + sizeof(I2C_FullStatePayload);

    } else if (cmd == I2C_CMD_GET_DELTA) {
        // --- DELTA: Events gruppiert (light) -----------------------------

        buf[0] = I2C_PKT_DELTA;
        buf[1] = 0;       // payloadLen (placeholder)
        buf[2] = 0;       // numEvents (wird am Ende gesetzt)
        idx    = 3;

        uint8_t numEvents = 0;
        Event ev;

        while (numEvents < MAX_DELTA_EVENTS_PER_PACKET && popEvent(ev)) {
            buf[idx++] = ev.type;
            buf[idx++] = ev.id;
            buf[idx++] = ev.value;
            numEvents++;
        }

        uint8_t payloadLen = 1 + numEvents * 3; // 1 für numEvents + 3 pro Event
        buf[1] = payloadLen;
        buf[2] = numEvents;
        idx    = 2 + payloadLen;

        // Wenn du willst, kannst du auch bei numEvents==0 auf I2C_PKT_NONE
        // umschwenken; aktuell schicken wir DELTA mit 0 Events.

    } else if (cmd == I2C_CMD_GET_META) {
        // Noch nicht wirklich genutzt, Beispiel-Payload
        buf[0] = I2C_PKT_META;
        buf[1] = 2;  // z.B. 2 Bytes Payload
        buf[2] = PROTO_VERSION;
        buf[3] = NUM_WEICHEN; // als simple Info
        idx = 4;

    } else {
        // Unbekanntes Kommando
        buf[0] = I2C_PKT_ERROR;
        buf[1] = 1;
        buf[2] = 0x01; // "unknown command"
        idx = 3;
    }

    Wire.write(buf, idx);
}

// --- Öffentliche API --------------------------------------------------

void megaI2C_begin() {
    pinMode(PIN_I2C_INT, OUTPUT);
    digitalWrite(PIN_I2C_INT, LOW); // zunächst kein DataReady

    // Mega als I2C-Slave mit Adresse I2C_SLAVE_ADDR
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onReceive(onI2CReceive);
    Wire.onRequest(onI2CRequest);

    g_lastCommand = I2C_CMD_NOP;
}

void megaI2C_update() {
    // DataReady-Pin: HIGH, solange Events in der Queue sind
    if (eventCount() > 0) {
        digitalWrite(PIN_I2C_INT, HIGH);
    } else {
        digitalWrite(PIN_I2C_INT, LOW);
    }

    // hier könnten später weitere Dinge passieren (Timeouts, Statistiken, ...)
}
