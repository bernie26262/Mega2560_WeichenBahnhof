#include "MegaI2C.h"
#include <Wire.h>
#include "config.h"
#include "EventQueue.h"
#include "Modus.h"

extern Modus modusController;

static void onRequest();
static void onReceive(int len);

void megaI2C_begin() {
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onRequest(onRequest);
    Wire.onReceive(onReceive);

    pinMode(PIN_I2C_INT, OUTPUT);
    digitalWrite(PIN_I2C_INT, LOW);
}

void megaI2C_update() {
    // INT setzen, wenn Events vorliegen
    if (eventCount() > 0) {
        digitalWrite(PIN_I2C_INT, HIGH);
    } else {
        digitalWrite(PIN_I2C_INT, LOW);
    }
}

static void onRequest() {
    Event ev;
    if (popEvent(ev)) {
        Wire.write(ev.type);
        Wire.write(ev.id);
        Wire.write(ev.value);
    } else {
        // keine Events → Systemstatus senden
        Wire.write((uint8_t)0);                         // type 0 = Status
        Wire.write((uint8_t)modusController.current()); // modus
        Wire.write((uint8_t)0);                         // reserved
    }
}

static void onReceive(int len) {
    // Platz für zukünftige Kommandos vom ESP32
    while (Wire.available()) {
        (void)Wire.read();
    }
}
