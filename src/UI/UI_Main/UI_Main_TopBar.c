#include "UI_Main_TopBar.h"
#include "lvgl.h"
#include "../UI_Shared_Colors.h"
#include "../../wifi_sta.h"

// ---------- Static objects ----------
//static lv_obj_t *top_bar = NULL;
lv_obj_t *labl_time = NULL;  // global for timer access

static lv_color_t wifi_bar_on_color;
static lv_color_t wifi_bar_off_color;

// Wifi icon struct
typedef struct {
    lv_obj_t *cont;
    lv_obj_t *bar[4];
    wifi_strength_t last_level;
} wifi_icon_t;

static wifi_icon_t wifi_icon = {0};

// ---------- Prototypes (internal) ----------
void wifi_icon_create(lv_obj_t *parent, int right_margin, uint32_t inactive_hex);
//static void wifi_icon_set_level(wifi_strength_t strength);



// ---------- Public: Create top bar ----------
void ui_top_bar_create(lv_obj_t *parent)
{

    lv_obj_t *top = lv_obj_create(parent);
    lv_obj_set_size(top, 320, 32);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(COL_TOPBAR), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(top, 0, 0); // kills theme padding

    // Service label on the left
    lv_obj_t *lbl_mode = lv_label_create(top);
    lv_label_set_text(lbl_mode, "Service");
    lv_obj_set_style_text_color(lbl_mode, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_mode, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_mode, LV_ALIGN_LEFT_MID, 10, 0);

    // Clock in the center
    labl_time = lv_label_create(top);
    lv_label_set_text(labl_time, "--:--");
    lv_obj_set_style_text_color(labl_time, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_time, &lv_font_montserrat_22, 0);
    lv_obj_align(labl_time, LV_ALIGN_CENTER, 0, 0);

    // Wifi Icon
    wifi_bar_on_color = lv_color_hex(COL_WIFI_BAR_ON);
    wifi_bar_off_color = lv_color_hex(COL_WIFI_BAR_OFF);
    lv_obj_set_style_pad_all(top, 0, 0);
    wifi_icon_create(top, 20, COL_WIFI_BAR_OFF); // right margin 20, inactive color
    ui_top_bar_update_wifi(wifi_sta_get_signal_strength());
}


// ---------- WiFi icon helpers (moved from ui_main.c) ----------
void wifi_icon_create(lv_obj_t *parent, int right_margin, uint32_t inactive_hex)
{
    const int bar_w = 5;
    const int bar_spacing = 3;
    const int bar_h[4] = {6, 10, 14, 18};

    const int count = 4;
    const int total_w = count * bar_w + (count - 1) * bar_spacing;
    const int max_h = bar_h[3];

    lv_obj_t *c = lv_obj_create(parent);
    wifi_icon.cont = c;
    wifi_icon.last_level = 255;

    lv_obj_set_size(c, total_w, max_h);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -right_margin, 0);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(c);
        wifi_icon.bar[i] = b;

        lv_obj_set_size(b, bar_w, bar_h[i]);
        lv_obj_set_pos(b, i * (bar_w + bar_spacing), max_h - bar_h[i]);
        lv_obj_set_style_radius(b, 2, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(inactive_hex), 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }
}

void ui_top_bar_update_wifi(wifi_strength_t strength)
{
    if (wifi_icon.last_level == strength) return;

    for (int i = 0; i < 4; i++) 
    {
        lv_color_t bar_color = (i < strength) ? wifi_bar_on_color : wifi_bar_off_color;
        lv_obj_set_style_bg_color(wifi_icon.bar[i], bar_color, 0);
    }

    wifi_icon.last_level = strength;
}


