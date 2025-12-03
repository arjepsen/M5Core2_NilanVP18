#include "AXP192.h"
#include "bsp/esp-bsp.h"

#define AXP192_I2C_ADDRESS 0x34
#define AXP192_DCDC3 0x27

static i2c_master_dev_handle_t axp192_handle = NULL;
bool axp192_initialized = false;

void axp192_init()
{
    if (axp192_initialized)
        return;

    // Ensure i2c is up, get a handle.
    bsp_i2c_init();
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();

    // Set up a handle for the axp192
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP192_I2C_ADDRESS,
        .scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ,
    };

    i2c_master_bus_add_device(i2c_bus, &dev_cfg, &axp192_handle);


    // Enable the 5V boost circuit for 5V on Port-A while we're at it.
    uint8_t tx_buf[2];

    // First, set the axp192 gpio0 up to 3.3V - it controls "bus_pw_en"
    tx_buf[0] = 0x91; // work on register 91H
    i2c_master_transmit_receive(axp192_handle, &tx_buf[0], 1, &tx_buf[1], 1, -1);  // read register 0x91
    tx_buf[1] = (tx_buf[1] & 0x0F) | 0xF0;    // Preserve bit 0-3 (reserved), write 1111 to bit 4-7 (gpio0 = 3.3V)
    i2c_master_transmit(axp192_handle, tx_buf, 2, -1);  // Write the new value.

    // Second, set up gpio0 for LDO output, using register 0x90
    tx_buf[0] = 0x90;   // set register
    i2c_master_transmit_receive(axp192_handle, &tx_buf[0], 1, &tx_buf[1], 1, -1);    // retrieve current values.
    tx_buf[1] = (tx_buf[1] & 0xF8) | 0x02;  // bit 3-7 reserved, bit 0-2 = 010 => low-noise LDO.
    i2c_master_transmit(axp192_handle, tx_buf, 2, -1);

    // Third, enable 5V boost, via the EXTEN bit (bit 2 of register 0x10)
    tx_buf[0] = 0x10;
    i2c_master_transmit_receive(axp192_handle, &tx_buf[0], 1, &tx_buf[1], 1, -1);
    tx_buf[1] |= 0x04; // Ensure bit 2 it set, exten = on.
    i2c_master_transmit(axp192_handle, tx_buf, 2, -1);

    axp192_initialized = true;
}



void set_backlight_level(uint8_t backlight_level)
{
    if (axp192_initialized)  // Ignore call if handle isn't set up.
    {

        // Clamp within range.
        backlight_level = (backlight_level > 0x68) ? 0x68 : backlight_level;

        uint8_t tx_buf[2] = {AXP192_DCDC3, backlight_level};
        i2c_master_transmit(axp192_handle, tx_buf, 2, 1000);
    }
}

