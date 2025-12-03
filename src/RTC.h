#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// --- RTC (BM8563 / PCF8563 compatible) ---
typedef struct {
    uint16_t year;   // full year, e.g. 2025
    uint8_t  month;  // 1-12
    uint8_t  day;    // 1-31
    uint8_t  hour;   // 0-23
    uint8_t  min;    // 0-59
    uint8_t  sec;    // 0-59
    uint8_t  wday;   // 0-6 (chip convention)
} rtc_time_t;

void core2_RTC_init();

#endif /* RTC_H */