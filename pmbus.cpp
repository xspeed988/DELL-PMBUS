#include "pmbus.h"
#include <math.h>

uint8_t pmbus_read_byte(uint8_t reg) {
    Wire.beginTransmission(PSU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)PSU_ADDR, (uint8_t)1, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

bool pmbus_read_word(uint8_t reg, uint16_t &out) {
    Wire.beginTransmission(PSU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)PSU_ADDR, (uint8_t)2, (uint8_t)1);
    if (Wire.available() < 2) return false;
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    out = ((uint16_t)hi << 8) | lo;
    return true;
}

String pmbus_read_string(uint8_t reg) {
    uint8_t len = pmbus_read_byte(reg);
    if (len == 0 || len > 32) return "";
    Wire.beginTransmission(PSU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(PSU_ADDR, (int)len + 1, 1);
    Wire.read(); 
    String result;
    for (int i = 0; i < len; i++) {
        int c = Wire.read();
        if (isPrintable(c)) result += (char)c;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Decoders
// ─────────────────────────────────────────────────────────────────────────────
float linear11(uint16_t data) {
    int16_t exp11 = (data >> 11) & 0x1F;
    if (exp11 > 15) exp11 -= 32;
    int16_t mant = data & 0x7FF;
    if (mant > 1023) mant -= 2048;
    return mant * powf(2.0f, (float)exp11);
}

float decode_vout(uint16_t raw) {
    return raw * powf(2.0f, -9.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan
// ─────────────────────────────────────────────────────────────────────────────
bool scan_psu() {
    Wire.beginTransmission(PSU_ADDR);
    return (Wire.endTransmission() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// MFR info
// ─────────────────────────────────────────────────────────────────────────────
void read_mfr() {
    g_mfr.id       = pmbus_read_string(CMD_MFR_ID);
    g_mfr.model    = pmbus_read_string(CMD_MFR_MODEL);
    g_mfr.revision = pmbus_read_string(CMD_MFR_REVISION);
    g_mfr.location = pmbus_read_string(CMD_MFR_LOCATION);
    g_mfr.date     = pmbus_read_string(CMD_MFR_DATE);
    g_mfr.serial   = pmbus_read_string(CMD_MFR_SERIAL);
    g_mfr.loaded   = true;
    Serial.printf("MFR: %s / %s / SN:%s\n",
        g_mfr.id.c_str(), g_mfr.model.c_str(), g_mfr.serial.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Protection limits + MFR specs
// ─────────────────────────────────────────────────────────────────────────────
void read_limits() {
    uint16_t w;

    Wire.beginTransmission(PSU_ADDR);
    Wire.write(0x10); // WRITE_PROTECT
    Wire.write(0x00);
    Wire.endTransmission(true);

    if (pmbus_read_word(CMD_VOUT_COMMAND,        w)) g_limits.vout_command  = decode_vout(w);
    if (pmbus_read_word(CMD_VOUT_MAX,            w)) g_limits.vout_max      = decode_vout(w);
    if (pmbus_read_word(CMD_VOUT_OV_FAULT_LIMIT, w)) g_limits.vout_ov_fault = decode_vout(w);
    if (pmbus_read_word(CMD_VOUT_OV_WARN_LIMIT,  w)) g_limits.vout_ov_warn  = decode_vout(w);
    if (pmbus_read_word(CMD_VOUT_UV_FAULT_LIMIT, w)) g_limits.vout_uv_fault = decode_vout(w);
    if (pmbus_read_word(CMD_IOUT_OC_FAULT_LIMIT, w)) g_limits.iout_oc_fault = linear11(w);
    if (pmbus_read_word(CMD_IOUT_OC_WARN_LIMIT,  w)) g_limits.iout_oc_warn  = linear11(w);
    if (pmbus_read_word(CMD_MFR_VIN_MIN,         w)) g_limits.mfr_vin_min      = linear11(w);
    if (pmbus_read_word(CMD_MFR_VIN_MAX,         w)) g_limits.mfr_vin_max      = linear11(w);
    if (pmbus_read_word(CMD_MFR_IIN_MAX,         w)) g_limits.mfr_iin_max      = linear11(w);
    if (pmbus_read_word(CMD_MFR_PIN_MAX,         w)) g_limits.mfr_pin_max      = linear11(w);
    if (pmbus_read_word(CMD_MFR_VOUT_MIN,        w)) g_limits.mfr_vout_min     = decode_vout(w);
    if (pmbus_read_word(CMD_MFR_VOUT_MAX,        w)) g_limits.mfr_vout_max     = decode_vout(w);
    if (pmbus_read_word(CMD_MFR_IOUT_MAX,        w)) g_limits.mfr_iout_max     = linear11(w);
    if (pmbus_read_word(CMD_MFR_POUT_MAX,        w)) g_limits.mfr_pout_max     = linear11(w);
    if (pmbus_read_word(CMD_MFR_TAMBIENT_MIN,    w)) g_limits.mfr_tambient_min = linear11(w);
    if (pmbus_read_word(CMD_MFR_TAMBIENT_MAX,    w)) g_limits.mfr_tambient_max = linear11(w);

    g_limits.loaded = true;
    Serial.println("Limits loaded.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Fast path — VIN/IIN/PIN/VOUT/IOUT/POUT only
// Returns false on any I2C failure; d.valid untouched on failure so stale data
// ─────────────────────────────────────────────────────────────────────────────
bool read_psu_fast(PSUData &d) {
    uint16_t w;
    if (!pmbus_read_word(CMD_READ_VIN,  w)) return false; d.V_in  = linear11(w);
    if (!pmbus_read_word(CMD_READ_IIN,  w)) return false; d.I_in  = linear11(w);
    if (!pmbus_read_word(CMD_READ_PIN,  w)) return false; d.W_in  = linear11(w);
    if (!pmbus_read_word(CMD_READ_VOUT, w)) return false; d.V_out = decode_vout(w);
    if (!pmbus_read_word(CMD_READ_IOUT, w)) return false; d.I_out = linear11(w);
    if (!pmbus_read_word(CMD_READ_POUT, w)) return false; d.W_out = linear11(w);
    d.valid = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slow path — temps, fan, status
// ─────────────────────────────────────────────────────────────────────────────
void read_psu_slow(PSUData &d) {
    uint16_t w;
    if (pmbus_read_word(CMD_READ_TEMP1, w)) d.T1      = linear11(w);
    if (pmbus_read_word(CMD_READ_TEMP2, w)) d.T2      = linear11(w);
    if (pmbus_read_word(CMD_READ_TEMP3, w)) d.T3      = linear11(w);
    if (pmbus_read_word(CMD_READ_FAN1,  w)) d.fan     = linear11(w);
    // FAN_COMMAND_1 read is harmless even if writes are locked
    if (pmbus_read_word(CMD_FAN_COMMAND_1, w)) d.fan_cmd = linear11(w);

    d.status_byte  = pmbus_read_byte(CMD_STATUS_BYTE);
    if (pmbus_read_word(CMD_STATUS_WORD, w)) d.status_word = w;
    d.status_vout  = pmbus_read_byte(CMD_STATUS_VOUT);
    d.status_iout  = pmbus_read_byte(CMD_STATUS_IOUT);
    d.status_input = pmbus_read_byte(CMD_STATUS_INPUT);
    d.status_temp  = pmbus_read_byte(CMD_STATUS_TEMP);
    d.status_cml   = pmbus_read_byte(CMD_STATUS_CML);
}
