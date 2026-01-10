#include "wifi_sta.h"
#include "RTC.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "secrets.h"
#include "time.h"
#include <string.h>


static const char *TAG = "wifi_sta";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 10

// How long wifi_sta_start() should wait before giving up (ms)
#ifndef WIFI_STA_START_TIMEOUT_MS
#define WIFI_STA_START_TIMEOUT_MS 10000
#endif

#define NTP_SERVER "pool.ntp.org"


static EventGroupHandle_t event = NULL;
static esp_netif_t *netif = NULL;

static volatile bool connected = false;
static volatile uint32_t ip_addr = 0;
static int retry_count = 0;
static bool wifi_initialized = false;
static wifi_strength_t current_strength = WIFI_STRENGTH_DISCONNECTED;
static int8_t current_rssi = 0;
static TimerHandle_t wifi_strength_timer = NULL;

// =========== PROTOTYPES ===================
static void update_wifi_strength();
static void wifi_sta_init_internal(const char *ssid, const char *pass);
bool wifi_sta_is_connected();
bool wifi_sta_wait_connected(uint32_t timeout_ms);
uint32_t wifi_sta_get_ip_u32();
bool wifi_sta_start();
static void wifi_strength_timer_callback(TimerHandle_t xTimer);

// Forward declarations
static void init_sntp(void);
static void time_sync_notification_cb(struct timeval *tv);

// ============= IMPLEMENTATIONS ===============================
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        connected = false;
        ip_addr = 0;

        if (retry_count < WIFI_MAX_RETRY)
        {
            retry_count++;
            esp_wifi_connect();
        }
        else
        {
            xEventGroupSetBits(event, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)event_data;
        ip_addr = e->ip_info.ip.addr;
        connected = true;
        retry_count = 0;
        xEventGroupSetBits(event, WIFI_CONNECTED_BIT);
        update_wifi_strength(); // Immediate update on connect

        init_sntp(); // Start non-blocking SNTP

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        return;
    }
    else
    {
        connected = false;
        current_strength = WIFI_STRENGTH_DISCONNECTED;
        current_rssi = 0;
    }
}

static void wifi_sta_init_internal(const char *ssid, const char *pass)
{
    if (wifi_initialized) return;
    wifi_initialized = true;

    event = xEventGroupCreate();

    // --- NVS required by Wi-Fi ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    // --- Netif + event loop ---
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(ret);

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(ret);

    // Must be after event loop create
    netif = esp_netif_create_default_wifi_sta();

    // --- Wi-Fi driver init ---
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    // --- STA config ---
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA started, connecting to \"%s\"", ssid);
}

bool wifi_sta_start()
{
    wifi_sta_init_internal(WIFI_STA_DEFAULT_SSID, WIFI_STA_DEFAULT_PASS);

    bool ok = wifi_sta_wait_connected(WIFI_STA_START_TIMEOUT_MS);

    if (ok)
    {
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
    else
    {
        ESP_LOGW(TAG, "Wi-Fi NOT connected (timeout %u ms)",
                 (unsigned)WIFI_STA_START_TIMEOUT_MS);
    }

    // Create a 5-second FreeRTOS timer for checking connectivity
    wifi_strength_timer = xTimerCreate(
        "WiFiStrength",
        pdMS_TO_TICKS(5000), // period
        pdTRUE,              // auto-reload
        NULL,
        wifi_strength_timer_callback);

    if (wifi_strength_timer != NULL)
    {
        xTimerStart(wifi_strength_timer, 0);
    }

    // Initial update
    update_wifi_strength();

    return ok;
}

bool wifi_sta_wait_connected(uint32_t timeout_ms)
{
    if (!event) return false;

    const TickType_t to = pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits = xEventGroupWaitBits(
        event,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        to);

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_sta_is_connected()
{
    return connected;
}

uint32_t wifi_sta_get_ip_u32()
{
    return ip_addr;
}

// Getter functions
wifi_strength_t wifi_sta_get_signal_strength()
{
    return current_strength;
}

int8_t wifi_sta_get_rssi(void)
{
    return current_rssi;
}

static void update_wifi_strength()
{
    if (!wifi_sta_is_connected())
    {
        current_strength = WIFI_STRENGTH_DISCONNECTED;
        current_rssi = 0;
        return;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        current_strength = WIFI_STRENGTH_DISCONNECTED;
        return;
    }

    current_rssi = ap_info.rssi;

    if (current_rssi >= -55)
        current_strength = WIFI_STRENGTH_4;
    else if (current_rssi >= -65)
        current_strength = WIFI_STRENGTH_3;
    else if (current_rssi >= -75)
        current_strength = WIFI_STRENGTH_2;
    else if (current_rssi >= -85)
        current_strength = WIFI_STRENGTH_1;
    else
        current_strength = WIFI_STRENGTH_0;
}

static void wifi_strength_timer_callback(TimerHandle_t xTimer)
{
    update_wifi_strength();
}

// Non-blocking SNTP initialization
static void init_sntp(void)
{
    if (esp_sntp_enabled())
    {
        ESP_LOGI(TAG, "SNTP already enabled");
        return;
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP initialized - waiting for background sync");
}

// Called automatically when NTP sync succeeds
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized!");

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Set system timezone (CET/CEST for Denmark)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Update RTC
    rtc_time_t rtc_time = {
        .sec = (uint8_t)timeinfo.tm_sec,
        .min = (uint8_t)timeinfo.tm_min,
        .hour = (uint8_t)timeinfo.tm_hour,
        .day = (uint8_t)timeinfo.tm_mday,
        .wday = (uint8_t)timeinfo.tm_wday,
        .month = (uint8_t)(timeinfo.tm_mon + 1),
        .year = (uint16_t)(timeinfo.tm_year + 1900)};

    if (rtc_set_time(&rtc_time))
    {
        ESP_LOGI(TAG, "RTC updated successfully from NTP");
    }
    else
    {
        ESP_LOGE(TAG, "RTC update failed");
    }

    ESP_LOGI(TAG, "Time synced: %s", asctime(&timeinfo));
}

