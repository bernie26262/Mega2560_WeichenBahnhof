// EventQueue.h
#pragma once
#include <Arduino.h>

enum EventType : uint8_t {
    EVT_NONE          = 0,
    EVT_SENSOR        = 1,
    EVT_WEICHE        = 2,
    EVT_BHF           = 3,
    EVT_MODUS_CHANGE  = 4,
    EVT_RECOVERY_DONE = 5,
    EVT_FS_COUNTER    = 6    // NEU: Fahrstraßen-Counter-Änderung (optional genutzt)
};

struct Event {
    uint8_t type;
    uint8_t id;
    uint8_t value;
};

void   pushEvent(uint8_t type, uint8_t id, uint8_t value);
bool   popEvent(Event &ev);
uint8_t eventCount();

