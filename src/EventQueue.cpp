#include "EventQueue.h"

static constexpr uint8_t QUEUE_SIZE = 32;
static Event    queueBuf[QUEUE_SIZE];
static uint8_t  head = 0;
static uint8_t  tail = 0;
static uint8_t  countEv = 0;

void pushEvent(uint8_t type, uint8_t id, uint8_t value) {
    if (countEv >= QUEUE_SIZE) return;
    queueBuf[head].type  = type;
    queueBuf[head].id    = id;
    queueBuf[head].value = value;
    head = (head + 1) % QUEUE_SIZE;
    countEv++;
}

bool popEvent(Event &ev) {
    if (!countEv) return false;
    ev = queueBuf[tail];
    tail = (tail + 1) % QUEUE_SIZE;
    countEv--;
    return true;
}

uint8_t eventCount() {
    return countEv;
}
