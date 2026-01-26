#pragma once
#include <Arduino.h>

// --------------------------------------------------
// I2C Grundeinstellungen
// --------------------------------------------------
constexpr uint8_t I2C_ADDR_MEGA1 = 0x10;

// Mega1 DataReady / Pending (DRDY)
// --------------------------------------------------
constexpr uint8_t  CMD_GET_PENDING_MASK = 0xE0; // read-only: returns uint16_t pendingMask
constexpr uint16_t M1_PEND_STATUS       = 0x0001;
constexpr uint16_t M1_PEND_DIAG         = 0x0002;

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

// Weichen-Selbsttest starten (optional/explicit; payload: [cmd])
constexpr uint8_t CMD_START_SELFTEST = 0x07;

// Diagnose (read-only snapshot, <=32B)
constexpr uint8_t CMD_GET_DIAG     = 0xD1;

// --------------------------------------------------
// Error Flags (Payload)
// --------------------------------------------------
constexpr uint8_t ERR_WEICHE = 0x01;

