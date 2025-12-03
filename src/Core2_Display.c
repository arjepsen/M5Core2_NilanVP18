#include "Core2_Display.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"

// Pick a buffer height in lines.
// 80 lines = ~25% of screen, a good smoothness sweet spot.
#define CORE2_BUF_LINES 80

// static i2c_master_dev_handle_t axp192_handle = NULL;
// static bool axp192_initialized = false;
static bool display_initialized = false;

void display_init()
{
    if (display_initialized)
        return;

    if (!axp192_initialized)
        axp192_init();

    // Set up config for the display object.
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CORE2_BUF_LINES, // buffer_size is in *pixels* (not bytes!)
        .double_buffer = true,
        .flags = {
            .buff_dma = true,     // DMA-capable buffer (fast) :contentReference[oaicite:1]{index=1}
            .buff_spiram = false, // keep in internal RAM for DMA reliability
        }};

    // Start display + LVGL with our tuned buffers
    bsp_display_start_with_config(&cfg); // :contentReference[oaicite:2]{index=2}

    // Backlight on - somwhat dim.
    set_backlight_level(0x50);

    display_initialized = true;
}


void display_set_bg_hex(uint32_t hex)
{
    // LVGL runs in its own task; lock around LVGL calls
    if (lvgl_port_lock(0))
    {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_hex(hex), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lvgl_port_unlock();
    }
}

