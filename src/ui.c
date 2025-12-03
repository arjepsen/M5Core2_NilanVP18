// ui.c
#include "ui.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "ui_screens/ui_main.h"
#include "ui_screens/ui_modbus_debug.h"

static lv_obj_t *s_tv = NULL;

void ui_init(void)
{
    lvgl_port_lock(0);

    // Root tileview
    s_tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(s_tv, 320, 240);
    lv_obj_set_style_bg_color(s_tv, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_COVER, 0);
    // NOTE: do NOT clear LV_OBJ_FLAG_SCROLLABLE -> we want swiping

    // Three horizontal tiles: 0, 1, 2
    lv_obj_t *t0 = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    lv_obj_t *t1 = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    lv_obj_t *t2 = lv_tileview_add_tile(s_tv, 2, 0, LV_DIR_HOR);

    // Screen 1: main VP18 UI (leave implementation in ui_main.c)
    ui_main_create(t0);

    // Screen 2: Modbus / Nilan debug UI
    ui_modbus_debug_create(t1);

    // Screen 3: simple placeholder for now
    lv_obj_set_style_bg_color(t2, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(t2, LV_OPA_COVER, 0);
    lv_obj_t *lbl = lv_label_create(t2);
    lv_label_set_text(lbl, "Screen 3");
    lv_obj_center(lbl);

    // Start on screen 1
    lv_obj_set_tile_id(s_tv, 0, 0, LV_ANIM_OFF);

    lvgl_port_unlock();
}
