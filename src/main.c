#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "AXP192.h"
#include "Core2_Display.h"
#include "RTC.h"
#include "UI/UI.h"

#include "Nilan_Modbus.h"
#include "wifi_sta.h"

#include "bsp/esp-bsp.h"

void app_main()
{
    axp192_init(); // Set up axp192 handle.
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
