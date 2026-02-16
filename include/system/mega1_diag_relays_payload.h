#pragma once
#include <stdint.h>
#include <stddef.h> // offsetof
// NOTE: keep <= 32 bytes for single I2C frame

// =====================================================
// Mega1 Diagnose: Relais/Outputs Snapshot (read-only, <= 32 Bytes, V1)
// CMD: 0xD2 (CMD_GET_RELAYS)  (Master schreibt 1 Byte CMD, danach Read)
// =====================================================
//
// Bit i (0..11):
//  - weicheGMask: pinG ist HIGH
//  - weicheAMask: pinA ist HIGH
//  - weicheRedMask: pinRed ist HIGH (nur wenn hasRed, sonst 0)
//
// bhfPowerMask Bits (0..3):
//  - 1 = Stromgleis AN (Pin HIGH), 0 = AUS (Pin LOW)
//
// flags:
//  bit0: valid
//
#pragma pack(push, 1)
struct Mega1DiagRelaysV1
{
    uint8_t  version;      // = 1
    uint8_t  flags;        // bit0: valid
    uint8_t  seq;          // increments only on change (wrap ok)
    uint8_t  reserved0;

    uint16_t weicheGMask;      // Bit i: Gerade-Relais Ausgang HIGH
    uint16_t weicheAMask;      // Bit i: Abzweig-Relais Ausgang HIGH
    uint16_t weicheRedMask;    // Bit i: Reduktions-Relais Ausgang HIGH (falls vorhanden)

    uint8_t  bhfPowerMask;     // Bit i (0..3): BHF power HIGH
    uint8_t  reserved1;

    uint16_t uptime16;         // uptime/100ms (wrap ok)
};
#pragma pack(pop)

static_assert(sizeof(Mega1DiagRelaysV1) <= 32, "Mega1DiagRelaysV1 must fit into a single I2C frame (<=32B).");