#include "UI_InputList.h"
#include "Nilan_Registers.h"
#include "nilan_modbus.h"
#include <stdio.h>

static lv_obj_t *list_obj = NULL;

static void msgbox_close_cb(lv_event_t *e)
{
    lv_msgbox_close(lv_event_get_target(e));
}

static void list_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    nilan_reg_id_t id = (nilan_reg_id_t)(uintptr_t)lv_event_get_user_data(e);
    if (id >= NILAN_REGID_COUNT) return;

    const nilan_reg_meta_t *meta = &nilan_registers[id];

    char value_buf[64] = "N/A";
    nilan_format_reg(id, value_buf, sizeof(value_buf));

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, meta->name ? meta->name : "Register");
    lv_msgbox_add_text(mbox, value_buf);
    lv_msgbox_add_close_button(mbox);

    lv_obj_set_style_text_font(mbox, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_font(lv_msgbox_get_content(mbox), &lv_font_montserrat_28, 0);

    lv_obj_center(mbox);
}

void UI_InputList_init(lv_obj_t *parent)
{
    list_obj = lv_list_create(parent);
    lv_obj_set_size(list_obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_row(list_obj, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list_obj, 6, LV_PART_MAIN);

    UI_InputList_refresh();
}

void UI_InputList_refresh(void)
{
    if (list_obj == NULL || !lv_obj_is_valid(list_obj)) return;

    // Correct LVGL v9 way to clear a list
    lv_obj_clean(list_obj);

    if (!nilan_modbus_is_online())
    {
        lv_obj_t *lbl = lv_list_add_text(list_obj, "Modbus Offline");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_RED), 0);
        return;
    }

    bool has_data = false;

    for (nilan_reg_id_t id = 0; id < NILAN_REGID_COUNT; id++)
    {
        const nilan_reg_meta_t *meta = &nilan_registers[id];
        if (meta->reg_type != NILAN_INPUT_REG) continue;
        if (!nilan_reg_state[id].valid) continue;

        char label[128];
        char value[64];
        nilan_format_reg(id, value, sizeof(value));

        snprintf(label, sizeof(label), "%s: %s", meta->name ? meta->name : "Unnamed", value);

        lv_obj_t *btn = lv_list_add_btn(list_obj, NULL, label);
        if (btn)
        {
            lv_obj_add_event_cb(btn, list_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
            has_data = true;
        }
    }

    if (!has_data)
    {
        lv_list_add_text(list_obj, "No valid input data yet");
    }
}