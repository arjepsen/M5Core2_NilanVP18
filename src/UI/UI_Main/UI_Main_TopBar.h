#pragma once

#include "lvgl.h"
#include "../../wifi_sta.h"  // for wifi_strength_t

// Public API
void ui_top_bar_create(lv_obj_t *parent);  // parent = the tile or main container
void ui_top_bar_update_wifi(wifi_strength_t strength);

// Exposed objects for timer updates (if needed elsewhere)
extern lv_obj_t *labl_time;                // clock label
