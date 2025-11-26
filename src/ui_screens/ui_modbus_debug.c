#include "ui_modbus_debug.h"

#include "lvgl.h"
#include "nilan_modbus.h"

#include <inttypes.h>

static lv_obj_t *s_lbl_title    = NULL;
static lv_obj_t *s_lbl_status   = NULL;
static lv_obj_t *s_lbl_top      = NULL;
static lv_obj_t *s_lbl_bottom   = NULL;
static lv_obj_t *s_lbl_counts   = NULL;
static lv_obj_t *s_lbl_last_ok  = NULL;
static lv_obj_t *s_lbl_last_err = NULL;

static const char *err_to_text(nilan_mb_err_t err)
{
    switch (err) {
    case NILAN_MB_ERR_NONE:      return "none";
    case NILAN_MB_ERR_TIMEOUT:   return "timeout";
    case NILAN_MB_ERR_CRC:       return "crc_fail";
    case NILAN_MB_ERR_LENGTH:    return "length";
    case NILAN_MB_ERR_ADDR:      return "addr_mismatch";
    case NILAN_MB_ERR_FUNC:      return "bad_func";
    case NILAN_MB_ERR_EXCEPTION: return "exception";
    default:                     return "unknown";
    }
}

static void dbg_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_lbl_status || !s_lbl_top || !s_lbl_bottom ||
        !s_lbl_counts || !s_lbl_last_ok || !s_lbl_last_err) {
        return;
    }

    const bool          online        = nilan_modbus_is_online();
    const int16_t       top_cC        = nilan_get_tank_top_cC();
    const int16_t       bot_cC        = nilan_get_tank_bottom_cC();
    const uint32_t      ok_count      = nilan_modbus_get_ok_count();
    const uint32_t      fail_count    = nilan_modbus_get_fail_count();
    const float         secs_since_ok = nilan_modbus_get_secs_since_last_ok();
    const nilan_mb_err_t last_err     = nilan_modbus_get_last_error();

    // Status label
    if (online) {
        lv_label_set_text(s_lbl_status, "Status: ONLINE");
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(s_lbl_status, "Status: OFFLINE");
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFF0000), 0);
    }

    // Temperatures
    if (secs_since_ok < 0.0f) {
        lv_label_set_text(s_lbl_top,    "Tank top: --.- C");
        lv_label_set_text(s_lbl_bottom, "Tank bottom: --.- C");
    } else {
        lv_label_set_text_fmt(s_lbl_top,
                              "Tank top: %.1f C",
                              (double)top_cC / 100.0);
        lv_label_set_text_fmt(s_lbl_bottom,
                              "Tank bottom: %.1f C",
                              (double)bot_cC / 100.0);
    }

    // OK / fail counters
    lv_label_set_text_fmt(s_lbl_counts,
                          "OK:%" PRIu32 "  Fail:%" PRIu32,
                          ok_count, fail_count);

    // Last OK
    if (secs_since_ok < 0.0f) {
        lv_label_set_text(s_lbl_last_ok, "Last OK: never");
    } else {
        lv_label_set_text_fmt(s_lbl_last_ok,
                              "Last OK: %.1f s ago",
                              (double)secs_since_ok);
    }

    // Last error
    lv_label_set_text_fmt(s_lbl_last_err,
                          "Last error: %s",
                          err_to_text(last_err));
}

void ui_modbus_debug_create(lv_obj_t *tile)
{
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

    // Title
    s_lbl_title = lv_label_create(tile);
    lv_label_set_text(s_lbl_title, "Nilan / Modbus debug");
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(0xFFFF00), 0);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 6);

    // Status
    s_lbl_status = lv_label_create(tile);
    lv_label_set_text(s_lbl_status, "Status: --");
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_TOP_LEFT, 8, 28);

    // Tank top
    s_lbl_top = lv_label_create(tile);
    lv_label_set_text(s_lbl_top, "Tank top: --.- C");
    lv_obj_set_style_text_color(s_lbl_top, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_top, LV_ALIGN_TOP_LEFT, 8, 52);

    // Tank bottom
    s_lbl_bottom = lv_label_create(tile);
    lv_label_set_text(s_lbl_bottom, "Tank bottom: --.- C");
    lv_obj_set_style_text_color(s_lbl_bottom, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_bottom, LV_ALIGN_TOP_LEFT, 8, 74);

    // OK / fail counters
    s_lbl_counts = lv_label_create(tile);
    lv_label_set_text(s_lbl_counts, "OK:0  Fail:0");
    lv_obj_set_style_text_color(s_lbl_counts, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_lbl_counts, LV_ALIGN_TOP_LEFT, 8, 100);

    // Last OK
    s_lbl_last_ok = lv_label_create(tile);
    lv_label_set_text(s_lbl_last_ok, "Last OK: --");
    lv_obj_set_style_text_color(s_lbl_last_ok, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(s_lbl_last_ok, LV_ALIGN_TOP_LEFT, 8, 122);

    // Last error
    s_lbl_last_err = lv_label_create(tile);
    lv_label_set_text(s_lbl_last_err, "Last error: --");
    lv_obj_set_style_text_color(s_lbl_last_err, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(s_lbl_last_err, LV_ALIGN_TOP_LEFT, 8, 144);

    // Periodic update timer (500 ms)
    lv_timer_create(dbg_update_timer_cb, 500, NULL);
}
