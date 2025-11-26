#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "esp_lcd_types.h"


#define COLOR_BG_DARK      0x202020u
#define COLOR_BG_LIGHT     0xE0E0E0u
#define COLOR_ACCENT_BLUE  0x2F80EDu
#define COLOR_ACCENT_GREEN 0x27AE60u
#define COLOR_ALERT_RED    0xEB5757u


// --- Display / LVGL ---
void core2_display_init(void);                 // start BSP display + LVGL task + backlight
void core2_display_set_bg_hex(uint32_t hex);   // solid background (no widgets)

// --- Brightness ---
void core2_brightness_set(uint8_t percent);     // 0..100, clamped
uint8_t core2_brightness_get(void);            // last set value (cached)

// --- Touch ---
lv_indev_t* core2_touch_get_indev(void);       // get LVGL indev handler from BSP
bool core2_touch_read(lv_indev_t *indev, lv_point_t *p_out); // returns true if pressed, and fills point

// --- Battery (AXP192 PMU) ---
void     core2_battery_init(void);
uint16_t core2_battery_read_mv(void);   // battery voltage in mV
bool core2_battery_is_charging(void); // true if charger active
bool core2_usb_present(void);         // true if VBUS/USB power present

// Enable/disable external 5 V rail on BUS_5V (Port-A, etc.)
void core2_bus5v_enable(void);



// --- AXP192 raw debug ---
uint8_t core2_axp_read_reg(uint8_t reg);

// R00 bits:
// 7: ACIN present 
// 6: ACIN usable
// 5: VBUS present
// 4: VBUS usable
// 2: batt current direction: 1=charge/0=disch.
// 0: boot source: ACIN/VBUS

// reg 0x01 (R01)
// 6: charger active: 1=charging 0=not charging
// 5: battery present

// --- Battery percent (voltage-based) ---
uint8_t core2_battery_percent(void);  // 0..100%


// --- Vibration motor ---
 void core2_vibration_on(void);
 void core2_vibration_off(void);
void core2_vibration_pulse_ms(uint16_t ms);   // one-shot pulse

// --- Speaker / Audio ---
void core2_audio_init(uint32_t sample_rate_hz);
void core2_speaker_set_volume(uint8_t percent);         // 0..100
void core2_speaker_beep(uint16_t freq_hz, uint16_t ms); // simple square-wave beep

// --- I2C scan ---
void core2_i2c_scan(void);   // logs found 7-bit addresses

// --- Wi-Fi (STA) ---
void core2_wifi_init_sta(const char *ssid, const char *pass);
bool core2_wifi_is_connected(void);
uint32_t core2_wifi_get_ip_u32(void);   // IPv4 in network byte order

// --- RTC (BM8563 / PCF8563 compatible) ---
typedef struct {
    uint16_t year;   // full year, e.g. 2025
    uint8_t  month;  // 1-12
    uint8_t  day;    // 1-31
    uint8_t  hour;   // 0-23
    uint8_t  min;    // 0-59
    uint8_t  sec;    // 0-59
    uint8_t  wday;   // 0-6 (chip convention)
} core2_rtc_time_t;

void core2_rtc_init(void);
bool core2_rtc_get_time(core2_rtc_time_t *t);
bool core2_rtc_set_time(const core2_rtc_time_t *t);

