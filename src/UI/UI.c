// ui.c
#include "ui.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "UI/UI_Main/UI_Main.h"
#include "UI/UI_InputList/UI_InputList.h"

static lv_obj_t *tileview_obj = NULL;

void ui_init()
{
    lvgl_port_lock(0);

    // Root tileview
    tileview_obj = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview_obj, 320, 240);
    lv_obj_set_style_bg_color(tileview_obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tileview_obj, LV_OPA_COVER, 0);
    // NOTE: do NOT clear LV_OBJ_FLAG_SCROLLABLE -> we want swiping

    // Three horizontal tiles: 0, 1, 2
    lv_obj_t *t0 = lv_tileview_add_tile(tileview_obj, 0, 0, LV_DIR_HOR);
    lv_obj_t *t1 = lv_tileview_add_tile(tileview_obj, 1, 0, LV_DIR_HOR);
    lv_obj_t *t2 = lv_tileview_add_tile(tileview_obj, 2, 0, LV_DIR_HOR);

    // Screen 1: main VP18 UI (leave implementation in ui_main.c)
    ui_main_create(t0);

    // Screen 2: Modbus / Nilan debug UI
    UI_InputList_init(t1);

    // Screen 3: simple placeholder for now
    lv_obj_set_style_bg_color(t2, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(t2, LV_OPA_COVER, 0);
    lv_obj_t *lbl = lv_label_create(t2);
    lv_label_set_text(lbl, "Screen 3");
    lv_obj_center(lbl);

    // Start on screen 1
    lv_obj_set_tile_id(tileview_obj, 0, 0, LV_ANIM_OFF);

    lvgl_port_unlock();
}
 