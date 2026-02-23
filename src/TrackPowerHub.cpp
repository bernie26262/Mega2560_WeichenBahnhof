#include "TrackPowerHub.h"

void TrackPowerHub::begin()
{
    for (uint8_t i = 0; i < BHF_COUNT; ++i)
    {
        pinMode(BHF_TRACK_POWER_PIN[i], OUTPUT);
        digitalWrite(BHF_TRACK_POWER_PIN[i], HIGH); // Strom AN
    }
}

void TrackPowerHub::setPower(uint8_t bhf, bool on)
{
    if (bhf >= BHF_COUNT) return;
    digitalWrite(BHF_TRACK_POWER_PIN[bhf], on ? LOW : HIGH);
}
