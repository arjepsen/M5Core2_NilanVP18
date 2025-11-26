#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core2_bringup.h"
#include "ui.h"

#include "wifi_sta.h"
#include "nilan_modbus.h"

// // TODO: move to menuconfig later; hardcode for now
// #define WIFI_SSID "YOUR_SSID"
// #define WIFI_PASS "YOUR_PASS"

void app_main(void)
{
    core2_display_init();
    core2_display_set_bg_hex(COLOR_BG_DARK);

    // Make sure Port-A / BUS_5V is powered for the RS485 unit
    core2_bus5v_enable();
    
    ui_init();

    wifi_sta_start();
    nilan_modbus_start();


    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
