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

#include "NilanRegisters.h"

// static const char *TAG = "nilan_modbus";

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
#define NILAN_POLL_INTERVAL_MS 3000    // Poll interval in ms
#define NILAN_OFFLINE_TIMEOUT_MS 10000 // 10 s without OK => OFFLINE

// ====================================================
// TYPEDEFS
// ====================================================

typedef struct
{
    uint8_t reg_type;    // 3 = holding, 4 = input (your reg_type)
    uint16_t start_addr; // Modbus start address for this block
    uint16_t qty;        // number of registers read in one shot

    // Filled at boot:
    uint8_t id_count; // how many nilan_reg_id in this group
    uint8_t *id_list; // dynamically allocated [id_count] entries
} nilan_poll_group_t;

// ====================================================
// VARIABLES
// ====================================================

// // Latest cached temperatures (centi-degC, signed)
// static int16_t s_tank_top_cC = 0;
// static int16_t s_tank_bot_cC = 0;

// Online / stats
static uint32_t ok_count = 0;
static uint32_t fail_count = 0;
static uint32_t last_ok_ms = 0; // 0 = never
static nilan_mb_err_t last_err = NILAN_MB_ERR_NONE;

static volatile uint32_t nilan_fault_bitmap = 0;

static bool modbus_started = false;
static TaskHandle_t poll_task = NULL;

// UART mutex to serialize all Modbus transactions
static SemaphoreHandle_t uart_mutex = NULL;

// Set up array of poll-group structs. We'll fill 'em out dynamically at runtime.
static nilan_poll_group_t poll_groups[] = {
    // Input ranges
    {NILAN_INPUT_REG, 100, 16, 0, NULL}, // Discrete I/O - on/off's
    {NILAN_INPUT_REG, 200, 23, 0, NULL}, // Analog I/O - temperatures.
    {NILAN_INPUT_REG, 400, 10, 0, NULL}, // Alarms
    {NILAN_INPUT_REG, 1000, 4, 0, NULL}, // System control/state
    {NILAN_INPUT_REG, 1100, 5, 0, NULL}, // Airflow - fan steps, filters.
    {NILAN_INPUT_REG, 1200, 7, 0, NULL}, // Air Temperatures

    // Holding ranges
    {NILAN_HOLDING_REG, 100, 28, 0, NULL},
    {NILAN_HOLDING_REG, 200, 6, 0, NULL},
    {NILAN_HOLDING_REG, 300, 6, 0, NULL},   // Time/Clock
    {NILAN_HOLDING_REG, 600, 6, 0, NULL},   // User function (1)
    {NILAN_HOLDING_REG, 610, 6, 0, NULL},   // User function (2)
    {NILAN_HOLDING_REG, 1000, 7, 0, NULL},
    {NILAN_HOLDING_REG, 1100, 5, 0, NULL},
    {NILAN_HOLDING_REG, 1200, 8, 0, NULL},
    {NILAN_HOLDING_REG, 1700, 2, 0, NULL}, // Tank temperature setpoints
    {NILAN_HOLDING_REG, 1910, 4, 0, NULL}, // Air quality
};

static const size_t POLL_GROUP_COUNT = sizeof(poll_groups) / sizeof(poll_groups[0]);



// ====================================================
// PROTOTYPES
// ====================================================
static inline uint32_t get_time_ms();
static void init_poll_groups();

// ====================================================
// IMPLEMENTATIONS
// ====================================================

bool nilan_read_regs(uint8_t reg_type, uint16_t start, uint16_t qty, uint16_t *regs)
{
    bool crc_ok = false;

    // Build request: [addr][func][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
    const size_t TX_BYTE_COUNT = 8;

    uint8_t tx[TX_BYTE_COUNT];
    tx[0] = (uint8_t)NILAN_SLAVE_ADDR; // 0x30
    tx[1] = reg_type;
    tx[2] = (uint8_t)(start >> 8);
    tx[3] = (uint8_t)(start & 0xFF);
    tx[4] = (uint8_t)(qty >> 8);
    tx[5] = (uint8_t)(qty & 0xFF);

    uint16_t crc = modbus_crc16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);        // CRC low
    tx[7] = (uint8_t)((crc >> 8) & 0xFF); // CRC high

    // Expected normal response length:  [addr][func][byte_count][data...][crc_lo][crc_hi]
    const uint32_t EXPECTED_RESPONSE_LENGTH = 5 + (qty * 2); // addr + func + count + crc = 5 bytes. Each register is 16 bit, hence the *2
    int response_length = 0;

    uint8_t rx[NILAN_UART_BUF_SIZE];

    if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        // Clear any stale data and send request
        // uart_flush_input(NILAN_UART_PORT);

        // Send data.
        uart_write_bytes(NILAN_UART_PORT, (const char *)tx, TX_BYTE_COUNT);

        response_length = uart_read_bytes(NILAN_UART_PORT, rx, EXPECTED_RESPONSE_LENGTH, pdMS_TO_TICKS(500)); // 500 ms timeout

        xSemaphoreGive(uart_mutex);
    }

    if (response_length == EXPECTED_RESPONSE_LENGTH)
    {
        // Verify CRC on full frame minus last 2 bytes
        uint16_t rx_crc = (uint16_t)rx[response_length - 2] | ((uint16_t)rx[response_length - 1] << 8);
        uint16_t calc_crc = modbus_crc16(rx, (uint16_t)(response_length - 2));

        crc_ok = (rx_crc == calc_crc);
        if (crc_ok)
        {
            // Extract registers
            for (uint16_t i = 0; i < qty; ++i)
            {
                uint16_t hi = rx[3 + (i * 2)];
                uint16_t lo = rx[4 + (i * 2)];
                regs[i] = (uint16_t)((hi << 8) | lo);
            }
        }
    }

    return crc_ok;
}

// ---------------- Polling task ----------------

static void nilan_poll_task(void *arg)
{
    (void)arg; // Silence the unused parameter warning.

    uint16_t regs_data[36];  // [addr][func][byte_count][data...][crc_lo][crc_hi] - arbitrarily large(36) to hold enough registers.
    size_t group_index = 0;

    while (1)
    {
        nilan_poll_group_t *grp = &poll_groups[group_index];    // pointer to the (next) poll group.

        // Read all registers in group, and place their raw value in the regs array.
        bool crc_ok = nilan_read_regs(grp->reg_type, grp->start_addr, grp->qty, regs_data);  // Read all consecutive registers in group range.

        if (crc_ok)
        {
            uint32_t now_ms = get_time_ms();

            // iterate through each known ID in group, and update its state.
            for (uint8_t i = 0; i < grp->id_count; i++)
            {
                uint8_t index = grp->id_list[i];
                const nilan_reg_meta_t *register_meta = &nilan_registers[index];

                // Calculate offset to index into the regs_data array.
                uint16_t offset = (uint16_t)(register_meta->addr - grp->start_addr);

                // Update the state array struct for this register.
                nilan_reg_state[index].raw = regs_data[offset];
                nilan_reg_state[index].timestamp_ms = now_ms;
                nilan_reg_state[index].valid = 1;
            }

            ok_count++;
            last_ok_ms = now_ms;
            last_err = NILAN_MB_ERR_NONE;
        }
        else
        {
            fail_count++;
            last_err = NILAN_MB_ERR_CRC;
        }

        // Rotate to next group.
        group_index++;
        if (group_index >= POLL_GROUP_COUNT)
        {
            group_index = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(NILAN_POLL_INTERVAL_MS));
    }
}

// ---------------- Public API ----------------

bool nilan_modbus_start(void)
{
    if (modbus_started)
    {
        return true;
    }

    // Mutex first
    uart_mutex = xSemaphoreCreateMutex();

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

    init_poll_groups();

    // Create polling task
    xTaskCreate(
        nilan_poll_task,
        "nilan_poll",
        4096,
        NULL,
        8,
        &poll_task);

    modbus_started = true;
    // ESP_LOGI(TAG, "Nilan Modbus started (UART1 on GPIO32/33, 19200 8E1)");
    return true;
}

// Has the module seen at least one valid reply recently?
bool nilan_modbus_is_online(void)
{
    uint32_t last = last_ok_ms;
    if (last == 0)
    {
        return false; // never seen a good frame
    }

    uint32_t now = get_time_ms();
    uint32_t age = now - last;
    return (age <= NILAN_OFFLINE_TIMEOUT_MS);
}

int16_t nilan_get_tank_top_cC()
{
    // uint16_t raw_value = nilan_reg_state[NILAN_REGID_IR_T11_TANK_TOP].raw;

    return nilan_reg_state[NILAN_REGID_IR_T11_TANK_TOP].raw;
}

int16_t nilan_get_tank_bottom_cC(void)
{
    return nilan_reg_state[NILAN_REGID_IR_T12_TANK_BOTTOM].raw;
}

// Extra debug helpers
uint32_t nilan_modbus_get_ok_count(void)
{
    return ok_count;
}

uint32_t nilan_modbus_get_fail_count(void)
{
    return fail_count;
}

nilan_mb_err_t nilan_modbus_get_last_error(void)
{
    return last_err;
}

float nilan_modbus_get_secs_since_last_ok(void)
{
    if (last_ok_ms == 0)
    {
        return -1.0f; // never
    }
    uint32_t now = get_time_ms();
    uint32_t diff = now - last_ok_ms;
    return (float)diff / 1000.0f;
}

// --------- Generic access wrappers (public API) ----------

bool nilan_modbus_read_input_block(uint16_t start_reg, uint16_t qty, uint16_t *out_regs)
{
    return nilan_read_regs(0x04, start_reg, qty, out_regs);
}

bool nilan_modbus_read_holding_block(uint16_t start_reg, uint16_t qty, uint16_t *out_regs)
{
    return nilan_read_regs(0x03, start_reg, qty, out_regs);
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

static void init_poll_groups()
{
    // Iterate over all the groups we set up above.
    for (size_t grp_id = 0; grp_id < POLL_GROUP_COUNT; ++grp_id)    
    {   
        nilan_poll_group_t *grp = &poll_groups[grp_id]; // pointer/alias to the group for shorter lines.

        uint16_t start = grp->start_addr;
        uint16_t end   = (uint16_t)(start + grp->qty); // exclusive

        // First pass: count how many registers fall in this range
        uint8_t count = 0;
        bool found_first_reg_id = false;
        size_t first_reg_id = 0;
        size_t last_reg_id = 0;

        for (size_t reg_id = 0; reg_id < NILAN_REGID_COUNT; ++reg_id)
        {
            const nilan_reg_meta_t *reg_meta = &nilan_registers[reg_id];   // pointer/alias to the register meta data struct

            if (reg_meta->reg_type != grp->reg_type)
                continue;

            if (reg_meta->addr < start || reg_meta->addr >= end)
                continue;

            if (!found_first_reg_id)
            {
                first_reg_id = reg_id;
                found_first_reg_id = true;
            }

            last_reg_id = reg_id;
            ++count;
        }

        grp->id_count = count;

        if (count == 0)
        {
            grp->id_list = NULL;
            continue;  // nothing to track in this group (still OK to read on wire)
        }


        // Second pass: actually fill the ID list
        grp->id_list = malloc(count);   // Allocate space for the array.
        uint8_t idx = 0;

        for (size_t reg_id = first_reg_id; reg_id <= last_reg_id; ++reg_id)
        {
            const nilan_reg_meta_t *reg_meta = &nilan_registers[reg_id];

            if (reg_meta->reg_type != grp->reg_type)
                continue;

            if (reg_meta->addr < start || reg_meta->addr >= end)
                continue;

            grp->id_list[idx++] = (uint8_t)reg_id;
        }
    }
}
