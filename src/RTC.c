#include "RTC.h"
#include "bsp/esp-bsp.h"


// BM8563 I2C address (PCF8563 compatible)
#define BM8563_I2C_ADDR      0x51  // :contentReference[oaicite:2]{index=2}

// Time/date registers (PCF8563/BM8563 map) :contentReference[oaicite:3]{index=3}
#define BM8563_REG 0x02
// #define BM8563_REG_SECONDS   0x02
// #define BM8563_REG_MINUTES   0x03
// #define BM8563_REG_HOURS     0x04
// #define BM8563_REG_DAYS      0x05
// #define BM8563_REG_WEEKDAYS  0x06
// #define BM8563_REG_MONTHS    0x07
// #define BM8563_REG_YEARS     0x08

static i2c_master_dev_handle_t rtc_handle = NULL;
bool rtc_initialized = false;

static inline uint8_t bcd_to_bin(uint8_t bcd);
static inline uint8_t bin_to_bcd(uint8_t bin);
// static inline esp_err_t rtc_read(uint8_t reg, uint8_t *data, size_t len);
// static inline esp_err_t rtc_write(uint8_t reg, const uint8_t *data, size_t len);



void core2_RTC_init()
{
    if (rtc_initialized)
        return;

    // Ensure i2c is up, get a handle.
    bsp_i2c_init();
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();

    // Set up handle for the RTC
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BM8563_I2C_ADDR,
        .scl_speed_hz    = CONFIG_BSP_I2C_CLK_SPEED_HZ
    };

    i2c_master_bus_add_device(i2c_bus, &dev_cfg, &rtc_handle);

}

bool rtc_get_time(rtc_time_t *time_ptr)
{
    // Presume pointer ok, and rtc already initialized.

    uint8_t bm8563_reg = BM8563_REG;
    uint8_t buffer[7];

    if (i2c_master_transmit_receive(rtc_handle, &bm8563_reg, 1, buffer, 7, -1) == ESP_OK) // Ignoring errors.
    {
        // Mask out control bits per PCF8563/BM8563 convention
        time_ptr->sec   = bcd_to_bin(buffer[0] & 0x7F);
        time_ptr->min   = bcd_to_bin(buffer[1] & 0x7F);
        time_ptr->hour  = bcd_to_bin(buffer[2] & 0x3F);
        time_ptr->day   = bcd_to_bin(buffer[3] & 0x3F);
        time_ptr->wday  = bcd_to_bin(buffer[4] & 0x07);
        time_ptr->month = bcd_to_bin(buffer[5] & 0x1F);
        time_ptr->year  = (uint16_t)(2000 + bcd_to_bin(buffer[6])); // Core2 RTC uses 2000-base

        return true;
    }
    return false;
}


bool rtc_set_time(const rtc_time_t *time_ptr)
{
    // Presume pointer ok, and rtc already initialized.

    uint8_t tx_buffer[8];
    tx_buffer[0] = BM8563_REG;

    tx_buffer[1] = bin_to_bcd(time_ptr->sec);
    tx_buffer[2] = bin_to_bcd(time_ptr->min);
    tx_buffer[3] = bin_to_bcd(time_ptr->hour);
    tx_buffer[4] = bin_to_bcd(time_ptr->day);
    tx_buffer[5] = bin_to_bcd(time_ptr->wday);
    tx_buffer[6] = bin_to_bcd(time_ptr->month);               // century bit left 0 => 2000+
    tx_buffer[7] = bin_to_bcd((uint8_t)(time_ptr->year - 2000));

    return i2c_master_transmit(rtc_handle, tx_buffer, 8, -1);
}



////////// HELPERS /////////////

static inline uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

static inline uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

// static inline esp_err_t rtc_read(uint8_t reg, uint8_t *data, size_t len)
// {
//     return i2c_master_transmit_receive(rtc_handle, &reg, 1, data, len, -1);
// }

// static inline esp_err_t rtc_write(uint8_t reg, const uint8_t *data, size_t len)
// {
//     uint8_t buf[1 + 8];
//     buf[0] = reg;
//     for (size_t i = 0; i < len; i++) 
//     {
//         buf[1 + i] = data[i];
        
//     return i2c_master_transmit(s_rtc, buf, 1 + len, -1);
// }