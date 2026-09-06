#include "oled.h"

void field_to_str(FieldID id, const PSUData &d, char *buf, size_t sz) {
    switch (id) {
        case F_VIN:    snprintf(buf, sz, "%s%6.1f V",   FIELD_LABELS[id], d.V_in);    break;
        case F_IIN:    snprintf(buf, sz, "%s%6.2f A",   FIELD_LABELS[id], d.I_in);    break;
        case F_PIN:    snprintf(buf, sz, "%s%6.1f W",   FIELD_LABELS[id], d.W_in);    break;
        case F_VOUT:   snprintf(buf, sz, "%s%6.3f V",   FIELD_LABELS[id], d.V_out);   break;
        case F_IOUT:   snprintf(buf, sz, "%s%6.2f A",   FIELD_LABELS[id], d.I_out);   break;
        case F_POUT:   snprintf(buf, sz, "%s%6.1f W",   FIELD_LABELS[id], d.W_out);   break;
        case F_T1:     snprintf(buf, sz, "%s%5.1f C",   FIELD_LABELS[id], d.T1);      break;
        case F_T2:     snprintf(buf, sz, "%s%5.1f C",   FIELD_LABELS[id], d.T2);      break;
        case F_T3:     snprintf(buf, sz, "%s%5.1f C",   FIELD_LABELS[id], d.T3);      break;
        case F_FAN:    snprintf(buf, sz, "%s%5.0f RPM", FIELD_LABELS[id], d.fan);     break;
        case F_STATUS: snprintf(buf, sz, "%s%02X %s",   FIELD_LABELS[id], d.status_byte,
                                d.status_byte ? "FLT" : "OK");                         break;
        default:       snprintf(buf, sz, "---"); break;
    }
}

FieldID name_to_field(const String &s) {
    for (uint8_t i = 0; i < F_COUNT; i++)
        if (s == FIELD_NAMES[i]) return (FieldID)i;
    return F_COUNT;
}

void draw_no_psu() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 12, "PSU not found");
    display.drawStr(0, 26, "0x58 no ACK");
    display.drawStr(0, 40, "Check pullups &");
    display.drawStr(0, 54, "PS_ON / PS_KILL");
    display.sendBuffer();
}

// Row layout on 128x64:
//   y=20  VOUT  "12.345 V"
//   y=41  IOUT  "12.34 A"
//   y=62  POUT  "123.4 W"
static void draw_big_row(uint8_t y, const char *label, const char *value) {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(0, y, label);

    display.setFont(u8g2_font_logisoso16_tf);
    display.drawStr(22, y, value);
}

void draw_psu(const PSUData &d) {
    char vbuf[16], ibuf[16], pbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%.3fV", d.V_out);
    snprintf(ibuf, sizeof(ibuf), "%.2fA",  d.I_out);
    snprintf(pbuf, sizeof(pbuf), "%.1fW",  d.W_out);

    display.clearBuffer();

    draw_big_row(20, "VOUT", vbuf);
    draw_big_row(41, "IOUT", ibuf);
    draw_big_row(62, "POUT", pbuf);

    display.sendBuffer();
}
