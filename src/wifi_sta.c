#include "wifi_sta.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_sta";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRY      10

// -------- Defaults live HERE --------
// Set via build_flags in platformio.ini.
#ifndef WIFI_STA_DEFAULT_SSID
#define WIFI_STA_DEFAULT_SSID "SSID"
#endif

#ifndef WIFI_STA_DEFAULT_PASS
#define WIFI_STA_DEFAULT_PASS "PASSWD"
#endif

// How long wifi_sta_start() should wait before giving up (ms)
#ifndef WIFI_STA_START_TIMEOUT_MS
#define WIFI_STA_START_TIMEOUT_MS 10000
#endif
// -----------------------------------

static EventGroupHandle_t s_evt = NULL;
static esp_netif_t *s_netif = NULL;

static volatile bool s_connected = false;
static volatile uint32_t s_ip_u32 = 0;
static int s_retry = 0;
static bool s_inited = false;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        s_ip_u32 = 0;

        if (s_retry < WIFI_MAX_RETRY) {
            s_retry++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_evt, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)event_data;
        s_ip_u32 = e->ip_info.ip.addr;
        s_connected = true;
        s_retry = 0;
        xEventGroupSetBits(s_evt, WIFI_CONNECTED_BIT);

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        return;
    }
}

static void wifi_sta_init_internal(const char *ssid, const char *pass)
{
    if (s_inited) return;
    s_inited = true;

    s_evt = xEventGroupCreate();

    // --- NVS required by Wi-Fi ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    // --- Netif + event loop ---
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(ret);

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(ret);

    // Must be after event loop create
    s_netif = esp_netif_create_default_wifi_sta();

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

bool wifi_sta_start(void)
{
    wifi_sta_init_internal(WIFI_STA_DEFAULT_SSID, WIFI_STA_DEFAULT_PASS);

    bool ok = wifi_sta_wait_connected(WIFI_STA_START_TIMEOUT_MS);

    if (ok) {
        ESP_LOGI(TAG, "Wi-Fi connected");
    } else {
        ESP_LOGW(TAG, "Wi-Fi NOT connected (timeout %u ms)",
                 (unsigned)WIFI_STA_START_TIMEOUT_MS);
    }
    return ok;
}

bool wifi_sta_wait_connected(uint32_t timeout_ms)
{
    if (!s_evt) return false;

    const TickType_t to = pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits = xEventGroupWaitBits(
        s_evt,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        to
    );

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}

uint32_t wifi_sta_get_ip_u32(void)
{
    return s_ip_u32;
}
