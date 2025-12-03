#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "AXP192.h"
#include "RTC.h"
#include "Core2_Display.h"

//#include "core2_bringup.h"
#include "ui.h"

#include "wifi_sta.h"
#include "nilan_modbus.h"

#include "bsp/esp-bsp.h"

// // TODO: move to menuconfig later; hardcode for now
// #define WIFI_SSID "YOUR_SSID"
// #define WIFI_PASS "YOUR_PASS"

void app_main()
{
    axp192_init();  // Set up axp192 handle.
    core2_RTC_init();

    display_init();
    display_set_bg_hex(COLOR_BG_DARK);

    ui_init();


    wifi_sta_start();
    nilan_modbus_start();


    while (1)
    {

        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}
