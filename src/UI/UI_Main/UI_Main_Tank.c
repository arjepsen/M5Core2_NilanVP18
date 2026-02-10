#include "UI_Main_Tank.h"
#include "lvgl.h"
#include "../UI_Shared_Colors.h"
#include "../../Nilan_Modbus.h"

#define TOP_BAR_HEIGHT 32
#define MAIN_HEIGHT (240 - TOP_BAR_HEIGHT)

#define LEFT_W 128
#define RIGHT_W (320 - LEFT_W)
#define SCREEN_HALF_WIDTH 160

#define PAD 6
#define PAD_TIGHT 4

#define TANK_WRAP_WIDTH (SCREEN_HALF_WIDTH - PAD)
#define TANK_WRAP_HEIGHT (MAIN_HEIGHT - PAD)
#define TANK_OUTLINE_WIDTH (TANK_WRAP_WIDTH - 40)
#define TANK_OUTLINE_HEIGHT (TANK_WRAP_HEIGHT - 20)
#define TANK_GRADIENT_WIDTH (TANK_OUTLINE_WIDTH - PAD_TIGHT) 
#define TANK_GRADIENT_HEIGHT (TANK_OUTLINE_HEIGHT - PAD_TIGHT)


#define TEMPERATURE_BUFFER_SIZE 5   // max 5 chars for the temperature label strings, incl. null terminator.

// Local prototypes
uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t);
uint32_t temp_to_warm_color(int t_c);

// Objects
lv_obj_t *labl_tank_top = NULL;
lv_obj_t *labl_tank_bottom = NULL;
lv_obj_t *tank_water_gradient = NULL; // water gradient rect

// Local-global variables
int16_t last_top_C = INT16_MIN;
int16_t last_bottom_C = INT16_MIN;


void ui_tank_create(lv_obj_t *parent)
{
    // Create a "wrapper" for the tank icon
    lv_obj_t *tank_wrap = lv_obj_create(parent);
    lv_obj_set_size(tank_wrap, TANK_WRAP_WIDTH, TANK_WRAP_HEIGHT);
    lv_obj_center(tank_wrap);
    lv_obj_set_style_bg_opa(tank_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tank_wrap, 0, 0);
    lv_obj_clear_flag(tank_wrap, LV_OBJ_FLAG_SCROLLABLE);

    // Create the outline of the tank - rounded rectangle.
    lv_obj_t *tank_outline = lv_obj_create(tank_wrap);
    lv_obj_set_size(tank_outline, TANK_OUTLINE_WIDTH, TANK_OUTLINE_HEIGHT);
    lv_obj_center(tank_outline);
    lv_obj_clear_flag(tank_outline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(tank_outline, 14, 0);
    lv_obj_set_style_border_width(tank_outline, 2, 0);
    lv_obj_set_style_border_color(tank_outline, lv_color_hex(COL_TANK_BORDER), 0);
    lv_obj_set_style_bg_color(tank_outline, lv_color_hex(COL_TANK_SHELL), 0);
    lv_obj_set_style_bg_opa(tank_outline, LV_OPA_COVER, 0);

    // Create the temperature-gradient inside the outline
    tank_water_gradient = lv_obj_create(tank_outline);
    lv_obj_set_size(tank_water_gradient, TANK_GRADIENT_WIDTH, TANK_GRADIENT_HEIGHT);
    lv_obj_center(tank_water_gradient);
    lv_obj_clear_flag(tank_water_gradient, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(tank_water_gradient, 10, 0);
    lv_obj_set_style_clip_corner(tank_water_gradient, true, 0);
    lv_obj_set_style_border_width(tank_water_gradient, 0, 0);

    labl_tank_top = lv_label_create(tank_water_gradient);
    lv_obj_set_style_text_color(labl_tank_top, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_tank_top, &lv_font_montserrat_28, 0);
    lv_obj_align(labl_tank_top, LV_ALIGN_TOP_MID, 0, 6);

    labl_tank_bottom = lv_label_create(tank_water_gradient);
    lv_obj_set_style_text_color(labl_tank_bottom, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(labl_tank_bottom, &lv_font_montserrat_28, 0);
    lv_obj_align(labl_tank_bottom, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_obj_t *div = lv_obj_create(tank_water_gradient);
    lv_obj_set_size(div, 56, 1);
    lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(div, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_20, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    ui_tank_update();
}

void ui_tank_update()
{
    if (tank_water_gradient && labl_tank_top && labl_tank_bottom)
    {
        // Pull temperatures over modbus.
        int16_t top_centiC = nilan_get_tank_top_cC();
        int16_t bottom_centiC = nilan_get_tank_bottom_cC();

        // Convert centi-degress to degrees.
        int16_t top_C = (int16_t)(((int32_t)top_centiC * 5243) >> 19);
        int16_t bottom_C = (int16_t)(((int32_t)bottom_centiC * 5243) >> 19);

        // Only update if necessary.
        if (top_C != last_top_C || bottom_C != last_bottom_C)
        {
            // Save values for next comparison.
            last_top_C = top_C;
            last_bottom_C = bottom_C;

            // Update labels
            char buffer[TEMPERATURE_BUFFER_SIZE];
            lv_snprintf(buffer, TEMPERATURE_BUFFER_SIZE, "%d°", top_C);
            lv_label_set_text(labl_tank_top, buffer);
            lv_snprintf(buffer, TEMPERATURE_BUFFER_SIZE, "%d°", bottom_C);
            lv_label_set_text(labl_tank_bottom, buffer);

            // Get colors for the gradient
            uint32_t top_col = temp_to_warm_color(top_C);
            uint32_t bot_col = temp_to_warm_color(bottom_C);

            // Update the object
            lv_obj_set_style_bg_color(tank_water_gradient, lv_color_hex(top_col), 0);
            lv_obj_set_style_bg_grad_color(tank_water_gradient, lv_color_hex(bot_col), 0);
            lv_obj_set_style_bg_grad_dir(tank_water_gradient, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_bg_opa(tank_water_gradient, LV_OPA_COVER, 0);
        }
    }
}

uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t)
{
    uint8_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint8_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;

    uint8_t r = (uint8_t)(ar + (((int)br - ar) * t) / 255);
    uint8_t g = (uint8_t)(ag + (((int)bg - ag) * t) / 255);
    uint8_t bch = (uint8_t)(ab + (((int)bb - ab) * t) / 255);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bch;
}

uint32_t temp_to_warm_color(int t_c)
{
    if (t_c < 20) t_c = 20;
    // if (t_c > 60) t_c = 60;

    uint32_t cool = 0x234F93;
    uint32_t warm = 0xB86316;
    uint8_t k = (uint8_t)((t_c - 20) * 255 / 40);
    return lerp_rgb(cool, warm, k);
}

