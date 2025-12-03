#ifndef CORE2_DISPLAY_H
#define CORE2_DISPLAY_H

#include <stdbool.h>
#include "lvgl.h"
#include "esp_lcd_types.h"
#include "AXP192.h"

// Defines for specific colors.
#define COLOR_BG_DARK      0x202020u
#define COLOR_BG_LIGHT     0xE0E0E0u
#define COLOR_ACCENT_BLUE  0x2F80EDu
#define COLOR_ACCENT_GREEN 0x27AE60u
#define COLOR_ALERT_RED    0xEB5757u


// Brightness:
// just barely visible: 0x46   for deep "screen IS still on... but not really visible..."
// Max is probably 0x68

void display_init();                 // start BSP display + LVGL task + backlight
//void set_brightness(uint8_t brightness);    // Backlight level between 0x46 and 0x68. lower than 0x46 is essentially off.
void display_set_bg_hex(uint32_t hex);

// Abstraction from axp192 module:
inline void set_brightness(uint8_t brightness)
{
    set_backlight_level(brightness);
}

#endif /* CORE2_DISPLAY_H */