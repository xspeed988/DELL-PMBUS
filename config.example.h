#pragma once
#include <Arduino.h>
#include <Wire.h>

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID "your_network_name"
#define WIFI_PASS "your_password"

// ── Pins ─────────────────────────────────────────────────────────────────────
// PSU PMBus I²C bus
#define PSU_SDA  4
#define PSU_SCL  5

// OLED I²C bus (software I²C via U8g2)
#define OLED_SDA 6
#define OLED_SCL 7

// ── PMBus ────────────────────────────────────────────────────────────────────
#define PSU_ADDR 0x58   // Dell D1100e / DPS-1100BB default address

// Live readings
#define CMD_READ_VIN      0x88
#define CMD_READ_IIN      0x89
#define CMD_READ_VOUT     0x8B
#define CMD_READ_IOUT     0x8C
#define CMD_READ_TEMP1    0x8D
#define CMD_READ_TEMP2    0x8E
#define CMD_READ_TEMP3    0x8F
#define CMD_READ_FAN1     0x90
#define CMD_READ_POUT     0x96
#define CMD_READ_PIN      0x97

// Status
#define CMD_STATUS_BYTE   0x78
#define CMD_STATUS_WORD   0x79
#define CMD_STATUS_VOUT   0x7A
#define CMD_STATUS_IOUT   0x7B
#define CMD_STATUS_INPUT  0x7C
#define CMD_STATUS_TEMP   0x7D
#define CMD_STATUS_CML    0x7E

// Protection limits
#define CMD_VOUT_COMMAND        0x21
#define CMD_VOUT_MAX            0x24
#define CMD_VOUT_OV_FAULT_LIMIT 0x40
#define CMD_VOUT_OV_WARN_LIMIT  0x42
#define CMD_VOUT_UV_FAULT_LIMIT 0x44
#define CMD_IOUT_OC_FAULT_LIMIT 0x46
#define CMD_IOUT_OC_WARN_LIMIT  0x4A
#define CMD_FAN_COMMAND_1       0x3B

// MFR info
#define CMD_MFR_ID        0x99
#define CMD_MFR_MODEL     0x9A
#define CMD_MFR_REVISION  0x9B
#define CMD_MFR_LOCATION  0x9C
#define CMD_MFR_DATE      0x9D
#define CMD_MFR_SERIAL    0x9E

// MFR rated specs
#define CMD_MFR_VIN_MIN       0xA0
#define CMD_MFR_VIN_MAX       0xA1
#define CMD_MFR_IIN_MAX       0xA2
#define CMD_MFR_PIN_MAX       0xA3
#define CMD_MFR_VOUT_MIN      0xA4
#define CMD_MFR_VOUT_MAX      0xA5
#define CMD_MFR_IOUT_MAX      0xA6
#define CMD_MFR_POUT_MAX      0xA7
#define CMD_MFR_TAMBIENT_MAX  0xA8
#define CMD_MFR_TAMBIENT_MIN  0xA9

// ── Timing ────────────────────────────────────────────────────────────────────
#define FAST_INTERVAL_MS  20    // Fast poll interval in ms
#define SLOW_EVERY        25    // Slow poll every N fast ticks (25 × 20 ms = 500 ms)
#define OLED_EVERY        2     // OLED refresh every N fast ticks (2 × 20 ms = 40 ms)

// ── Field IDs ────────────────────────────────────────────────────────────────
enum FieldID : uint8_t {
    F_VIN=0, F_IIN, F_PIN,
    F_VOUT, F_IOUT, F_POUT,
    F_T1, F_T2, F_T3,
    F_FAN, F_STATUS,
    F_COUNT
};

#define OLED_ROWS 6

// ── Data structs ─────────────────────────────────────────────────────────────
struct PSUData {
    float    V_in, I_in, W_in;
    float    V_out, I_out, W_out;
    float    T1, T2, T3;
    float    fan, fan_cmd;
    uint8_t  status_byte;
    uint16_t status_word;
    uint8_t  status_vout;
    uint8_t  status_iout;
    uint8_t  status_input;
    uint8_t  status_temp;
    uint8_t  status_cml;
    bool     valid;
};

struct PSULimits {
    float   vout_command;
    float   vout_max;
    float   vout_ov_fault, vout_ov_warn;
    float   vout_uv_fault;
    float   iout_oc_fault, iout_oc_warn;
    float   mfr_vin_min, mfr_vin_max;
    float   mfr_iin_max;
    float   mfr_pin_max;
    float   mfr_vout_min, mfr_vout_max;
    float   mfr_iout_max;
    float   mfr_pout_max;
    float   mfr_tambient_min, mfr_tambient_max;
    bool    loaded;
};

struct MFRInfo {
    String id, model, revision, location, date, serial;
    bool   loaded;
};

// ── Extern globals (defined in psu_monitor.ino) ───────────────────────────────
extern PSUData   g_data;
extern PSULimits g_limits;
extern MFRInfo   g_mfr;
extern float     g_iout_max;
extern float     g_pout_max;
extern FieldID   oled_fields[OLED_ROWS];
extern bool      psu_found;

extern const char* FIELD_NAMES[F_COUNT];
extern const char* FIELD_LABELS[F_COUNT];
