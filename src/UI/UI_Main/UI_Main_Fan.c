#include "UI_Main_Fan.h"
#include "../UI_Shared_Colors.h"


#define FAN_CARD_SIZE 80


// Prototypes
lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex);
void fan_draw_event_cb(lv_event_t *e);
void fan_draw_event_cb(lv_event_t *e);
void fan_free_event_cb(lv_event_t *e);
void popup_open(lv_obj_t *parent);
static inline void on_step_tapped(lv_event_t *e);
static inline void set_label_u8(lv_obj_t *label, int v);
void popup_close();
void popup_step_minus(lv_event_t *e);
void popup_step_plus(lv_event_t *e);

// Objects, Variables & typedefs
lv_obj_t *labl_fan_step = NULL;
lv_obj_t *step_popup = NULL;
lv_obj_t *parent_tile = NULL;

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

int vent_step = 3;


void ui_fan_create(lv_obj_t *parent, lv_obj_t *parent_tile_obj)
{
    // Save parent tile 
    parent_tile = parent_tile_obj;

    lv_obj_t *fan_card = lv_obj_create(parent);
    lv_obj_set_size(fan_card, FAN_CARD_SIZE, FAN_CARD_SIZE);               // <<< Smaller card (was 96) — tighter outline
    lv_obj_align(fan_card, LV_ALIGN_TOP_MID, 0, 16); // Shift down a bit to keep centered vertically
    lv_obj_set_style_bg_opa(fan_card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fan_card, 2, 0);
    lv_obj_set_style_border_color(fan_card, lv_color_hex(COL_TANK_BORDER), 0);
    lv_obj_set_style_radius(fan_card, 14, 0);      // Same rounded corners as tank
    lv_obj_set_style_shadow_width(fan_card, 0, 0); // No shadow
    lv_obj_clear_flag(fan_card, LV_OBJ_FLAG_SCROLLABLE);

    // Create the step label
    labl_fan_step = lv_label_create(fan_card);
    lv_obj_set_style_text_color(labl_fan_step, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_fan_step, &lv_font_montserrat_22, 0);

    // Fan icon — keep large (78px) for good visibility
    lv_obj_t *fan = fan_icon_create(fan_card, 78, COL_TEXT_DIM);
    lv_obj_center(fan);

    // Center the number inside
    lv_obj_center(labl_fan_step);
    set_label_u8(labl_fan_step, vent_step);

    // Make the whole card clickable
    lv_obj_add_flag(fan_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fan_card, on_step_tapped, LV_EVENT_CLICKED, parent_tile);
}

lv_obj_t *fan_icon_create(lv_obj_t *parent, int size_px, uint32_t outline_hex)
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

void fan_draw_event_cb(lv_event_t *e)
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


void fan_free_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
    fan_draw_cfg_t *cfg = (fan_draw_cfg_t *)lv_event_get_user_data(e);
    if (cfg) lv_free(cfg);
}

void popup_open(lv_obj_t *parent)
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


// ---------- Popup internals ----------
void popup_close()
{
    if (step_popup)
    {
        lv_obj_del(step_popup);
        step_popup = NULL;
    }
}

void popup_step_minus(lv_event_t *e)
{
    if (vent_step > 0) vent_step--;
    set_label_u8(labl_fan_step, vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, vent_step);
}

void popup_step_plus(lv_event_t *e)
{
    if (vent_step < 4) vent_step++;
    set_label_u8(labl_fan_step, vent_step);

    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    if (lbl) set_label_u8(lbl, vent_step);
}

static inline void set_label_u8(lv_obj_t *label, int v)
{
    char b[8];
    lv_snprintf(b, sizeof(b), "%d", v);
    lv_label_set_text(label, b);
}