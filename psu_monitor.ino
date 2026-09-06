/*
 * Dell D1100e (Delta DPS-1100BB) PMBus Monitor
 * ESP32-C3  |  U8g2 + WebServer + ArduinoOTA
 *
 * PSU PMBus  : SDA=GPIO4,  SCL=GPIO5   (Wire, 400 kHz)
 * OLED       : SDA=GPIO6,  SCL=GPIO7   (SW I2C via U8g2)
 *
 * Web UI     : http://<device-ip>/
 */

#include "config.h"
#include "pmbus.h"
#include "oled.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

// ── Global definitions ────────────────────────────────────────────────────────
PSUData   g_data   = {};
PSULimits g_limits = {};
MFRInfo   g_mfr    = {};
float     g_iout_max = 0.0f;
float     g_pout_max = 0.0f;
float     g_vout_min = 1e9f; 
bool      psu_found  = false;

#define HIST_LEN  600
#define HIST_EVERY 2    // fast ticks between samples (2 × 20 ms = 40 ms = 10 Hz)
struct HistSample { float pout; float iout; float vout; };
HistSample g_hist[HIST_LEN];
uint16_t   g_hist_head  = 0;
uint16_t   g_hist_count = 0;

FieldID oled_fields[OLED_ROWS] = { F_VIN, F_VOUT, F_IIN, F_IOUT, F_POUT, F_T1 };

const char* FIELD_NAMES[F_COUNT]  = { "VIN","IIN","PIN","VOUT","IOUT","POUT","T1","T2","T3","FAN","STATUS" };
const char* FIELD_LABELS[F_COUNT] = { "Vin ","Iin ","Pin ","Vout","Iout","Pout","T1  ","T2  ","T3  ","Fan ","Stat" };

// ── Peripherals ───────────────────────────────────────────────────────────────
U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);
WebServer server(80);

// ─────────────────────────────────────────────────────────────────────────────
static void setup_ota() {
    ArduinoOTA.setHostname("psu-monitor");
    // ArduinoOTA.setPassword("yourpassword");  // uncomment to protect

    ArduinoOTA.onStart([]() {
        Serial.println("OTA start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA done");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready — hostname: psu-monitor");
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    // 400 kHz
    Wire.begin(PSU_SDA, PSU_SCL, 400000UL);

    display.begin();
    display.setFont(u8g2_font_6x10_tf);
    display.clearBuffer();
    display.drawStr(0, 12, "PSU Monitor");
    display.drawStr(0, 26, "Dell D1100e");
    display.drawStr(0, 40, "Full PMBus mode");
    display.drawStr(0, 54, "Connecting WiFi..");
    display.sendBuffer();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 50) {
        delay(500); Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP: "); Serial.println(WiFi.localIP());
        display.clearBuffer();
        display.drawStr(0, 12, "WiFi OK");
        display.drawStr(0, 26, WiFi.localIP().toString().c_str());
        display.sendBuffer();
        setup_ota();
    } else {
        Serial.println("WiFi failed — monitor only");
        display.clearBuffer();
        display.drawStr(0, 12, "WiFi FAILED");
        display.drawStr(0, 26, "Monitor only");
        display.sendBuffer();
    }
    delay(1000);

    server.on("/",          HTTP_GET,  handle_root);
    server.on("/data",      HTTP_GET,  handle_data);
    server.on("/history",   HTTP_GET,  handle_history);
    server.on("/oled",      HTTP_POST, handle_oled_cfg);
    server.on("/reset_max", HTTP_POST, handle_reset_max);
    server.begin();
    Serial.println("HTTP server started");

    psu_found = scan_psu();
    if (psu_found) {
        Serial.println("PSU found — reading MFR + limits");
        read_mfr();
        read_limits();
        read_psu_slow(g_data);
    } else {
        Serial.println("PSU NOT found at 0x58");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    static uint32_t last_fast = 0;
    static uint16_t tick      = 0;

    ArduinoOTA.handle();
    server.handleClient();

    uint32_t now = millis();
    if (now - last_fast < FAST_INTERVAL_MS) return;
    last_fast = now;
    tick++;

    // ── PSU lost / reconnect ────────────────────────────────────────────────
    if (!psu_found) {
        psu_found = scan_psu();
        if (!psu_found) {
            draw_no_psu();
            return;
        }
        Serial.println("PSU reconnected — reloading statics");
        read_mfr();
        read_limits();
        read_psu_slow(g_data);
        tick = 0;
    }

    // ── Fast read: V/I/W ────────────────────────────────────────────────────
    if (!read_psu_fast(g_data)) {
        Serial.println("Fast read failed");
        psu_found = false;
        draw_no_psu();
        return;
    }

    // Peak / minimum tracking
    if (g_data.I_out > g_iout_max) g_iout_max = g_data.I_out;
    if (g_data.W_out > g_pout_max) g_pout_max = g_data.W_out;
    if (g_data.valid && g_data.V_out > 0.1f && g_data.V_out < g_vout_min)
        g_vout_min = g_data.V_out;

    // ── History sample
    if (tick % HIST_EVERY == 0) {
        g_hist[g_hist_head] = { g_data.W_out, g_data.I_out, g_data.V_out };
        g_hist_head = (g_hist_head + 1) % HIST_LEN;
        if (g_hist_count < HIST_LEN) g_hist_count++;
    }

    // ── Slow read: temps / fan / status ─────────────────────────────────────
    if (tick % SLOW_EVERY == 0) {
        read_psu_slow(g_data);
    }

    // ── OLED ────────────────────────────────────────────────────────────────
    if (tick % OLED_EVERY == 0) {
        draw_psu(g_data);
    }

    // ── Serial heartbeat (every slow tick) ──────────────────────────────────
    if (tick % SLOW_EVERY == 0) {
        Serial.printf(
            "Vin=%.1fV Iin=%.2fA Pin=%.1fW | Vout=%.3fV Iout=%.2fA Pout=%.1fW"
            " (pk=%.1fW vmin=%.3fV) | T=%.1f/%.1f/%.1f Fan=%.0f | STAT=%02X\n",
            g_data.V_in, g_data.I_in, g_data.W_in,
            g_data.V_out, g_data.I_out, g_data.W_out, g_pout_max, g_vout_min,
            g_data.T1, g_data.T2, g_data.T3, g_data.fan,
            g_data.status_byte);
    }
}
