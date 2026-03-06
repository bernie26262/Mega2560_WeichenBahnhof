#include "TrackPowerHub.h"

void TrackPowerHub::begin()
{
    for (uint8_t i = 0; i < BHF_COUNT; ++i)
    {
        pinMode(BHF_TRACK_POWER_PIN[i], OUTPUT);
        digitalWrite(BHF_TRACK_POWER_PIN[i], HIGH); // default: AUS (active-low)
    }
    
#ifdef DEBUG_SERIAL
    Serial.print(F("[M1PWR] begin pins: "));
    for (uint8_t i = 0; i < BHF_COUNT; ++i) {
        Serial.print(F("BHF")); Serial.print(i);
        Serial.print(F("="));
        Serial.print(digitalRead(BHF_TRACK_POWER_PIN[i]) == HIGH ? F("HIGH") : F("LOW"));
        if (i + 1 < BHF_COUNT) Serial.print(F(" "));
    }
    Serial.println();
#endif
}

void TrackPowerHub::setPower(uint8_t bhf, bool on)
{
    if (bhf >= BHF_COUNT) return;
   
#ifdef DEBUG_SERIAL
    Serial.print(F("[M1PWR] setPower bhf=")); Serial.print(bhf);
    Serial.print(F(" on=")); Serial.print(on ? F("1") : F("0"));
    Serial.print(F(" before="));
    Serial.print(digitalRead(BHF_TRACK_POWER_PIN[bhf]) == HIGH ? F("HIGH") : F("LOW"));
    Serial.println();
#endif

    digitalWrite(BHF_TRACK_POWER_PIN[bhf], on ? LOW : HIGH);
    
#ifdef DEBUG_SERIAL
    Serial.print(F("[M1PWR] setPower bhf=")); Serial.print(bhf);
    Serial.print(F(" after="));
    Serial.print(digitalRead(BHF_TRACK_POWER_PIN[bhf]) == HIGH ? F("HIGH") : F("LOW"));
    Serial.println();
#endif
}
