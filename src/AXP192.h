#ifndef AXP192_H
#define AXP192_h

#include <stdint.h>
#include <stdbool.h>


extern bool axp192_initialized;

// Sets up handle, and activates 5V on Port A
void axp192_init();
void set_backlight_level(uint8_t backlight_level);




#endif /* AXP192_h */