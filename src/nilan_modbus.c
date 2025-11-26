#include "nilan_modbus.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "nilan_modbus";

// ---------------- UART / Modbus configuration ----------------

// CTS 602 / Nilan on Core2 Port A using the RS485 module:
//
// Port A:
//   GPIO32 -> Unit RX (Core2 TX)
//   GPIO33 -> Unit TX (Core2 RX)
//
// Arduino code used:
//   Serial2.begin(19200, SERIAL_8E1, RS485_RX_PIN, RS485_TX_PIN);
// with RS485_RX_PIN = 33, RS485_TX_PIN = 32
// => TX = 32, RX = 33 here.
#define NILAN_UART_PORT          UART_NUM_2
#define NILAN_UART_TX_GPIO       GPIO_NUM_32
#define NILAN_UART_RX_GPIO       GPIO_NUM_33
#define NILAN_UART_BAUDRATE      19200
#define NILAN_UART_BUF_SIZE      256

// Input registers: tank top/bottom (you originally used 211 / 212).
// Adjust here if you later want to poll other registers (e.g. 215 for T15).
#define NILAN_SLAVE_ADDR         30           // CTS 602 default Modbus address (from Arduino sketch)
#define NILAN_REG_TANK_TOP       211
#define NILAN_POLL_INTERVAL_MS   1000
#define NILAN_OFFLINE_TIMEOUT_MS 10000        // 10 s without OK => OFFLINE

// ---------------- Internal state ----------------

static bool           s_started       = false;
static TaskHandle_t   s_poll_task     = NULL;

// Latest cached temperatures (centi-degC, signed)
static int16_t        s_tank_top_cC   = 0;
static int16_t        s_tank_bot_cC   = 0;

// Online / stats
static uint32_t       s_ok_count      = 0;
static uint32_t       s_fail_count    = 0;
static uint32_t       s_last_ok_ms    = 0;      // 0 = never
static nilan_mb_err_t s_last_err      = NILAN_MB_ERR_NONE;

// ---------------- Helpers ----------------

static uint32_t get_time_ms(void)
{
    int64_t us = esp_timer_get_time(); // microseconds since boot
    return (uint32_t)(us / 1000);      // wrap is fine (49+ days)
}

// Standard Modbus RTU CRC16 (poly 0xA001, init 0xFFFF, little-endian)
static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Low-level: read "qty" input registers starting at "start" into "regs".
static esp_err_t nilan_read_input_regs(uint16_t start,
                                       uint16_t qty,
                                       uint16_t *regs,
                                       nilan_mb_err_t *err_out)
{
    if (err_out) {
        *err_out = NILAN_MB_ERR_NONE;
    }

    if (qty == 0 || qty > 16) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_LENGTH;
        }
        return ESP_ERR_INVALID_ARG;
    }

    // Build request: [addr][func=0x04][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
    uint8_t tx[8];
    tx[0] = (uint8_t)NILAN_SLAVE_ADDR;
    tx[1] = 0x04; // Read Input Registers
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);

    uint16_t crc = modbus_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);        // CRC low
    tx[7] = (uint8_t)((crc >> 8) & 0xFF); // CRC high

    // Clear any stale data and send request
    uart_flush_input(NILAN_UART_PORT);
    int written = uart_write_bytes(NILAN_UART_PORT, (const char *)tx, sizeof(tx));
    if (written != (int)sizeof(tx)) {
        ESP_LOGW(TAG, "UART write short: %d / %d", written, (int)sizeof(tx));
    }
    // Optionally: ESP_LOG_BUFFER_HEXDUMP(TAG, tx, sizeof(tx), ESP_LOG_DEBUG);

    // Expected normal response length:
    // [addr][func][byte_count][data ...][crc_lo][crc_hi]
    const int expected_len = 3 + (qty * 2) + 2;
    if (expected_len > NILAN_UART_BUF_SIZE) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_LENGTH;
        }
        return ESP_ERR_NO_MEM;
    }

    uint8_t rx[NILAN_UART_BUF_SIZE];
    memset(rx, 0, sizeof(rx));

    int len = uart_read_bytes(NILAN_UART_PORT,
                              rx,
                              expected_len,
                              pdMS_TO_TICKS(500)); // 500 ms timeout

    if (len <= 0) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_TIMEOUT;
        }
        return ESP_ERR_TIMEOUT;
    }

    // Minimal frame is 5 bytes: addr, func, byte_count/exception, crc_lo, crc_hi
    if (len < 5) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_LENGTH;
        }
        return ESP_FAIL;
    }

    uint8_t addr  = rx[0];
    uint8_t func  = rx[1];
    uint8_t bcnt  = rx[2];

    // Check address
    if (addr != (uint8_t)NILAN_SLAVE_ADDR) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_ADDR;
        }
        return ESP_FAIL;
    }

    // Exception frame: function | 0x80, then exception code
    if (func & 0x80) {
        uint8_t ex_code = (len >= 4) ? rx[2] : 0xFF;
        ESP_LOGW(TAG, "Modbus exception: func=0x%02X code=0x%02X", func, ex_code);
        if (err_out) {
            *err_out = NILAN_MB_ERR_EXCEPTION;
        }
        return ESP_FAIL;
    }

    // Must be function 0x04
    if (func != 0x04) {
        if (err_out) {
            *err_out = NILAN_MB_ERR_FUNC;
        }
        return ESP_FAIL;
    }

    // Check byte count
    if (bcnt != (uint8_t)(qty * 2)) {
        ESP_LOGW(TAG, "Modbus bad byte count: got %u expected %u",
                 (unsigned)bcnt, (unsigned)(qty * 2));
        if (err_out) {
            *err_out = NILAN_MB_ERR_LENGTH;
        }
        return ESP_FAIL;
    }

    // Check total length (3 header + data + 2 crc)
    if (len != (3 + bcnt + 2)) {
        ESP_LOGW(TAG, "Modbus bad length: got %d expected %d",
                 len, 3 + bcnt + 2);
        if (err_out) {
            *err_out = NILAN_MB_ERR_LENGTH;
        }
        return ESP_FAIL;
    }

    // Verify CRC on full frame minus last 2 bytes
    uint16_t rx_crc   = (uint16_t)rx[len - 2] | ((uint16_t)rx[len - 1] << 8);
    uint16_t calc_crc = modbus_crc16(rx, (uint16_t)(len - 2));
    if (rx_crc != calc_crc) {
        ESP_LOGW(TAG, "Modbus CRC mismatch: rx=0x%04X calc=0x%04X",
                 rx_crc, calc_crc);
        if (err_out) {
            *err_out = NILAN_MB_ERR_CRC;
        }
        return ESP_FAIL;
    }

    //ESP_LOG_BUFFER_HEXDUMP(TAG, rx, len, ESP_LOG_DEBUG);

    // Extract registers
    for (uint16_t i = 0; i < qty; ++i) {
        uint16_t hi = rx[3 + (i * 2)];
        uint16_t lo = rx[3 + (i * 2) + 1];
        regs[i] = (uint16_t)((hi << 8) | lo);
    }

    return ESP_OK;
}

// ---------------- Polling task ----------------

static void nilan_poll_task(void *arg)
{
    (void)arg;

    uint16_t regs[2];

    while (1) {
        nilan_mb_err_t errcode = NILAN_MB_ERR_NONE;
        esp_err_t err = nilan_read_input_regs(NILAN_REG_TANK_TOP,
                                              2,
                                              regs,
                                              &errcode);
        uint32_t now_ms = get_time_ms();

        if (err == ESP_OK) {
            // Docs: values are in 0.01 °C, signed
            s_tank_top_cC = (int16_t)regs[0];
            s_tank_bot_cC = (int16_t)regs[1];

            s_ok_count++;
            s_last_ok_ms = now_ms;
            s_last_err   = NILAN_MB_ERR_NONE;

            ESP_LOGD(TAG, "Poll OK: top=%d cC, bot=%d cC",
                     (int)s_tank_top_cC, (int)s_tank_bot_cC);
        } else {
            s_fail_count++;
            s_last_err = errcode;
            ESP_LOGW(TAG, "Poll failed: esp_err=0x%x, mb_err=%d",
                     (unsigned)err, (int)errcode);
        }

        vTaskDelay(pdMS_TO_TICKS(NILAN_POLL_INTERVAL_MS));
    }
}

// ---------------- Public API ----------------

bool nilan_modbus_start(void)
{
    if (s_started) {
        return true;
    }

    // Configure UART
    uart_config_t cfg = {
        .baud_rate = NILAN_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,   // 8E1 matches Arduino SERIAL_8E1
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(NILAN_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(NILAN_UART_PORT,
                                 NILAN_UART_TX_GPIO,
                                 NILAN_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(NILAN_UART_PORT,
                                        NILAN_UART_BUF_SIZE,
                                        NILAN_UART_BUF_SIZE,
                                        0,
                                        NULL,
                                        0));

    // Create polling task
    BaseType_t res = xTaskCreate(
        nilan_poll_task,
        "nilan_poll",
        4096,
        NULL,
        8,
        &s_poll_task);

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create poll task");
        return false;
    }

    s_started = true;
    return true;
}

// Has the module seen at least one valid reply recently?
bool nilan_modbus_is_online(void)
{
    uint32_t last = s_last_ok_ms;
    if (last == 0) {
        return false; // never seen a good frame
    }

    uint32_t now = get_time_ms();
    uint32_t age = now - last;
    return (age <= NILAN_OFFLINE_TIMEOUT_MS);
}

int16_t nilan_get_tank_top_cC(void)
{
    return s_tank_top_cC;
}

int16_t nilan_get_tank_bottom_cC(void)
{
    return s_tank_bot_cC;
}

// Extra debug helpers
uint32_t nilan_modbus_get_ok_count(void)
{
    return s_ok_count;
}

uint32_t nilan_modbus_get_fail_count(void)
{
    return s_fail_count;
}

nilan_mb_err_t nilan_modbus_get_last_error(void)
{
    return s_last_err;
}

float nilan_modbus_get_secs_since_last_ok(void)
{
    if (s_last_ok_ms == 0) {
        return -1.0f; // never
    }
    uint32_t now = get_time_ms();
    uint32_t diff = now - s_last_ok_ms;
    return (float)diff / 1000.0f;
}
