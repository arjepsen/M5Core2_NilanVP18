#include "nilan_modbus.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "CRC16.h"

static const char *TAG = "nilan_modbus";

// ---------------- UART / Modbus configuration ----------------
//
// Using Core2 Port A with the RS485 module:
//
//   GPIO32 -> Unit RX (Core2 TX)
//   GPIO33 -> Unit TX (Core2 RX)
//
// Arduino sketch used:
//   Serial2.begin(19200, SERIAL_8E1, RS485_RX_PIN, RS485_TX_PIN);
//   RS485_RX_PIN = 33, RS485_TX_PIN = 32
//
// So here:
//   TX = GPIO32, RX = GPIO33 on UART1
//
#define NILAN_UART_PORT UART_NUM_1
#define NILAN_UART_TX_GPIO GPIO_NUM_32
#define NILAN_UART_RX_GPIO GPIO_NUM_33
#define NILAN_UART_BAUDRATE 19200
#define NILAN_UART_BUF_SIZE 256

// Nilan defaults
#define NILAN_SLAVE_ADDR 30 // CTS 602 default Modbus address

// Background poll: tank top/bottom (input registers 211 / 212)
#define NILAN_REG_TANK_TOP 211
#define NILAN_POLL_INTERVAL_MS 3000    // ~3 s, as discussed
#define NILAN_OFFLINE_TIMEOUT_MS 10000 // 10 s without OK => OFFLINE

// ---------------- Internal state ----------------

static bool s_started = false;
static TaskHandle_t s_poll_task = NULL;

// UART mutex to serialize all Modbus transactions
static SemaphoreHandle_t s_uart_mutex = NULL;

// Latest cached temperatures (centi-degC, signed)
static int16_t s_tank_top_cC = 0;
static int16_t s_tank_bot_cC = 0;

// Online / stats
static uint32_t s_ok_count = 0;
static uint32_t s_fail_count = 0;
static uint32_t s_last_ok_ms = 0; // 0 = never
static nilan_mb_err_t s_last_err = NILAN_MB_ERR_NONE;




// ====================================================
// PROTOTYPES
// ====================================================
static inline uint32_t get_time_ms();


// ====================================================
// IMPLEMENTATIONS
// ====================================================



bool nilan_read_regs(uint8_t func, uint16_t start, uint16_t qty, uint16_t *regs)
{
    bool crc_ok = false;

    // Build request: [addr][func][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
    const size_t TX_BYTE_COUNT = 8;

    uint8_t tx[TX_BYTE_COUNT];
    tx[0] = (uint8_t)NILAN_SLAVE_ADDR;  // 0x30
    tx[1] = func;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);

    uint16_t crc = modbus_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);        // CRC low
    tx[7] = (uint8_t)((crc >> 8) & 0xFF); // CRC high

    // Expected normal response length:  [addr][func][byte_count][data...][crc_lo][crc_hi]
    const int EXPECTED_RESPONSE_LENGTH = 5 + (qty * 2);
    int response_length = 0;

    uint8_t rx[NILAN_UART_BUF_SIZE];

    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        // Clear any stale data and send request
        uart_flush_input(NILAN_UART_PORT);

        // Send data.
        uart_write_bytes(NILAN_UART_PORT, (const char *)tx, TX_BYTE_COUNT);

        response_length = uart_read_bytes(NILAN_UART_PORT, rx, EXPECTED_RESPONSE_LENGTH, pdMS_TO_TICKS(500)); // 500 ms timeout

        xSemaphoreGive(s_uart_mutex);
    }

    if (response_length == EXPECTED_RESPONSE_LENGTH)
    {
        // Verify CRC on full frame minus last 2 bytes
        uint16_t rx_crc = (uint16_t)rx[response_length - 2] | ((uint16_t)rx[response_length - 1] << 8);
        uint16_t calc_crc = modbus_crc16(rx, (uint16_t)(response_length - 2));

        crc_ok = rx_crc == calc_crc;

        // Extract registers
        for (uint16_t i = 0; i < qty; ++i)
        {
            uint16_t hi = rx[3 + (i * 2)];
            uint16_t lo = rx[3 + (i * 2) + 1];
            regs[i] = (uint16_t)((hi << 8) | lo);
        }
    }

    return crc_ok;
}

// // Low-level: write single holding register (0x06). Assumes we hold mutex.
// static esp_err_t nilan_write_single_reg(uint16_t reg, uint16_t value)
// {
//     const size_t BYTE_COUNT = 8;

//     uint8_t tx[BYTE_COUNT];
//     tx[0] = (uint8_t)NILAN_SLAVE_ADDR;
//     tx[1] = 0x06; // Write Single Register
//     tx[2] = (uint8_t)(reg >> 8);
//     tx[3] = (uint8_t)(reg & 0xFF);
//     tx[4] = (uint8_t)(value >> 8);
//     tx[5] = (uint8_t)(value & 0xFF);

//     uint16_t crc = modbus_crc16(tx, 6);
//     tx[6] = (uint8_t)(crc & 0xFF);
//     tx[7] = (uint8_t)((crc >> 8) & 0xFF);

//     uart_flush_input(NILAN_UART_PORT);
//     uart_write_bytes(NILAN_UART_PORT, (const char *)tx, BYTE_COUNT);

//     // Response is an exact echo of the request (addr, func, reg, value, crc)
//     uint8_t rx[BYTE_COUNT];
//     uart_read_bytes(NILAN_UART_PORT, rx, BYTE_COUNT, pdMS_TO_TICKS(500));

//     uint16_t rx_crc = (uint16_t)rx[6] | ((uint16_t)rx[7] << 8);
//     uint16_t calc_crc = modbus_crc16(rx, 6);
//     if (rx_crc != calc_crc)
//     {
//         return ESP_FAIL;
//     }

//     return ESP_OK;
// }

// ---------------- Polling task ----------------

static void nilan_poll_task(void *arg)
{
    (void)arg;

    uint16_t regs[2];

    while (1)
    {
        // Serialize with any UI-triggered Modbus call
        if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            nilan_mb_err_t errcode = NILAN_MB_ERR_NONE;
            esp_err_t err = nilan_read_regs(0x04, // Input registers
                                            NILAN_REG_TANK_TOP,
                                            2,
                                            regs);
            xSemaphoreGive(s_uart_mutex);

            uint32_t now_ms = get_time_ms();

            if (err == ESP_OK)
            {
                // Docs: values are in 0.01 °C, signed
                s_tank_top_cC = (int16_t)regs[0];
                s_tank_bot_cC = (int16_t)regs[1];

                s_ok_count++;
                s_last_ok_ms = now_ms;
                s_last_err = NILAN_MB_ERR_NONE;

                // ESP_LOGD(TAG, "Poll OK: top=%d cC, bot=%d cC", (int)s_tank_top_cC, (int)s_tank_bot_cC);
            }
            else
            {
                s_fail_count++;
                s_last_err = errcode;
                ESP_LOGW(TAG, "Poll failed\n");
            }
        }
        else
        {
            // Could not get mutex; treat as "internal" failure
            s_fail_count++;
            s_last_err = NILAN_MB_ERR_INTERNAL;
        }

        vTaskDelay(pdMS_TO_TICKS(NILAN_POLL_INTERVAL_MS));
    }
}

// ---------------- Public API ----------------

bool nilan_modbus_start(void)
{
    if (s_started)
    {
        return true;
    }

    // Mutex first
    s_uart_mutex = xSemaphoreCreateMutex();
    // if (!s_uart_mutex)
    // {
    //     ESP_LOGE(TAG, "Failed to create UART mutex");
    //     return false;
    // }

    // Configure UART
    uart_config_t cfg = {
        .baud_rate = NILAN_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN, // 8E1 matches Arduino SERIAL_8E1
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_param_config(NILAN_UART_PORT, &cfg);
    uart_set_pin(NILAN_UART_PORT,
                 NILAN_UART_TX_GPIO,
                 NILAN_UART_RX_GPIO,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    uart_driver_install(NILAN_UART_PORT,
                        NILAN_UART_BUF_SIZE,
                        NILAN_UART_BUF_SIZE,
                        0,
                        NULL,
                        0);


    // Create polling task
    xTaskCreate(
        nilan_poll_task,
        "nilan_poll",
        4096,
        NULL,
        8,
        &s_poll_task);

    s_started = true;
    //ESP_LOGI(TAG, "Nilan Modbus started (UART1 on GPIO32/33, 19200 8E1)");
    return true;
}

// Has the module seen at least one valid reply recently?
bool nilan_modbus_is_online(void)
{
    uint32_t last = s_last_ok_ms;
    if (last == 0)
    {
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
    if (s_last_ok_ms == 0)
    {
        return -1.0f; // never
    }
    uint32_t now = get_time_ms();
    uint32_t diff = now - s_last_ok_ms;
    return (float)diff / 1000.0f;
}

// --------- Generic access wrappers (public API) ----------

bool nilan_modbus_read_input_block(uint16_t start_reg, uint16_t qty, uint16_t *out_regs)
{
    bool response = false;
    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        response = nilan_read_regs(0x04, start_reg, qty, out_regs);
        xSemaphoreGive(s_uart_mutex);
    }
    return response;
}

bool nilan_modbus_read_holding_block(uint16_t start_reg, uint16_t qty, uint16_t *out_regs)
{
    bool response = false;
    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        response = nilan_read_regs(0x03, start_reg, qty, out_regs);
        xSemaphoreGive(s_uart_mutex);
    }
    return response;
}

// bool nilan_modbus_write_single_holding(uint16_t reg,
//                                        uint16_t value)
// {
//     if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
//     {
//         nilan_write_single_reg(reg, value);
//         xSemaphoreGive(s_uart_mutex);
//     }
// }


// ===============================================================
// HELPERS
// ===============================================================
static inline uint32_t get_time_ms()
{
    // xTaskGetTickCount() is 32-bit, wraparound-safe.
    return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}