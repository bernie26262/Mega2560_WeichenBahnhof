#pragma once
#include <Arduino.h>

// --------------------------------------------------
// I2C Grundeinstellungen
// --------------------------------------------------
constexpr uint8_t I2C_ADDR_MEGA1 = 0x10;

// --------------------------------------------------
// I2C Kommandos
// --------------------------------------------------
constexpr uint8_t CMD_GET_STATUS   = 0x01;
constexpr uint8_t CMD_SET_MODE     = 0x02;
constexpr uint8_t CMD_SET_WEICHE   = 0x03;
constexpr uint8_t CMD_RELEASE_BHF  = 0x04;
constexpr uint8_t CMD_ACK_ERROR    = 0x05;

// Bahnhof-Stromgleise/Signale (0..3) EIN/AUS
constexpr uint8_t CMD_SET_BHF_POWER = 0x06; // payload: [cmd,bhf(0..3),on(0/1)]

// Diagnose (read-only snapshot, <=32B)
constexpr uint8_t CMD_GET_DIAG     = 0xD1;

// --------------------------------------------------
// Error Flags (Payload)
// --------------------------------------------------
constexpr uint8_t ERR_WEICHE = 0x01;
