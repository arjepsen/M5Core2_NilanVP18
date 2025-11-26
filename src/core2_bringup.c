#include "core2_bringup.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

#include "esp_codec_dev.h"


#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"



// AXP192 I2C address on Core2 boards
#define AXP192_I2C_ADDR 0x34

// Battery voltage ADC registers (AXP192/AXP202-compatible map)
// 12-bit value: [0x78: high 8 bits][0x79: low 4 bits]
// LSB = 1.1 mV  (so mV = raw * 1.1)
#define AXP192_REG_BAT_VH 0x78
#define AXP192_REG_BAT_VL 0x79

#define AXP192_REG_POWER_STATUS 0x00  // REG00H
#define AXP192_REG_CHG_STATUS   0x01  // REG01H


// Controls EXTEN, which in Core2 design enables BUS_5V -> Port-A 5V, etc.
// See M5Core2 issue #99: PowerOnPeripherials() toggles 0x0A bit 1 to power BUS_5V. 
#define AXP192_REG_EXTEN_CTRL   0x0A

// Pick a buffer height in lines.
// 80 lines = ~25% of screen, a good smoothness sweet spot.
#define CORE2_BUF_LINES  80

static i2c_master_dev_handle_t s_axp = NULL;

// ---------------- Display ----------------
static uint8_t s_brightness = 100; // cached last brightness (percent)

// void core2_display_init(void)
// {
//     bsp_display_start();        // BSP init display + LVGL port task
//     bsp_display_backlight_on(); // make sure backlight is on
// }

static const char *TAG_DISP = "disp";


void core2_display_init(void)
{
    static bool inited = false;
    if (inited) return;

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg  = ESP_LVGL_PORT_INIT_CONFIG(),
        // buffer_size is in *pixels* (not bytes!)
        .buffer_size    = BSP_LCD_H_RES * CORE2_BUF_LINES,
        .double_buffer  = true,
        .flags = {
            .buff_dma    = true,   // DMA-capable buffer (fast) :contentReference[oaicite:1]{index=1}
            .buff_spiram = false,  // keep in internal RAM for DMA reliability
        }
    };

    // Start display + LVGL with our tuned buffers
    bsp_display_start_with_config(&cfg);  // :contentReference[oaicite:2]{index=2}

    // Backlight on (you already do this somewhere)
    bsp_display_backlight_on();

    


    inited = true;
}



void core2_display_set_bg_hex(uint32_t hex)
{
    // LVGL runs in its own task; lock around LVGL calls
    if (bsp_display_lock(0))
    {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_hex(hex), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        bsp_display_unlock();
    }
}

void core2_brightness_set(uint8_t percent)
{
    // Fast clamp to 0..100
    if (percent > 100) percent = 100;

    s_brightness = percent;

    // BSP call: sets backlight duty/voltage in percent :contentReference[oaicite:2]{index=2}
    // We ignore return here for speed; if you want error logging, we can add a debug-only path.
    (void)bsp_display_brightness_set((int)percent);
}

uint8_t core2_brightness_get(void)
{
    return s_brightness;
}

// ---------------- Touch ----------------

lv_indev_t *core2_touch_get_indev(void)
{
    return bsp_display_get_input_dev(); // BSP wires touch into LVGL
}

bool core2_touch_read(lv_indev_t *indev, lv_point_t *p_out)
{
    if (!indev || !p_out) return false;

    bool pressed = false;

    if (bsp_display_lock(0))
    {
        lv_indev_state_t st = lv_indev_get_state(indev);
        lv_indev_get_point(indev, p_out);
        bsp_display_unlock();

        pressed = (st == LV_INDEV_STATE_PRESSED);
    }

    return pressed;
}

// ---------------- Battery -------------------

static inline esp_err_t axp_read_u16(uint8_t reg, uint16_t *out)
{
    uint8_t rx[2];
    esp_err_t e = i2c_master_transmit_receive(
        s_axp, &reg, 1, rx, 2, -1); // new IDF 5.x API :contentReference[oaicite:2]{index=2}
    if (e == ESP_OK)
    {
        *out = ((uint16_t)rx[0] << 8) | rx[1];
    }
    return e;
}

void core2_battery_init(void)
{
    if (s_axp) return;

    // Make sure BSP I2C bus exists and grab its handle
    bsp_i2c_init();
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle(); // BSP shared bus :contentReference[oaicite:3]{index=3}

    // Attach AXP192 device to that bus
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP192_I2C_ADDR,
        .scl_speed_hz = 400000, // fast but safe for AXP192 + Core2 bus
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_axp)); // :contentReference[oaicite:4]{index=4}
}

uint16_t core2_battery_read_mv(void)
{
    if (!s_axp) core2_battery_init();

    uint16_t raw16;
    if (axp_read_u16(AXP192_REG_BAT_VH, &raw16) != ESP_OK)
    {
        return 0;
    }

    // 12-bit ADC value lives in top 12 bits of the 16-bit word
    uint16_t raw12 = raw16 >> 4;

    // Convert to mV with integer math (raw * 1.1 mV)
    // mv = raw12 * 11 / 10
    uint32_t mv = (uint32_t)raw12 * 11u;
    mv = (mv + 5u) / 10u; // rounded division, still integer-only

    return (uint16_t)mv;
}

static inline esp_err_t axp_read_u8(uint8_t reg, uint8_t *out)
{
    return i2c_master_transmit_receive(
        s_axp, &reg, 1, out, 1, -1);  // single reg read 
}

// NEW: simple 1-byte write helper
static inline esp_err_t axp_write_u8(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    return i2c_master_transmit(s_axp, tx, sizeof(tx), -1);
}

// Enable 5V on Grove / Port A (BUS_5V) using AXP192 internal boost.
// Based on M5Stack’s own AXP192::SetBusPowerMode(0) sequence. 
void core2_bus5v_enable(void)
{
    if (!s_axp) {
        core2_battery_init();   // make sure s_axp is set up on the BSP I2C bus
    }

    uint8_t data;

    // 1) Set GPIO0 LDO to 3.3 V (LDO OUTPUT mode) – AXP reg 0x91
    if (axp_read_u8(0x91, &data) == ESP_OK) {
        data = (data & 0x0F) | 0xF0;
        (void)axp_write_u8(0x91, data);
    }

    // 2) Configure GPIO0 as LDO OUTPUT, pull-up N_VBUSEN – AXP reg 0x90
    if (axp_read_u8(0x90, &data) == ESP_OK) {
        data = (data & 0xF8) | 0x02;
        (void)axp_write_u8(0x90, data);
    }

    // 3) Enable 5V boost via EXTEN bit (bit2 of reg 0x10)
    if (axp_read_u8(0x10, &data) == ESP_OK) {
        data |= 0x04; // EXTEN = 1
        (void)axp_write_u8(0x10, data);
    }
}




bool core2_usb_present(void)
{
    if (!s_axp) core2_battery_init();

    uint8_t r00 = 0;
    if (axp_read_u8(AXP192_REG_POWER_STATUS, &r00) != ESP_OK) {
        return false;
    }

    // Core2 USB feeds ACIN, not VBUS.
    // bit7 ACIN present, bit5 VBUS present (keep OR for safety)
    return (r00 & (1u << 7)) || (r00 & (1u << 5));
}

bool core2_battery_is_charging(void)
{
    if (!s_axp) core2_battery_init();

    uint8_t r00 = 0, r01 = 0;
    if (axp_read_u8(AXP192_REG_POWER_STATUS, &r00) != ESP_OK) return false;
    if (axp_read_u8(AXP192_REG_CHG_STATUS,   &r01) != ESP_OK) return false;

    // bit2: battery current direction (1 = charging)
    // bit6: charger active (1 = charging)
    return (r00 & (1u << 2)) || (r01 & (1u << 6));
}




uint8_t core2_axp_read_reg(uint8_t reg)
{
    if (!s_axp) core2_battery_init();

    uint8_t v = 0;
    (void)axp_read_u8(reg, &v);
    return v;
}


// Typical 1S LiPo voltage-to-percent curve (resting voltage).
// Small table = fast + predictable.
typedef struct {
    uint16_t mv;
    uint8_t  pct;
} bat_lut_t;

static const bat_lut_t s_bat_lut[] = {
    {4200, 100},
    {4100,  90},
    {4000,  80},
    {3900,  70},
    {3800,  60},
    {3700,  50},
    {3600,  35},
    {3500,  20},
    {3400,  10},
    {3300,   5},
    {3200,   0},
};

uint8_t core2_battery_percent(void)
{
    const uint16_t mv = core2_battery_read_mv();

    // Above top
    if (mv >= s_bat_lut[0].mv) return 100;
    // Below bottom
    const int last = (int)(sizeof(s_bat_lut)/sizeof(s_bat_lut[0])) - 1;
    if (mv <= s_bat_lut[last].mv) return 0;

    // Find segment (table is small; linear scan is fastest overall here)
    for (int i = 0; i < last; i++) {
        uint16_t v_hi = s_bat_lut[i].mv;
        uint16_t v_lo = s_bat_lut[i+1].mv;

        if (mv <= v_hi && mv >= v_lo) {
            uint8_t p_hi = s_bat_lut[i].pct;
            uint8_t p_lo = s_bat_lut[i+1].pct;

            uint16_t dv = v_hi - v_lo;
            uint16_t x  = v_hi - mv; // how far from top

            // Linear interpolation: p = p_hi - x*(p_hi-p_lo)/dv
            uint16_t dp = (uint16_t)(p_hi - p_lo);
            uint16_t drop = (uint32_t)x * dp / dv;

            return (uint8_t)(p_hi - drop);
        }
    }

    return 0; // should never hit
}


// Simple on/off inliners (no logging for speed)
 void core2_vibration_on(void)
{
    (void)bsp_feature_enable(BSP_FEATURE_VIBRATION, true);  // :contentReference[oaicite:1]{index=1}
}

 void core2_vibration_off(void)
{
    (void)bsp_feature_enable(BSP_FEATURE_VIBRATION, false); // :contentReference[oaicite:2]{index=2}
}

void core2_vibration_pulse_ms(uint16_t ms)
{
    core2_vibration_on();
    vTaskDelay(pdMS_TO_TICKS(ms));
    core2_vibration_off();
}


static esp_codec_dev_handle_t s_spk = NULL;
static esp_codec_dev_sample_info_t s_spk_fs;

void core2_audio_init(uint32_t sample_rate_hz)
{
    if (s_spk) return;

    // Make sure speaker feature is enabled
    (void)bsp_feature_enable(BSP_FEATURE_SPEAKER, true);  // feature flag exists :contentReference[oaicite:1]{index=1}

    // Init speaker codec via BSP
    s_spk = bsp_audio_codec_speaker_init();              // :contentReference[oaicite:2]{index=2}

    // Define PCM format (mono, 16-bit)
    s_spk_fs.sample_rate     = sample_rate_hz;
    s_spk_fs.channel         = 1;
    s_spk_fs.bits_per_sample = 16;

    // Open speaker stream
    (void)esp_codec_dev_open(s_spk, &s_spk_fs);          // :contentReference[oaicite:3]{index=3}

    // Default volume
    (void)esp_codec_dev_set_out_vol(s_spk, 50);          // :contentReference[oaicite:4]{index=4}
}

void core2_speaker_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    if (!s_spk) core2_audio_init(16000);
    (void)esp_codec_dev_set_out_vol(s_spk, percent);     // :contentReference[oaicite:5]{index=5}
}

void core2_speaker_beep(uint16_t freq_hz, uint16_t ms)
{
    if (!s_spk) core2_audio_init(16000);

    const uint32_t sr = s_spk_fs.sample_rate;
    if (freq_hz == 0 || sr == 0 || ms == 0) return;

    // Total samples to play
    uint32_t total_samples = (sr * (uint32_t)ms) / 1000u;

    // Square wave: half-period in samples
    uint32_t half_period = sr / (2u * (uint32_t)freq_hz);
    if (half_period == 0) half_period = 1;

    // Small stack buffer, no malloc
    int16_t buf[256];
    uint32_t idx_in_half = 0;
    int16_t level = 12000;  // amplitude (keep well below clipping)

    while (total_samples) {
        uint32_t n = total_samples;
        if (n > (uint32_t)(sizeof(buf)/sizeof(buf[0]))) n = (uint32_t)(sizeof(buf)/sizeof(buf[0]));

        for (uint32_t i = 0; i < n; i++) {
            buf[i] = level;

            // toggle every half period
            if (++idx_in_half >= half_period) {
                idx_in_half = 0;
                level = (int16_t)-level;
            }
        }

        // Write PCM to speaker
        (void)esp_codec_dev_write(s_spk, buf, n * sizeof(int16_t)); // :contentReference[oaicite:6]{index=6}

        total_samples -= n;
    }
}


void core2_i2c_scan(void)
{
    static const char *TAG = "i2c_scan";

    // Ensure BSP I2C is up and get its bus handle
    bsp_i2c_init();
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();

    ESP_LOGI(TAG, "Scanning I2C bus...");

    // Scan all 7-bit addresses, skip reserved ranges
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t e = i2c_master_probe(bus, addr, pdMS_TO_TICKS(10));  // IDF 5.x API
        if (e == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        }
    }

    ESP_LOGI(TAG, "I2C scan done.");
}


static const char *WIFI_TAG = "wifi";

static EventGroupHandle_t s_wifi_evt = NULL;
static esp_netif_t *s_netif_sta = NULL;

static volatile bool s_wifi_connected = false;
static volatile uint32_t s_ip_u32 = 0;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRY      10

static int s_retry_num = 0;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        s_ip_u32 = 0;

        if (s_retry_num < WIFI_MAX_RETRY) {
            s_retry_num++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_evt, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_ip_u32 = event->ip_info.ip.addr;
        s_wifi_connected = true;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
    }
}

void core2_wifi_init_sta(const char *ssid, const char *pass)
{
    if (s_wifi_evt) return;  // already inited

    s_wifi_evt = xEventGroupCreate();

    // 1) NVS required by Wi-Fi driver :contentReference[oaicite:1]{index=1}
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2) Netif + default event loop :contentReference[oaicite:2]{index=2}
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif_sta = esp_netif_create_default_wifi_sta();

    // 3) Wi-Fi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4) Register handlers (5.x style) :contentReference[oaicite:3]{index=3}
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // 5) Configure STA
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));                 // STA mode :contentReference[oaicite:4]{index=4}
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));   // apply creds :contentReference[oaicite:5]{index=5}
    ESP_ERROR_CHECK(esp_wifi_start());                                 // start driver :contentReference[oaicite:6]{index=6}

    ESP_LOGI(WIFI_TAG, "wifi_init_sta done");
}

bool core2_wifi_is_connected(void)
{
    return s_wifi_connected;
}

uint32_t core2_wifi_get_ip_u32(void)
{
    return s_ip_u32;
}


// BM8563 I2C address (PCF8563 compatible)
#define BM8563_I2C_ADDR      0x51  // :contentReference[oaicite:2]{index=2}

// Time/date registers (PCF8563/BM8563 map) :contentReference[oaicite:3]{index=3}
#define BM8563_REG_SECONDS   0x02
#define BM8563_REG_MINUTES   0x03
#define BM8563_REG_HOURS     0x04
#define BM8563_REG_DAYS      0x05
#define BM8563_REG_WEEKDAYS  0x06
#define BM8563_REG_MONTHS    0x07
#define BM8563_REG_YEARS     0x08

static i2c_master_dev_handle_t s_rtc = NULL;

static inline uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

static inline uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static inline esp_err_t rtc_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_rtc, &reg, 1, data, len, -1);
}

static inline esp_err_t rtc_write(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[1 + 8];
    buf[0] = reg;
    for (size_t i = 0; i < len; i++) buf[1 + i] = data[i];
    return i2c_master_transmit(s_rtc, buf, 1 + len, -1);
}

void core2_rtc_init(void)
{
    if (s_rtc) return;

    // Use BSP shared I2C bus (same one you use for AXP192)
    bsp_i2c_init();
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BM8563_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_rtc));
}

bool core2_rtc_get_time(core2_rtc_time_t *t)
{
    if (!t) return false;
    if (!s_rtc) core2_rtc_init();

    uint8_t r[7];
    if (rtc_read(BM8563_REG_SECONDS, r, sizeof(r)) != ESP_OK) {
        return false;
    }

    // Mask out control bits per PCF8563/BM8563 convention
    t->sec   = bcd_to_bin(r[0] & 0x7F);
    t->min   = bcd_to_bin(r[1] & 0x7F);
    t->hour  = bcd_to_bin(r[2] & 0x3F);
    t->day   = bcd_to_bin(r[3] & 0x3F);
    t->wday  = bcd_to_bin(r[4] & 0x07);
    t->month = bcd_to_bin(r[5] & 0x1F);
    t->year  = (uint16_t)(2000 + bcd_to_bin(r[6])); // Core2 RTC uses 2000-base

    return true;
}

bool core2_rtc_set_time(const core2_rtc_time_t *t)
{
    if (!t) return false;
    if (!s_rtc) core2_rtc_init();

    uint8_t w[7];
    w[0] = bin_to_bcd(t->sec);
    w[1] = bin_to_bcd(t->min);
    w[2] = bin_to_bcd(t->hour);
    w[3] = bin_to_bcd(t->day);
    w[4] = bin_to_bcd(t->wday);
    w[5] = bin_to_bcd(t->month);               // century bit left 0 => 2000+
    w[6] = bin_to_bcd((uint8_t)(t->year - 2000));

    return (rtc_write(BM8563_REG_SECONDS, w, sizeof(w)) == ESP_OK);
}
