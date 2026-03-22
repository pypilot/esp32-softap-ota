/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "soc/gpio_periph.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/reset_reasons.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"

#include "bootloader_common.h"
#include "bootloader_config.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define BOOTLOADER_SELECT_GPIO 32
#else
#define BOOTLOADER_SELECT_GPIO 14
#endif

int __real_bootloader_utility_get_selected_boot_partition(const bootloader_state_t *bs);

static bool bootloader_select_is_button_pushed(void)
{
    esp_rom_gpio_pad_pullup_only(BOOTLOADER_SELECT_GPIO);
    esp_rom_gpio_pad_select_gpio(BOOTLOADER_SELECT_GPIO);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[BOOTLOADER_SELECT_GPIO]);
    return (gpio_ll_get_level(&GPIO, BOOTLOADER_SELECT_GPIO) == 0);
}

#ifdef CONFIG_SELECT_ON_ACCELEROMETER
#include "i2c_bitbang.h"
#include "hal/wdt_hal.h"
#include "hal/wdt_types.h"
#include "soc/rtc.h"

static void feed_rtc_wdt(void)
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_init(&rtc_wdt_ctx, WDT_RWDT, 0, false);

    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}

extern wdt_hal_context_t rtc_wdt_ctx;   // only if your build already defines it
static bool probe_sensor(uint8_t addr) {
    uint8_t whoami;
    if (!i2c_reg_read8(addr, 0x0F, &whoami))
        return false;
    return whoami == 0x44;
}

static bool inverted_sensor(uint8_t addr)
{
    // configure to output
    if (!i2c_reg_write8(addr, 0x20, 0x46))
        return 0;

    int c = 0;
    for(int i=0; i<300; i++) { // must be inverted 300 seconds at powerup
        esp_rom_delay_us(10000);
        feed_rtc_wdt();

        if (!probe_sensor(addr)) // added for more robustness
            return false;

        uint8_t z0, z1;
        if (!i2c_reg_read16(addr, 0x28, &z0, &z1))
            return false;

        int8_t zt = z1;
        if(zt > -56 && ++c > 5)
            return false;
    }
    return true;
}

static bool bootloader_select_is_accelerometer_inverted(void)
{
    i2c_init();
    uint8_t addrs[] = {0x18, 0x19};
    for(uint8_t i = 0; i<sizeof addrs; i++)
        if(probe_sensor(addrs[i]))
            return inverted_sensor(addrs[i]);
    return false;
}
#endif

int __wrap_bootloader_utility_get_selected_boot_partition(const bootloader_state_t *bs)
{
    if(esp_rom_get_reset_reason(0) != RESET_REASON_CORE_DEEP_SLEEP)
    {
#ifdef CONFIG_SELECT_ON_IO_PIN
        if(bootloader_select_is_button_pushed())
        {
            return FACTORY_INDEX;
        }
#endif
#ifdef CONFIG_SELECT_ON_ACCELEROMETER
        // delay 10ms help accelerometer stabilize
        esp_rom_delay_us(10000);
        feed_rtc_wdt();
        
        if(bootloader_select_is_accelerometer_inverted())
        {
            return FACTORY_INDEX;
        }
#endif
    }
    return __real_bootloader_utility_get_selected_boot_partition(bs);
}

void __attribute__((section("/DISCARD/"))) bootloader_select_include() {}
