#pragma once
#include <Arduino.h>
#include <Wire.h>

void i2cSlaveBegin(uint8_t address);
void i2cOnReceive(int len);
void i2cOnRequest();