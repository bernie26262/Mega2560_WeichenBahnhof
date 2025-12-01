// include/i2c_packets.h
#pragma once
#include <Arduino.h>

/**
 * I2C-Kommandos vom Master (ESP32) an den Slave (Mega)
 */
enum I2C_Command : uint8_t {
    I2C_CMD_NOP       = 0x00,
    I2C_CMD_GET_FULL  = 0x01,  // kompletter Snapshot
    I2C_CMD_GET_DELTA = 0x02,  // Deltas (Event-Gruppierung light)
    I2C_CMD_GET_META  = 0x03   // optional: Metadaten (aktuell nicht genutzt)
};

/**
 * Packet-Typen, die der Mega an den ESP32 zurücksendet
 */
enum I2C_PacketType : uint8_t {
    I2C_PKT_NONE  = 0x00,  // kein Inhalt
    I2C_PKT_FULL  = 0x10,  // Full-Snapshot
    I2C_PKT_DELTA = 0x11,  // Deltas (eine Gruppe Events)
    I2C_PKT_META  = 0x12,  // Metadaten (optional)
    I2C_PKT_ERROR = 0x1F   // Fehler
};

/**
 * Gemeinsamer Header jedes I2C-Pakets:
 *  byte 0: packetType  (I2C_PacketType)
 *  byte 1: payloadLen  (Anzahl der folgenden Nutzdatenbytes)
 */
struct __attribute__((packed)) I2C_Header {
    uint8_t packetType;
    uint8_t payloadLen;
};

/**
 * FULL-Snapshot-Payload (klein und kompakt gehalten)
 * Layout:
 *  byte 0: protoVersion
 *  byte 1: numWeichen
 *  byte 2: weichenStateLo  (Bits 0..7: W0..W7, 0=GERADE,1=ABBIEGEN)
 *  byte 3: weichenStateHi  (Bits 0..3: W8..W11)
 *  byte 4: numBahnhof
 *  byte 5: bhfStromBits    (Bits 0..3: Bhf0..Bhf3, 1=Strom AN)
 *  byte 6: modus           (Betriebsmodus; 0=AUTO,1=MANUELL)
 */
struct __attribute__((packed)) I2C_FullStatePayload {
    uint8_t protoVersion;
    uint8_t numWeichen;
    uint8_t weichenStateLo;
    uint8_t weichenStateHi;
    uint8_t numBahnhof;
    uint8_t bhfStromBits;
    uint8_t modus;
};

/**
 * Ein einzelnes Delta-Event (entspricht EventQueue-Event)
 * type:  EventType (EVT_SENSOR, EVT_WEICHE, EVT_BHF, EVT_MODUS_CHANGE, EVT_RECOVERY_DONE)
 * id:    Index (z.B. Weichennummer, Bahnhofsnummer, Sensorindex, 0 bei MODUS)
 * value: je nach Typ interpretierbar (z.B. Stellung, AN/AUS, Modus)
 */
struct __attribute__((packed)) I2C_DeltaItem {
    uint8_t type;
    uint8_t id;
    uint8_t value;
};
