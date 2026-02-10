#include "UI_Main.h"
#include "../../nilan_modbus.h"
#include "../../wifi_sta.h" // for wifi strength
#include "../UI_Shared_Colors.h"
#include "UI_Main_Tank.h"
#include "UI_Main_TopBar.h"
#include "UI_Main_Fan.h"
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

// ---------- State ----------
// static int vent_step = 3;
static bool power_on = true;

// LVGL label objects we update
static lv_obj_t *labl_step = NULL;
static lv_obj_t *labl_power = NULL;

// ======================================================
// PROTOTYPES
// ======================================================

static inline void set_label_u8(lv_obj_t *label, int v);
static inline void on_step_tapped(lv_event_t *e);
static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t /*0..255*/);
static uint32_t temp_to_warm_color(int t_c);
static void main_status_timer_cb(lv_timer_t *t);
static void power_btn_update();
static void on_power_tapped(lv_event_t *e);


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
    ui_fan_create(left_container, tile);

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

