// include/MegaI2C.h
#pragma once
#include <Arduino.h>

/**
 * Initialisiert den Mega als I2C-Slave:
 *  - Wire.begin(I2C_SLAVE_ADDR)
 *  - registriert onReceive/onRequest
 *  - setzt den DataReady-Pin auf LOW
 */
void megaI2C_begin();

/**
 * Muss zyklisch aus loop() aufgerufen werden.
 *  - pflegt den DataReady-Pin (HIGH, solange Events in der Queue sind)
 *  - kann später erweitert werden (Watchdogs, Statistiken etc.)
 */
void megaI2C_update();
