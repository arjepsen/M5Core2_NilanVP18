#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple Modbus / Nilan status code for debugging purposes.
typedef enum {
    NILAN_MB_ERR_NONE = 0,
    NILAN_MB_ERR_TIMEOUT,
    NILAN_MB_ERR_CRC,
    NILAN_MB_ERR_LENGTH,
    NILAN_MB_ERR_ADDR,
    NILAN_MB_ERR_FUNC,
    NILAN_MB_ERR_EXCEPTION
} nilan_mb_err_t;

// Start Nilan Modbus RTU master.
// Returns true if UART was configured and polling task started.
bool nilan_modbus_start(void);

// Has the module seen at least one valid reply recently?
bool nilan_modbus_is_online(void);

// Latest cached values (centi-degC, i.e. 4850 = 48.50 °C)
int16_t nilan_get_tank_top_cC(void);
int16_t nilan_get_tank_bottom_cC(void);

// Extra debug info for the UI
uint32_t       nilan_modbus_get_ok_count(void);
uint32_t       nilan_modbus_get_fail_count(void);
nilan_mb_err_t nilan_modbus_get_last_error(void);
// Seconds since last successful poll; -1.0f if never
float          nilan_modbus_get_secs_since_last_ok(void);

#ifdef __cplusplus
}
#endif
