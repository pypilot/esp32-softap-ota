#include "soc/gpio_periph.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/reset_reasons.h"
#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "bootloader_common.h"
#include "bootloader_config.h"

#define BOOTLOADER_SELECT_GPIO 6

int __real_bootloader_utility_get_selected_boot_partition(const bootloader_state_t *bs);

static bool bootloader_select_is_button_pushed(void)
{
    esp_rom_gpio_pad_pullup_only(BOOTLOADER_SELECT_GPIO);
    esp_rom_gpio_pad_select_gpio(BOOTLOADER_SELECT_GPIO);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[BOOTLOADER_SELECT_GPIO]);
    return (gpio_ll_get_level(&GPIO, BOOTLOADER_SELECT_GPIO) == 0);
}

int __wrap_bootloader_utility_get_selected_boot_partition(const bootloader_state_t *bs)
{
    if(esp_rom_get_reset_reason(0) != RESET_REASON_CORE_DEEP_SLEEP)
    {
        if(bootloader_select_is_button_pushed())
        {
            return FACTORY_INDEX;
        }
    }
    return __real_bootloader_utility_get_selected_boot_partition(bs);
}

void __attribute__((section("/DISCARD/"))) bootloader_select_include() {}
