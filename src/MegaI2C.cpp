#include "MegaI2C.h"
#include <Wire.h>

#include "config.h"
#include "EventQueue.h"
#include "Weiche.h"
#include "Bahnhof.h"
#include "SteuerungWeichen.h"
#include "Modus.h"
#include "debug.h"

// Globale Objekte kommen aus main.cpp
extern Weiche           weichen[NUM_WEICHEN];
extern Bahnhof          bahnhoefe[NUM_BHF];
extern Modus            modusController;
extern SteuerungWeichen stwController;

// Protokoll-Konstanten
static const uint8_t PROTO_VERSION = 1;

enum MsgType : uint8_t {
    MSG_FULL  = 1,
    MSG_DELTA = 2
};

enum CmdType : uint8_t {
    CMD_GET_FULL  = 1,
    CMD_GET_DELTA = 2
};

// Letzter vom Master gesetzter Befehl
static volatile uint8_t g_lastCmd = CMD_GET_DELTA;

// I2C-Callbacks
static void onI2CReceive(int len);
static void onI2CRequest();

// Hilfsfunktionen
static void buildFullSnapshot(uint8_t* buf, size_t& len);
static void buildDeltaBatch(uint8_t* buf, size_t& len);

void megaI2C_begin() {
    pinMode(PIN_I2C_INT, OUTPUT);
    digitalWrite(PIN_I2C_INT, HIGH);  // INT inaktiv (HIGH)

    Wire.begin(I2C_SLAVE_ADDR);       // als Slave
    Wire.onReceive(onI2CReceive);
    Wire.onRequest(onI2CRequest);

    DBGLN(String("[MegaI2C] gestartet, Addr=0x") + String(I2C_SLAVE_ADDR, HEX));
}

void megaI2C_update() {
    // INT-Pin low, wenn Events in der Queue sind
    static bool intLow = false;

    if (eventCount() > 0 && !intLow) {
        digitalWrite(PIN_I2C_INT, LOW); // Data ready
        intLow = true;
    } else if (eventCount() == 0 && intLow) {
        digitalWrite(PIN_I2C_INT, HIGH); // nichts mehr zu senden
        intLow = false;
    }
}

// ------------------------------------------------------
// I2C-Callbacks
// ------------------------------------------------------

static void onI2CReceive(int len) {
    if (len <= 0) return;

    uint8_t cmd = Wire.read();
    g_lastCmd = cmd;

    // Rest evtl. verwerfen
    while (Wire.available()) {
        (void)Wire.read();
    }
}

static void onI2CRequest() {
    uint8_t buf[32];
    size_t  len = 0;

    if (g_lastCmd == CMD_GET_FULL) {
        buildFullSnapshot(buf, len);
    } else {
        // Default: DELTA
        buildDeltaBatch(buf, len);
    }

    Wire.write(buf, (uint8_t)len);
}

// ------------------------------------------------------
// Full-Snapshot bauen
// ------------------------------------------------------
//
// Layout (max. 32 Bytes):
//  [0]  = PROTO_VERSION
//  [1]  = MSG_FULL
//  [2]  = Weichen-Bits low  (W0..W7)
//  [3]  = Weichen-Bits high (W8..W11 in lower Bits)
//  [4]  = BHF-Bits (B0..B3)
//  [5]  = Modus (0=AUTOMATIK,1=MANUELL)
//  [6]  = numFS
//  [7+] = für jede FS: counter low, counter high
//
// Bei deinen aktuellen Werten bleibt das deutlich < 32 Bytes.
//

static void buildFullSnapshot(uint8_t* buf, size_t& len) {
    len = 0;
    buf[len++] = PROTO_VERSION;
    buf[len++] = MSG_FULL;

    // Weichen-Bits:
    // Bit i = 1 wenn Weiche i in Abzweig-Stellung (laut Rückmelder)
    uint16_t wBits = 0;
    for (uint8_t i = 0; i < NUM_WEICHEN; i++) {
        bool abzweig = weichen[i].istAbzweig();
        if (abzweig) {
            wBits |= (1u << i);
        }
    }
    buf[len++] = (uint8_t)(wBits & 0xFF);
    buf[len++] = (uint8_t)(wBits >> 8);

    // Bahnhofs-Strom-Bits:
    // Bit i = 1 wenn Bahnhof i Strom AN
    uint8_t bBits = 0;
    for (uint8_t i = 0; i < NUM_BHF; i++) {
        if (bahnhoefe[i].stromIstAn()) {
            bBits |= (1u << i);
        }
    }
    buf[len++] = bBits;

    // Modus
    buf[len++] = (uint8_t)modusController.current();

    // Fahrstraßen-Counter
    uint16_t counters[16];
    stwController.getCounters(counters, 16);

    buf[len++] = NUM_STW_FS;   // Anzahl gültiger Counter

    for (uint8_t i = 0; i < NUM_STW_FS; i++) {
        if (len + 2 >= 32) break; // Sicherheit, falls später erweitert wird
        uint16_t c = counters[i];
        buf[len++] = (uint8_t)(c & 0xFF);
        buf[len++] = (uint8_t)(c >> 8);
    }
}

// ------------------------------------------------------
// Delta-Batch bauen (Gruppierung light)
// ------------------------------------------------------
//
// Layout:
//  [0] = PROTO_VERSION
//  [1] = MSG_DELTA
//  [2] = N (Anzahl Events)
//  dann N * 3 Bytes:
//    [type, id, value] (wie Event-Struct)
//
// Maximal 8 Events pro Batch => 3 + 8*3 = 27 Bytes < 32.
//

static void buildDeltaBatch(uint8_t* buf, size_t& len) {
    len = 0;
    buf[len++] = PROTO_VERSION;
    buf[len++] = MSG_DELTA;

    uint8_t countIndex = (uint8_t)len;
    buf[len++] = 0; // Platzhalter für N

    static const uint8_t MAX_EVENTS_PER_BATCH = 8;

    uint8_t n = 0;
    Event ev;

    while (n < MAX_EVENTS_PER_BATCH && eventCount() > 0) {
        if (!popEvent(ev)) break;

        if (len + 3 >= 32) {
            // sollte eigentlich nicht passieren, aber zur Sicherheit
            break;
        }

        buf[len++] = ev.type;
        buf[len++] = ev.id;
        buf[len++] = ev.value;

        n++;
    }

    buf[countIndex] = n;
}
