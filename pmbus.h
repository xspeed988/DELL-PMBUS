#pragma once
#include "config.h"

// Primitives
uint8_t  pmbus_read_byte(uint8_t reg);
bool     pmbus_read_word(uint8_t reg, uint16_t &out);
String   pmbus_read_string(uint8_t reg);

// Decoders
float    linear11(uint16_t data);
float    decode_vout(uint16_t raw);

// High-level
bool     scan_psu();
void     read_mfr();
void     read_limits();

// Fast path: VIN/IIN/PIN/VOUT/IOUT/POUT only
bool     read_psu_fast(PSUData &d);

// Slow path: temps, fan, status registers
void     read_psu_slow(PSUData &d);
