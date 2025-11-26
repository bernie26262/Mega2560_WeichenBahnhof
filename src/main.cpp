#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "SensorHub.h"
#include "Weiche.h"
#include "Bahnhof.h"
#include "SteuerungWeichen.h"
#include "Modus.h"
#include "MegaI2C.h"

// Globale Objekte
SensorHub            sensorHub;
Weiche               weichen[NUM_WEICHEN];
Bahnhof              bahnhoefe[NUM_BHF];
Modus                modusController(PIN_MODUS_TASTER, PIN_LED_AUTOMATIK, PIN_LED_MANUELL,
                                     weichen, NUM_WEICHEN);
SteuerungWeichen     stwController(weichen, STW_DEFS, NUM_STW_FS);

void setup() {
    Serial.begin(115200);
    delay(200);
    DBGLN("\n=== Mega2560_1_WeichenBahnhof START ===");

    // Sensoren
    sensorHub.begin(SENSOR_PINS, NUM_SENS, 3000);

    // Weichen
    for (uint8_t i = 0; i < NUM_WEICHEN; i++) {
        weichen[i].configure(i, WEICHEN_PINS[i]);
        weichen[i].begin();
    }

    // Bahnhöfe
    for (uint8_t i = 0; i < NUM_BHF; i++) {
        bahnhoefe[i].configure(BHF_CONFIGS[i], i);
        bahnhoefe[i].begin();
    }

    // Modus
    modusController.begin();

    // I2C
    megaI2C_begin();

    DBGLN("Setup abgeschlossen.");
}

void loop() {
    modusController.update();
    Betriebsmodus mode = modusController.current();

    // Sensoren verarbeiten
    sensorHub.update([&](uint8_t sIdx){
        if (mode == MOD_AUTOMATIK) {
            stwController.onSensorTrigger(sIdx);
        }

        for (uint8_t b = 0; b < NUM_BHF; b++) {
            bahnhoefe[b].handleSensor(sIdx, mode);
        }
    });

    // Weichen und Bahnhöfe zyklisch pflegen
    for (uint8_t i = 0; i < NUM_WEICHEN; i++) {
        weichen[i].update();
    }
    for (uint8_t i = 0; i < NUM_BHF; i++) {
        bahnhoefe[i].update(mode);
    }

    megaI2C_update();

    delay(1);
}
