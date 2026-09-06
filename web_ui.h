#pragma once
#include "config.h"
#include <WebServer.h>

extern WebServer server;

void handle_root();
void handle_data();
void handle_history();
void handle_oled_cfg();
void handle_reset_max();