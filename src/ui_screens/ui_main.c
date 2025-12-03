#include "ui_main.h"
#include "lvgl.h"
#include "nilan_modbus.h"

// ---------- Layout constants for 320x240 ----------
#define TOP_BAR_H     20
#define MAIN_H        (240 - TOP_BAR_H)

#define LEFT_W        128
#define RIGHT_W       (320 - LEFT_W)

#define PAD           6
#define PAD_TIGHT     4

// ---------- Colors ----------
#define COL_BG        0x202020
#define COL_TOPBAR    0x3A291C
#define COL_TEXT      0xFFFFFF
#define COL_TEXT_DIM  0xB0B0B0
#define COL_BTN_BG    0x262B30
#define COL_BTN_EDGE  0x3A3A3A

// Tank shell
#define COL_TANK_SHELL   0x2F2F2F
#define COL_TANK_BORDER  0x6A6A6A

// ---------- State ----------
static int s_vent_step = 3;
static bool s_power_on = true;

// LVGL objects we update
static lv_obj_t *s_lbl_step     = NULL;
static lv_obj_t *s_lbl_tank_top = NULL;
static lv_obj_t *s_lbl_tank_bot = NULL;
static lv_obj_t *s_lbl_power    = NULL;

// Popup handle (lazy-created)
static lv_obj_t *s_step_popup = NULL;

static lv_obj_t *s_tank_water = NULL;   // water gradient rect


// ---------- Helpers ----------
static void set_label_u8(lv_obj_t *lbl, int v)
{
    char b[8];
    lv_snprintf(b, sizeof(b), "%d", v);
    lv_label_set_text(lbl, b);
}

static void set_label_temp(lv_obj_t *lbl, int t_c)
{
    char b[8];
    lv_snprintf(b, sizeof(b), "%d°", t_c);
    lv_label_set_text(lbl, b);
}

// --- small RGB hex lerp (8-bit per channel) ---
static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t /*0..255*/)
{
    uint8_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint8_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;

    uint8_t r = (uint8_t)(ar + (((int)br - ar) * t) / 255);
    uint8_t g = (uint8_t)(ag + (((int)bg - ag) * t) / 255);
    uint8_t bch = (uint8_t)(ab + (((int)bb - ab) * t) / 255);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bch;
}

// Map temp to color endpoint (darkened to avoid RGB565 blow-out)
static uint32_t temp_to_warm_color(int t_c)
{
    if (t_c < 20) t_c = 20;
    if (t_c > 60) t_c = 60;

    uint32_t cool = 0x234F93;  // darker blue
    uint32_t warm = 0xB86316;  // darker orange
    uint8_t k = (uint8_t)((t_c - 20) * 255 / 40);
    return lerp_rgb(cool, warm, k);
}

// Set real LVGL gradient on the water object
static void tank_water_set_gradient(lv_obj_t *water, int top_c, int bot_c)
{
    uint32_t top_col = temp_to_warm_color(top_c);
    uint32_t bot_col = temp_to_warm_color(bot_c);

    lv_obj_set_style_bg_color(water, lv_color_hex(top_col), 0);
    lv_obj_set_style_bg_grad_color(water, lv_color_hex(bot_col), 0);
    lv_obj_set_style_bg_grad_dir(water, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(water, LV_OPA_COVER, 0);
}

// Periodic main-screen status update (tank temps)
static void main_status_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (!s_tank_water || !s_lbl_tank_top || !s_lbl_tank_bot) {
        return;
    }

    // Values from Modbus are in centi-degrees C (x100)
    int16_t top_cC = nilan_get_tank_top_cC();
    int16_t bot_cC = nilan_get_tank_bottom_cC();

    int top_c = top_cC / 100;
    int bot_c = bot_cC / 100;

    set_label_temp(s_lbl_tank_top, top_c);
    set_label_temp(s_lbl_tank_bot, bot_c);
    tank_water_set_gradient(s_tank_water, top_c, bot_c);
}


/* ---------------- Fan icon (3-blade ventilator style) ----------------
 * Outer ring + 3 thick blades + center hub.
 * NO label inside.
 */
static lv_obj_t* fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, size_px, size_px);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);

    // Outer ring
    lv_obj_t *ring = lv_arc_create(cont);
    lv_obj_set_size(ring, size_px, size_px);
    lv_obj_center(ring);
    lv_arc_set_bg_angles(ring, 0, 360);

    lv_obj_set_style_arc_width(ring, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(outline_hex), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring, LV_OPA_0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(ring, 0, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    // Blades
    const int blade_r = size_px - 10;
    const int arc_w   = 4;      // thicker = more fan-like

    for (int i = 0; i < 3; i++) {
        lv_obj_t *arc = lv_arc_create(cont);
        lv_obj_set_size(arc, blade_r, blade_r);
        lv_obj_center(arc);

        // big sweep so it reads like a curved blade
        lv_arc_set_bg_angles(arc, 235, 35);
        lv_arc_set_rotation(arc, i * 120);
        lv_arc_set_value(arc, 0);

        lv_obj_set_style_arc_width(arc, arc_w, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(outline_hex), LV_PART_MAIN);

        lv_obj_set_style_arc_opa(arc, LV_OPA_0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_width(arc, 0, 0);

        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    }

    // Center hub (small circle)
    lv_obj_t *hub = lv_obj_create(cont);
    int hub_sz = size_px / 5;
    lv_obj_set_size(hub, hub_sz, hub_sz);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(outline_hex), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hub, 0, 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);

    return cont;
}

// ---------- Power button ----------
static void power_btn_update(void)
{
    if (!s_lbl_power) return;
    lv_label_set_text(s_lbl_power, s_power_on ? "ON" : "OFF");
    lv_obj_set_style_text_color(s_lbl_power,
                                lv_color_hex(s_power_on ? COL_TEXT : COL_TEXT_DIM),
                                0);
}

static void on_power_tapped(lv_event_t *e)
{
    (void)e;
    s_power_on = !s_power_on;
    power_btn_update();
}

// ---------- Popup internals ----------
static void popup_close(void)
{
    if (s_step_popup) {
        lv_obj_del(s_step_popup);
        s_step_popup = NULL;
    }
}

static void popup_step_minus(lv_event_t *e)
{
    if (s_vent_step > 0) s_vent_step--;
    set_label_u8(s_lbl_step, s_vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, s_vent_step);
}

static void popup_step_plus(lv_event_t *e)
{
    if (s_vent_step < 4) s_vent_step++;
    set_label_u8(s_lbl_step, s_vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, s_vent_step);
}

static void popup_open(lv_obj_t *parent)
{
    if (s_step_popup) return;

    s_step_popup = lv_obj_create(parent);
    lv_obj_set_size(s_step_popup, 320, 240);
    lv_obj_align(s_step_popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_step_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_step_popup, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_step_popup, 0, 0);
    lv_obj_clear_flag(s_step_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(s_step_popup);
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
    lv_obj_set_style_text_font(num, &lv_font_montserrat_48, 0); // remove if missing
    lv_obj_align(num, LV_ALIGN_CENTER, 0, -6);
    set_label_u8(num, s_vent_step);

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

    lv_obj_add_event_cb(s_step_popup, (lv_event_cb_t)popup_close, LV_EVENT_CLICKED, NULL);
}

static void on_step_tapped(lv_event_t *e)
{
    lv_obj_t *tile = (lv_obj_t *)lv_event_get_user_data(e);
    popup_open(tile);
}

// ---------- Main screen ----------
void ui_main_create(lv_obj_t *tile)
{
    lv_obj_set_style_bg_color(tile, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // ---------- Top bar ----------
    lv_obj_t *top = lv_obj_create(tile);
    lv_obj_set_size(top, 320, TOP_BAR_H);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(COL_TOPBAR), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_time = lv_label_create(top);
    lv_label_set_text(lbl_time, "22:45  WiFi");
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *lbl_mode = lv_label_create(top);
    lv_label_set_text(lbl_mode, "Mode: Auto");
    lv_obj_set_style_text_color(lbl_mode, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(lbl_mode, LV_ALIGN_RIGHT_MID, -6, 0);

    // ---------- Main area containers ----------
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

    // ---------- Left: Fan + step number (no card, no text) ----------
    lv_obj_t *step_btn = lv_btn_create(left);
    lv_obj_set_size(step_btn, 96, 96);
    lv_obj_align(step_btn, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(step_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(step_btn, 0, 0);
    lv_obj_set_style_radius(step_btn, 0, 0);

    lv_obj_t *fan = fan_icon_create(step_btn, 78, COL_TEXT_DIM);
    lv_obj_center(fan);

    // Step number BELOW the fan
    s_lbl_step = lv_label_create(left);
    lv_obj_set_style_text_color(s_lbl_step, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_step, &lv_font_montserrat_32, 0);
    lv_obj_align_to(s_lbl_step, step_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    set_label_u8(s_lbl_step, s_vent_step);

    // Clicking fan area opens popup
    lv_obj_add_event_cb(step_btn, on_step_tapped, LV_EVENT_CLICKED, tile);

    // ---------- Right: Tank ----------
    lv_obj_t *tank_wrap = lv_obj_create(right);
    lv_obj_set_size(tank_wrap, RIGHT_W - PAD*2, MAIN_H - PAD*2);
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

    s_lbl_tank_top = lv_label_create(water);
    lv_obj_set_style_text_color(s_lbl_tank_top, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_tank_top, &lv_font_montserrat_28, 0);
    lv_obj_align(s_lbl_tank_top, LV_ALIGN_TOP_MID, 0, 6);
    //set_label_temp(s_lbl_tank_top, 48);

    s_lbl_tank_bot = lv_label_create(water);
    lv_obj_set_style_text_color(s_lbl_tank_bot, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_tank_bot, &lv_font_montserrat_28, 0);
    lv_obj_align(s_lbl_tank_bot, LV_ALIGN_BOTTOM_MID, 0, -6);
    //set_label_temp(s_lbl_tank_bot, 35);

    // Initial values from Modbus (will be 0 until first poll)
    int16_t init_top_cC = nilan_get_tank_top_cC();
    int16_t init_bot_cC = nilan_get_tank_bottom_cC();
    int init_top_c = init_top_cC / 100;
    int init_bot_c = init_bot_cC / 100;

    //tank_water_set_gradient(water, 48, 35);
    set_label_temp(s_lbl_tank_top, init_top_c);
    set_label_temp(s_lbl_tank_bot, init_bot_c);
    tank_water_set_gradient(water, init_top_c, init_bot_c);

    lv_obj_t *div = lv_obj_create(water);
    lv_obj_set_size(div, 56, 1);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_20, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    // ---------- Power ON/OFF button (rect, simple text) ----------
    lv_obj_t *pwr_btn = lv_btn_create(tile);
    lv_obj_set_size(pwr_btn, 64, 28);
    lv_obj_align(pwr_btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_radius(pwr_btn, 6, 0);
    lv_obj_set_style_bg_color(pwr_btn, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_bg_opa(pwr_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pwr_btn, 1, 0);
    lv_obj_set_style_border_color(pwr_btn, lv_color_hex(COL_BTN_EDGE), 0);

    s_lbl_power = lv_label_create(pwr_btn);
    lv_obj_set_style_text_font(s_lbl_power, &lv_font_montserrat_16, 0);
    lv_obj_center(s_lbl_power);
    power_btn_update();

    lv_obj_add_event_cb(pwr_btn, on_power_tapped, LV_EVENT_CLICKED, NULL);

    s_tank_water = water;
    lv_timer_create(main_status_timer_cb, 1000, NULL);  // 1Hz UI update
}
