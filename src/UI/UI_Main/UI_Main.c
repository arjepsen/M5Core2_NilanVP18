#include "UI_Main.h"
#include "../../nilan_modbus.h"
#include "../../wifi_sta.h" // for wifi strength
#include "../UI_Shared_Colors.h"
#include "UI_Main_Tank.h"
#include "UI_Main_TopBar.h"
#include "lvgl.h"
#include <limits.h>
#include <math.h>
#include <time.h>

#include "esp_log.h"

// ---------- Layout constants for 320x240 ----------
#define TOP_BAR_HEIGHT 32
#define MAIN_HEIGHT (240 - TOP_BAR_HEIGHT)

#define LEFT_WIDTH 160 //128
#define HALF_SCREEN_WIDTH 160 // 320 / 2
#define RIGHT_WIDTH (320 - LEFT_WIDTH)

// #define PAD 6
// #define PAD_TIGHT 4

// ---------- State ----------
static int vent_step = 3;
static bool power_on = true;

// LVGL label objects we update
static lv_obj_t *labl_step = NULL;
static lv_obj_t *labl_power = NULL;

// Popup handle (lazy-created)
static lv_obj_t *step_popup = NULL;


typedef struct
{
    uint32_t color_hex;
    int16_t blade_off;  // px (e.g. 25)
    int16_t blade_diam; // px (e.g. size_px - 25)
    int16_t hub_diam;   // e.g. 35
    uint8_t blade_w;    // arc width for blades
    uint8_t blade_span; // degrees (e.g. 50)
    uint8_t hub_w;      // e.g. 3
} fan_draw_cfg_t;

// ======================================================
// PROTOTYPES
// ======================================================

static inline void set_label_u8(lv_obj_t *label, int v);
static inline void on_step_tapped(lv_event_t *e);
static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t /*0..255*/);
static uint32_t temp_to_warm_color(int t_c);
static void main_status_timer_cb(lv_timer_t *t);
static lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex);
static void power_btn_update();
static void on_power_tapped(lv_event_t *e);
static void popup_close();
static void popup_step_minus(lv_event_t *e);
static void popup_step_plus(lv_event_t *e);
static void popup_open(lv_obj_t *parent);
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
    ui_top_bar_create(tile);

    // -------------- MAIN AREA CONTAINERS ------------------
    lv_obj_t *left_container = lv_obj_create(tile);
    lv_obj_set_size(left_container, HALF_SCREEN_WIDTH, MAIN_HEIGHT);
    lv_obj_align(left_container, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_container, 0, 0);
    lv_obj_clear_flag(left_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *right_container = lv_obj_create(tile);
    lv_obj_set_size(right_container, HALF_SCREEN_WIDTH, MAIN_HEIGHT);
    lv_obj_align(right_container, LV_ALIGN_TOP_LEFT, HALF_SCREEN_WIDTH, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(right_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_container, 0, 0);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);

    // -------------------- FAN ICON ------------------------
    lv_obj_t *fan_card = lv_obj_create(left_container);
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
    ui_tank_create(right_container);

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


    // ----------- Create the main timer for updating the UI --------------------
    lv_timer_create(main_status_timer_cb, 1000, NULL); // 1Hz UI update
}

// ===============================================================
// MAIN SCREEN UPDATER (callback)
// ===============================================================

static void main_status_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    ui_tank_update();
    ui_top_bar_update();

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
