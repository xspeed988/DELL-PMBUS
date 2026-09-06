# DELL-PMBUS
PMBus monitor for Dell server PSUs — running on an ESP32-C3
Dell server power supply as a bench supply with a real-time web dashboard, OLED display — all over Wi-Fi. Tested on the D1100e (Delta DPS-1100BB).

## Hardware
- ESP32-C3 dev board
- Dell D1100e / DPS-1100BB
- SSD1306 128×64 OLED I²C

## Wiring
```
Dell D1100e S0 connector          ESP32-C3
─────────────────────────         ────────────────────
S1  PS_PRESENT ──┐
                 │
S14 PS_KILL ─────┘

S13 PS_ON ──────[SW]──── GND

S17 SDA ─────────────────────────── GPIO4
S19 SCL ─────────────────────────── GPIO5

```

```
SSD1306 OLED              ESP32-C3
────────────              ────────
SDA ─────────────────── GPIO6
SCL ─────────────────── GPIO7
VCC ─────────────────── 3.3V
GND ─────────────────── GND
```

## Configuration
1. Copy `config.example.h` to `config.h`
2. Fill in your Wi-Fi credentials and verify pin assignments:
```cpp
  #define WIFI_SSID  "your_network"
  #define WIFI_PASS  "your_password"
  
  #define PSU_SDA  4    // PMBus I²C
  #define PSU_SCL  5
  #define OLED_SDA 6    // OLED I²C
  #define OLED_SCL 7
```
3. Open psu_monitor.ino in Arduino IDE and flash.

## OTA updates
After the first USB flash the device advertises itself as psu-monitor on your local network. Subsequent updates can be flashed wirelessly.

## Web interface
Once connected, open `http://<device-ip>/` in any browser.

## License
This project is licensed under the GPLv3. GPLv3 (GNU General Public License version 3) is a free, open-source software license that guarantees users the freedoms to run, study, share, and modify the software. The complete text of the GPLv3 license is included in the LICENSE file of this project. Before using, modifying, or distributing the code of this project, make sure you have read and understood the entire GPLv3 license.

## Acknowledgements
Inspired by [MYNOVA-SmartPower](https://github.com/Tomosawa/MYNOVA-SmartPower) by Tomosawa — particularly the initial PMBus register map for the Dell D1100e.
