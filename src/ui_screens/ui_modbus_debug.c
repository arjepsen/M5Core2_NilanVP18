#include "ui_modbus_debug.h"
#include "lvgl.h"
#include "nilan_modbus.h"

typedef enum
{
    REG_FMT_TEMP_CENTI_S16 = 0, // signed, in 0.01 °C
    REG_FMT_UINT16,             // generic unsigned
    REG_FMT_INT16               // generic signed
} reg_fmt_t;

typedef struct
{
    const char *name;
    uint16_t addr;
    uint8_t reg_type;  // 3 = holding, 4 = input
    uint8_t count; // number of registers (1..2 for now)
    reg_fmt_t fmt;
} reg_desc_t;

// Minimal initial catalog (you can extend this later)
static const reg_desc_t s_reg_table[] = {
    // name          addr  reg_type  count  fmt
    {"Version (1..3)", 1, 4, 3, REG_FMT_UINT16},            // Input regs 1-3
    {"Room temp T15", 215, 4, 1, REG_FMT_TEMP_CENTI_S16},   // Input.T15 (x100°C)
    {"Tank top T11", 211, 4, 1, REG_FMT_TEMP_CENTI_S16},    // Input.T11
    {"Tank bottom T12", 212, 4, 1, REG_FMT_TEMP_CENTI_S16}, // Input.T12
    {"Control.State", 1002, 4, 1, REG_FMT_UINT16},          // Input.Control.State
};

static const int s_reg_count = (int)(sizeof(s_reg_table) / sizeof(s_reg_table[0]));

// UI elements
static lv_obj_t *s_lbl_status = NULL;   // top status bar
static lv_obj_t *s_lbl_selected = NULL; // selected register
static lv_obj_t *s_lbl_value = NULL;    // last read value
static lv_obj_t *s_btn_read = NULL;
static lv_obj_t *s_btn_select = NULL;

// Modal list overlay
static lv_obj_t *s_overlay = NULL;

static int s_current_index = 0;

// ---------- Helpers ----------

static void format_value(char *buf,
                         size_t buf_size,
                         const reg_desc_t *desc,
                         const uint16_t *raw_regs,
                         int raw_count)
{
    if (!desc || !raw_regs || raw_count <= 0 || buf_size == 0)
    {
        if (buf_size > 0) buf[0] = '\0';
        return;
    }

    switch (desc->fmt)
    {
        case REG_FMT_TEMP_CENTI_S16:
        {
            int16_t v = (int16_t)raw_regs[0];
            int whole = v / 100;
            int frac = v % 100;
            if (frac < 0) frac = -frac;
            lv_snprintf(buf, buf_size, "%d.%02d C", whole, frac);
            break;
        }
        case REG_FMT_UINT16:
        {
            if (raw_count == 1)
            {
                lv_snprintf(buf, buf_size, "%u", (unsigned)raw_regs[0]);
            }
            else if (raw_count == 2)
            {
                // Simple "hi:lo" representation
                lv_snprintf(buf, buf_size, "%u / %u",
                            (unsigned)raw_regs[0],
                            (unsigned)raw_regs[1]);
            }
            else
            {
                lv_snprintf(buf, buf_size, "%u (n=%d)",
                            (unsigned)raw_regs[0], raw_count);
            }
            break;
        }
        case REG_FMT_INT16:
        {
            int16_t v = (int16_t)raw_regs[0];
            lv_snprintf(buf, buf_size, "%d", (int)v);
            break;
        }
        default:
            lv_snprintf(buf, buf_size, "n/a");
            break;
    }
}

static void update_selected_label(void)
{
    if (!s_lbl_selected) return;

    const reg_desc_t *d = &s_reg_table[s_current_index];
    lv_label_set_text_fmt(s_lbl_selected,
                          "Selected: %s (addr %u, reg_type 0x%02X, count %u)",
                          d->name,
                          (unsigned)d->addr,
                          (unsigned)d->reg_type,
                          (unsigned)d->count);
}

// ---------- Status timer ----------

static void dbg_status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_lbl_status) return;

    bool online = nilan_modbus_is_online();

    if (online)
    {
        float age = nilan_modbus_get_secs_since_last_ok();
        lv_label_set_text_fmt(s_lbl_status,
                              "Status: ONLINE (last OK %.1fs ago)",
                              (double)age);
    }
    else
    {
        lv_label_set_text(s_lbl_status, "Status: offline");
    }
}

// ---------- Modal list handling ----------

static void overlay_close(void)
{
    if (s_overlay)
    {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
    }
}

static void reg_list_item_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    s_current_index = idx;
    update_selected_label();

    // Also clear the last value (forces a new read)
    if (s_lbl_value)
    {
        lv_label_set_text(s_lbl_value, "Value: (not read yet)");
    }

    (void)btn;
    overlay_close();
}

static void select_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_overlay)
    {
        // Already open; do nothing
        return;
    }

    // parent is the tile; grab it from button
    lv_obj_t *tile = lv_obj_get_parent(s_btn_select);
    s_overlay = lv_obj_create(tile);
    lv_obj_set_size(s_overlay, 320, 240);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Simple list in the center
    lv_obj_t *list = lv_list_create(s_overlay);
    lv_obj_set_size(list, 280, 180);
    lv_obj_center(list);

    for (int i = 0; i < s_reg_count; ++i)
    {
        const reg_desc_t *d = &s_reg_table[i];
        lv_obj_t *btn = lv_list_add_btn(list, NULL, d->name);
        lv_obj_add_event_cb(btn,
                            reg_list_item_event_cb,
                            LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    // If you tap outside the list, close overlay
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_overlay, (void (*)(lv_event_t *))overlay_close, LV_EVENT_CLICKED, NULL);
}

// ---------- Read button ----------

static void read_btn_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_lbl_value) return;

    const reg_desc_t *d = &s_reg_table[s_current_index];

    uint16_t raw[4] = {0};
    //nilan_mb_err_t err = NILAN_MB_ERR_NONE;
    bool modbus_read_ok = false;

    modbus_read_ok = nilan_read_regs(d->reg_type, d->addr, d->count, raw);


    if (modbus_read_ok == false)
    {
        lv_label_set_text_fmt(s_lbl_value, "Read CRC error");
        return;     
    }


    char buf[64];
    format_value(buf, sizeof(buf), d, raw, d->count);
    lv_label_set_text_fmt(s_lbl_value, "Value: %s", buf);
}

// ---------- Screen creation ----------

void ui_modbus_debug_create(lv_obj_t *tile)
{
    // Background
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // Top status bar
    s_lbl_status = lv_label_create(tile);
    lv_label_set_text(s_lbl_status, "Status: ---");
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_TOP_LEFT, 6, 4);

    // Selected register label
    s_lbl_selected = lv_label_create(tile);
    lv_label_set_text(s_lbl_selected, "Selected: (none)");
    lv_obj_set_style_text_color(s_lbl_selected, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(s_lbl_selected, LV_ALIGN_TOP_LEFT, 6, 26);

    // Buttons
    s_btn_select = lv_btn_create(tile);
    lv_obj_set_size(s_btn_select, 120, 32);
    lv_obj_align(s_btn_select, LV_ALIGN_TOP_LEFT, 6, 54);
    lv_obj_t *lbl_sel = lv_label_create(s_btn_select);
    lv_label_set_text(lbl_sel, "Select register");
    lv_obj_center(lbl_sel);
    lv_obj_add_event_cb(s_btn_select, select_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_btn_read = lv_btn_create(tile);
    lv_obj_set_size(s_btn_read, 80, 32);
    lv_obj_align(s_btn_read, LV_ALIGN_TOP_RIGHT, -6, 54);
    lv_obj_t *lbl_read = lv_label_create(s_btn_read);
    lv_label_set_text(lbl_read, "Read");
    lv_obj_center(lbl_read);
    lv_obj_add_event_cb(s_btn_read, read_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Value label
    s_lbl_value = lv_label_create(tile);
    lv_label_set_text(s_lbl_value, "Value: (not read yet)");
    lv_obj_set_style_text_color(s_lbl_value, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_value, LV_ALIGN_TOP_LEFT, 6, 94);

    // Initialize selected label
    s_current_index = 0;
    update_selected_label();

    // Periodic status update timer (500 ms, only uses cached data)
    lv_timer_create(dbg_status_timer_cb, 500, NULL);
}
