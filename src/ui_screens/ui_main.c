#include "ui_main.h"
#include "lvgl.h"
#include "nilan_modbus.h"
#include "wifi_sta.h" // for wifi strength
#include <limits.h>
#include <math.h>
#include <time.h>

#include "esp_log.h"

// ---------- Layout constants for 320x240 ----------
#define TOP_BAR_H 32
#define MAIN_H (240 - TOP_BAR_H)

#define LEFT_W 128
#define RIGHT_W (320 - LEFT_W)

#define PAD 6
#define PAD_TIGHT 4

// ---------- Colors ----------
#define COL_BG 0x202020
#define COL_TOPBAR 0x3A291C
#define COL_TEXT 0xFFFFFF
#define COL_TEXT_DIM 0xB0B0B0
#define COL_BTN_BG 0x262B30
#define COL_BTN_EDGE 0x3A3A3A
#define COL_WIFI_BAR_OFF 0x404040
#define COL_WIFI_BAR_ON 0xB0B0B0

// Tank shell
#define COL_TANK_SHELL 0x2F2F2F
#define COL_TANK_BORDER 0x6A6A6A

// ---------- State ----------
static int vent_step = 3;
static bool power_on = true;

// LVGL objects we update
static lv_obj_t *labl_step = NULL;
static lv_obj_t *labl_tank_top = NULL;
static lv_obj_t *labl_tank_bottom = NULL;
static lv_obj_t *labl_power = NULL;
static lv_obj_t *labl_time = NULL;

// Popup handle (lazy-created)
static lv_obj_t *step_popup = NULL;

static lv_obj_t *tank_water_gradient = NULL; // water gradient rect

// Wifi indicator
typedef struct
{
    lv_obj_t *cont;
    lv_obj_t *bar[4];
    // uint8_t last_level;   // 0..4, 255 = invalid
    wifi_strength_t last_level; // enumeration in wifi_sta.h
} wifi_icon_t;

static lv_color_t wifi_bar_on_color;
static lv_color_t wifi_bar_off_color;
static wifi_icon_t wifi_icon = {0};

typedef struct
{
    uint32_t color_hex;

    uint8_t blade_w;    // arc width for blades
    uint8_t blade_span; // degrees (e.g. 50)
    int16_t blade_off;  // px (e.g. 25)
    int16_t blade_diam; // px (e.g. size_px - 25)

    uint8_t hub_w;    // e.g. 3
    int16_t hub_diam; // e.g. 35
} fan_draw_cfg_t;

// ======================================================
// PROTOTYPES
// ======================================================

static inline void set_label_u8(lv_obj_t *label, int v);
static inline void set_label_temp(lv_obj_t *label, int t_c);
static inline void on_step_tapped(lv_event_t *e);
static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t /*0..255*/);
static uint32_t temp_to_warm_color(int t_c);
static void tank_water_set_gradient(lv_obj_t *water, int top_c, int bot_c);
static void main_status_timer_cb(lv_timer_t *t);
static lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex);
static void power_btn_update();
static void on_power_tapped(lv_event_t *e);
static void popup_close();
static void popup_step_minus(lv_event_t *e);
static void popup_step_plus(lv_event_t *e);
static void popup_open(lv_obj_t *parent);
static void wifi_icon_create(lv_obj_t *parent, int right_margin, uint32_t inactive_hex);
static void wifi_icon_set_level(wifi_strength_t strength);
static lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex);
static void fan_free_event_cb(lv_event_t *e);
static void fan_draw_event_cb(lv_event_t *e);

// ======================================================
// MAIN SCREEN
// ======================================================
void ui_main_create(lv_obj_t *tile)
{
    lv_obj_set_style_bg_color(tile, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // ------------------- TOP BAR --------------------------
    lv_obj_t *top = lv_obj_create(tile);
    lv_obj_set_size(top, 320, 32);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(COL_TOPBAR), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(top, 0, 0); // kills theme padding

    // Service on the left
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
    wifi_icon_set_level(wifi_sta_get_signal_strength());

    // -------------- MAIN AREA CONTAINERS ------------------
    lv_obj_t *left = lv_obj_create(tile);
    lv_obj_set_size(left, LEFT_W, MAIN_H);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_H);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *right = lv_obj_create(tile);
    lv_obj_set_size(right, RIGHT_W, MAIN_H);
    lv_obj_align(right, LV_ALIGN_TOP_LEFT, LEFT_W, TOP_BAR_H);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    // -------------------- FAN ICON ------------------------
    lv_obj_t *fan_card = lv_obj_create(left);
    lv_obj_set_size(fan_card, 80, 80);               // <<< Smaller card (was 96) — tighter outline
    lv_obj_align(fan_card, LV_ALIGN_TOP_MID, 0, 16); // Shift down a bit to keep centered vertically
    lv_obj_set_style_bg_opa(fan_card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fan_card, 2, 0);
    lv_obj_set_style_border_color(fan_card, lv_color_hex(COL_TANK_BORDER), 0);
    lv_obj_set_style_radius(fan_card, 14, 0);      // Same rounded corners as tank
    lv_obj_set_style_shadow_width(fan_card, 0, 0); // No shadow
    lv_obj_clear_flag(fan_card, LV_OBJ_FLAG_SCROLLABLE);

    // Create the step label
    labl_step = lv_label_create(fan_card);
    lv_obj_set_style_text_color(labl_step, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_step, &lv_font_montserrat_22, 0);

    // Fan icon — keep large (78px) for good visibility
    lv_obj_t *fan = fan_icon_create(fan_card, 78, COL_TEXT_DIM);
    lv_obj_center(fan);

    // Center the number inside
    lv_obj_center(labl_step);
    set_label_u8(labl_step, vent_step);

    // Make the whole card clickable
    lv_obj_add_flag(fan_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fan_card, on_step_tapped, LV_EVENT_CLICKED, tile);

    // ---------------------- TANK --------------------------
    lv_obj_t *tank_wrap = lv_obj_create(right);
    lv_obj_set_size(tank_wrap, RIGHT_W - PAD * 2, MAIN_H - PAD * 2);
    lv_obj_align(tank_wrap, LV_ALIGN_TOP_MID, 0, PAD_TIGHT);
    lv_obj_set_style_bg_opa(tank_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tank_wrap, 0, 0);
    lv_obj_clear_flag(tank_wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tank = lv_obj_create(tank_wrap);
    lv_obj_set_size(tank, 86, 136);
    lv_obj_align(tank, LV_ALIGN_CENTER, 30, 2);
    lv_obj_clear_flag(tank, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(tank, 14, 0);
    lv_obj_set_style_border_width(tank, 2, 0);
    lv_obj_set_style_border_color(tank, lv_color_hex(COL_TANK_BORDER), 0);
    lv_obj_set_style_bg_color(tank, lv_color_hex(COL_TANK_SHELL), 0);
    lv_obj_set_style_bg_opa(tank, LV_OPA_COVER, 0);

    lv_obj_t *water = lv_obj_create(tank);
    lv_obj_set_size(water, 76, 126);
    lv_obj_center(water);
    lv_obj_clear_flag(water, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(water, 10, 0);
    lv_obj_set_style_clip_corner(water, true, 0);
    lv_obj_set_style_border_width(water, 0, 0);

    labl_tank_top = lv_label_create(water);
    lv_obj_set_style_text_color(labl_tank_top, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_tank_top, &lv_font_montserrat_28, 0);
    lv_obj_align(labl_tank_top, LV_ALIGN_TOP_MID, 0, 6);

    labl_tank_bottom = lv_label_create(water);
    lv_obj_set_style_text_color(labl_tank_bottom, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_tank_bottom, &lv_font_montserrat_28, 0);
    lv_obj_align(labl_tank_bottom, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Initial values from Modbus
    int16_t init_top_cC = nilan_get_tank_top_cC();
    int16_t init_bot_cC = nilan_get_tank_bottom_cC();
    int init_top_c = init_top_cC / 100;
    int init_bot_c = init_bot_cC / 100;

    set_label_temp(labl_tank_top, init_top_c);
    set_label_temp(labl_tank_bottom, init_bot_c);
    tank_water_set_gradient(water, init_top_c, init_bot_c);

    lv_obj_t *div = lv_obj_create(water);
    lv_obj_set_size(div, 56, 1);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_20, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    // ------------------- ON/OFF BUTTON--------------------------
    lv_obj_t *pwr_btn = lv_btn_create(tile);
    lv_obj_set_size(pwr_btn, 64, 28);
    lv_obj_align(pwr_btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_radius(pwr_btn, 6, 0);
    lv_obj_set_style_bg_color(pwr_btn, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_bg_opa(pwr_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pwr_btn, 1, 0);
    lv_obj_set_style_border_color(pwr_btn, lv_color_hex(COL_BTN_EDGE), 0);

    labl_power = lv_label_create(pwr_btn);
    lv_obj_set_style_text_font(labl_power, &lv_font_montserrat_16, 0);
    lv_obj_center(labl_power);
    power_btn_update();

    lv_obj_add_event_cb(pwr_btn, on_power_tapped, LV_EVENT_CLICKED, NULL);

    tank_water_gradient = water;
    lv_timer_create(main_status_timer_cb, 1000, NULL); // 1Hz UI update
}

// ===============================================================
// MAIN SCREEN UPDATER (callback)
// ===============================================================

static void main_status_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    // if (!tank_water_gradient || !labl_tank_top || !labl_tank_bottom) return;

    if (tank_water_gradient && labl_tank_top && labl_tank_bottom)
    {
        int16_t top_centiC = nilan_get_tank_top_cC();
        int16_t bottom_centiC = nilan_get_tank_bottom_cC();

        int16_t top_C = (int16_t)(((int32_t)top_centiC * 5243) >> 19);
        int16_t bottom_C = (int16_t)(((int32_t)bottom_centiC * 5243) >> 19);

        static int16_t last_top_C = INT16_MIN;
        static int16_t last_bottom_C = INT16_MIN;

        if (top_C != last_top_C || bottom_C != last_bottom_C)
        {

            last_top_C = top_C;
            last_bottom_C = bottom_C;

            set_label_temp(labl_tank_top, top_C);
            set_label_temp(labl_tank_bottom, bottom_C);
            tank_water_set_gradient(tank_water_gradient, top_C, bottom_C);
        }
    }

    wifi_strength_t strength = wifi_sta_get_signal_strength();
    wifi_icon_set_level(strength);

    // Update clock label from system time (NTP-synced)
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char time_buf[6];  // "HH:MM\0"
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);

    lv_label_set_text(labl_time, time_buf);

}


// ===============================================================
// HELPERS
// ===============================================================

static inline void set_label_u8(lv_obj_t *label, int v)
{
    char b[8];
    lv_snprintf(b, sizeof(b), "%d", v);
    lv_label_set_text(label, b);
}

static inline void set_label_temp(lv_obj_t *label, int t_c)
{
    char b[8];
    lv_snprintf(b, sizeof(b), "%d°", t_c);
    lv_label_set_text(label, b);
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t)
{
    uint8_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint8_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;

    uint8_t r = (uint8_t)(ar + (((int)br - ar) * t) / 255);
    uint8_t g = (uint8_t)(ag + (((int)bg - ag) * t) / 255);
    uint8_t bch = (uint8_t)(ab + (((int)bb - ab) * t) / 255);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bch;
}

static uint32_t temp_to_warm_color(int t_c)
{
    if (t_c < 20) t_c = 20;
    if (t_c > 60) t_c = 60;

    uint32_t cool = 0x234F93;
    uint32_t warm = 0xB86316;
    uint8_t k = (uint8_t)((t_c - 20) * 255 / 40);
    return lerp_rgb(cool, warm, k);
}

static void tank_water_set_gradient(lv_obj_t *water, int top_c, int bot_c)
{
    uint32_t top_col = temp_to_warm_color(top_c);
    uint32_t bot_col = temp_to_warm_color(bot_c);

    lv_obj_set_style_bg_color(water, lv_color_hex(top_col), 0);
    lv_obj_set_style_bg_grad_color(water, lv_color_hex(bot_col), 0);
    lv_obj_set_style_bg_grad_dir(water, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(water, LV_OPA_COVER, 0);
}



// ======================================================
// FAN ICON (FAST: single object draws hub + 4 blades)
// ======================================================

static lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, size_px, size_px);

    // Make it a "drawing surface only"
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);

    // Allocate cfg once (cheap) so draw callback has everything precomputed
    fan_draw_cfg_t *cfg = (fan_draw_cfg_t *)lv_malloc(sizeof(fan_draw_cfg_t));
    if (!cfg) return cont; // fallback: empty

    cfg->color_hex = outline_hex;

    cfg->blade_w = 4;
    cfg->blade_span = 50;
    cfg->blade_off = 25;
    cfg->blade_diam = (int16_t)(size_px - 25); // matches your "blade_r*2 - 15" result

    cfg->hub_w = 3;
    cfg->hub_diam = 35;

    // Draw + free hooks
    lv_obj_add_event_cb(cont, fan_draw_event_cb, LV_EVENT_DRAW_MAIN, cfg);
    lv_obj_add_event_cb(cont, fan_free_event_cb, LV_EVENT_DELETE, cfg);

    return cont;
}

// ---------- Power button ----------
static void power_btn_update()
{
    if (!labl_power) return;
    lv_label_set_text(labl_power, power_on ? "ON" : "OFF");
    lv_obj_set_style_text_color(labl_power,
                                lv_color_hex(power_on ? COL_TEXT : COL_TEXT_DIM),
                                0);
}

static void on_power_tapped(lv_event_t *e)
{
    (void)e;
    power_on = !power_on;
    power_btn_update();
}

// ---------- Popup internals ----------
static void popup_close()
{
    if (step_popup)
    {
        lv_obj_del(step_popup);
        step_popup = NULL;
    }
}

static void popup_step_minus(lv_event_t *e)
{
    if (vent_step > 0) vent_step--;
    set_label_u8(labl_step, vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, vent_step);
}

static void popup_step_plus(lv_event_t *e)
{
    if (vent_step < 4) vent_step++;
    set_label_u8(labl_step, vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, vent_step);
}

static void popup_open(lv_obj_t *parent)
{
    if (step_popup) return;

    step_popup = lv_obj_create(parent);
    lv_obj_set_size(step_popup, 320, 240);
    lv_obj_align(step_popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(step_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(step_popup, LV_OPA_60, 0);
    lv_obj_set_style_border_width(step_popup, 0, 0);
    lv_obj_clear_flag(step_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(step_popup);
    lv_obj_set_size(panel, 220, 140);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Ventilation step");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *num = lv_label_create(panel);
    lv_obj_set_style_text_color(num, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(num, &lv_font_montserrat_48, 0);
    lv_obj_align(num, LV_ALIGN_CENTER, 0, -6);
    set_label_u8(num, vent_step);

    lv_obj_t *bminus = lv_btn_create(panel);
    lv_obj_set_size(bminus, 56, 44);
    lv_obj_align(bminus, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    lv_obj_t *lminus = lv_label_create(bminus);
    lv_label_set_text(lminus, "-");
    lv_obj_set_style_text_font(lminus, &lv_font_montserrat_32, 0);
    lv_obj_center(lminus);
    lv_obj_add_event_cb(bminus, popup_step_minus, LV_EVENT_CLICKED, num);

    lv_obj_t *bplus = lv_btn_create(panel);
    lv_obj_set_size(bplus, 56, 44);
    lv_obj_align(bplus, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_t *lplus = lv_label_create(bplus);
    lv_label_set_text(lplus, "+");
    lv_obj_set_style_text_font(lplus, &lv_font_montserrat_32, 0);
    lv_obj_center(lplus);
    lv_obj_add_event_cb(bplus, popup_step_plus, LV_EVENT_CLICKED, num);

    lv_obj_add_event_cb(step_popup, (lv_event_cb_t)popup_close, LV_EVENT_CLICKED, NULL);
}

static inline void on_step_tapped(lv_event_t *e)
{
    lv_obj_t *tile = (lv_obj_t *)lv_event_get_user_data(e);
    popup_open(tile);
}

static void wifi_icon_create(lv_obj_t *parent,
                             int right_margin,
                             uint32_t inactive_hex)
{
    const int bar_w = 5;
    const int bar_spacing = 3;
    const int bar_h[4] = {6, 10, 14, 18};

    const int count = 4;
    const int total_w = count * bar_w + (count - 1) * bar_spacing; // 29
    const int max_h = bar_h[3];                                    // 18

    // Container for the icon (pad=0 => stable coordinates)
    lv_obj_t *c = lv_obj_create(parent);
    wifi_icon.cont = c;
    wifi_icon.last_level = 255;

    lv_obj_set_size(c, total_w, max_h);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);

    // Place it in the top bar: right side, vertically centered
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -right_margin, 0);

    // Bars
    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *b = lv_obj_create(c);
        wifi_icon.bar[i] = b;

        lv_obj_set_size(b, bar_w, bar_h[i]);
        lv_obj_set_pos(b, i * (bar_w + bar_spacing), max_h - bar_h[i]); // bottom aligned
        lv_obj_set_style_radius(b, 2, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(inactive_hex), 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void wifi_icon_set_level(wifi_strength_t strength)
{
    // Return if no change
    if (wifi_icon.last_level == strength) return;

    // Determine which bars should be shown as active
    for (int i = 0; i < 4; i++)
    {
        lv_color_t bar_color = (i < strength) ? wifi_bar_on_color : wifi_bar_off_color;
        lv_obj_set_style_bg_color(wifi_icon.bar[i], bar_color, 0);
    }

    wifi_icon.last_level = strength;
}

static void fan_draw_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *obj = lv_event_get_target(e);
    fan_draw_cfg_t *cfg = (fan_draw_cfg_t *)lv_event_get_user_data(e);
    if (!cfg) return;

    lv_layer_t *layer = lv_event_get_layer(e);
    if (!layer) return;

    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    const int32_t w = lv_area_get_width(&a);
    const int32_t h = lv_area_get_height(&a);

    // True center of the fan icon object (hub & number center)
    const int32_t cx = a.x1 + (w / 2);
    const int32_t cy = a.y1 + (h / 2);

    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = lv_color_hex(cfg->color_hex);
    d.opa = LV_OPA_COVER;
    d.rounded = 0;

    // ---- Hub ring ----
    d.width = cfg->hub_w;
    d.center.x = (lv_coord_t)cx;
    d.center.y = (lv_coord_t)cy;
    d.radius = (uint16_t)(cfg->hub_diam / 2);
    d.start_angle = 0;
    d.end_angle = 360;
    lv_draw_arc(layer, &d);

    // ---- 4 blades ----
    static const int16_t dx[4] = {0, 1, 0, -1};
    static const int16_t dy[4] = {-1, 0, 1, 0};
    static const uint16_t a0[4] = {0, 90, 180, 270};

    const int32_t blade_r = cfg->blade_diam / 2;

    d.width = cfg->blade_w;
    d.radius = (uint16_t)blade_r;

    for (int i = 0; i < 4; i++)
    {
        d.center.x = (lv_coord_t)(cx + (int32_t)dx[i] * cfg->blade_off);
        d.center.y = (lv_coord_t)(cy + (int32_t)dy[i] * cfg->blade_off);
        d.start_angle = a0[i];
        d.end_angle = (uint16_t)(a0[i] + cfg->blade_span);
        lv_draw_arc(layer, &d);
    }
}

static void fan_free_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
    fan_draw_cfg_t *cfg = (fan_draw_cfg_t *)lv_event_get_user_data(e);
    if (cfg) lv_free(cfg);
}