#pragma once
#include "config.h"
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_SW_I2C display;

void field_to_str(FieldID id, const PSUData &d, char *buf, size_t sz);
FieldID name_to_field(const String &s);
void draw_no_psu();
void draw_psu(const PSUData &d);