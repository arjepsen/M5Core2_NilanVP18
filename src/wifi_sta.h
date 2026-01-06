#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        WIFI_STRENGTH_0 = 0,
        WIFI_STRENGTH_1,
        WIFI_STRENGTH_2,
        WIFI_STRENGTH_3,
        WIFI_STRENGTH_4,
        WIFI_STRENGTH_DISCONNECTED
    } wifi_strength_t;

    // Start Wi-Fi STA using module defaults (SSID/PASS/timeout live in wifi_sta.c).
    // Returns true if connected within timeout, false otherwise.
    bool wifi_sta_start(void);

    // Query state later (e.g. UI)
    bool wifi_sta_is_connected(void);

    // Wait explicitly if you want (optional)
    bool wifi_sta_wait_connected(uint32_t timeout_ms);

    // Get IPv4 (network byte order). 0 if not connected.
    uint32_t wifi_sta_get_ip_u32(void);

    // Returns current signal strength (0–4 bars or disconnected)
    wifi_strength_t wifi_sta_get_signal_strength();

    // Optional: for debugging
    int8_t wifi_sta_get_rssi();

#ifdef __cplusplus
}
#endif
