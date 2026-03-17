/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_rom_sys.h"

#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"

#include <stdint.h>
#include "esp_rom_gpio.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"

#define I2C_SDA_GPIO 21
#define I2C_SCL_GPIO 22

static void gpio_od_init(int gpio_num)
{
    esp_rom_gpio_pad_select_gpio(gpio_num);

    // Enable input so we can sample the line when released
    GPIO.enable_w1tc = (1U << gpio_num);   // start released (output disabled)
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpio_num]);

    // Optional internal pull-up. External pull-ups are still preferred for I2C.
    esp_rom_gpio_pad_pullup_only(gpio_num);

    // Ensure output value register is 0 so when output is enabled it drives low
    GPIO.out_w1tc = (1U << gpio_num);
}

static void sda_low(void)
{
    // output enabled, output register already 0 => drive low
    GPIO.enable_w1ts = (1U << I2C_SDA_GPIO);
}

static void sda_release(void)
{
    // disable output => high-Z, external/internal pull-up pulls high
    GPIO.enable_w1tc = (1U << I2C_SDA_GPIO);
}

static void scl_low(void)
{
    GPIO.enable_w1ts = (1U << I2C_SCL_GPIO);
}

static void scl_release(void)
{
    GPIO.enable_w1tc = (1U << I2C_SCL_GPIO);
}

static int read_sda(void)
{
    return (GPIO.in >> I2C_SDA_GPIO) & 1U;
}

/* ~100 kHz target: 5 us high + 5 us low */
static inline void i2c_delay(void)
{
    esp_rom_delay_us(5);
}

static void i2c_start(void)
{
    sda_release();
    scl_release();
    i2c_delay();

    sda_low();
    i2c_delay();

    scl_low();
    i2c_delay();
}

static void i2c_stop(void)
{
    sda_low();
    i2c_delay();

    scl_release();
    i2c_delay();

    sda_release();
    i2c_delay();
}

static bool i2c_write_bit(int bit)
{
    if (bit) {
        sda_release();
    } else {
        sda_low();
    }

    i2c_delay();
    scl_release();
    i2c_delay();
    scl_low();
    i2c_delay();

    return true;
}

static int i2c_read_bit(void)
{
    int bit;

    sda_release();     // release SDA so slave can drive it
    i2c_delay();

    scl_release();
    i2c_delay();

    bit = read_sda();

    scl_low();
    i2c_delay();

    return bit;
}

static bool i2c_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        if (!i2c_write_bit((byte & 0x80) != 0))
            return false;
        byte <<= 1;
    }

    /* ACK bit: slave pulls SDA low */
    return i2c_read_bit() == 0;
}

static uint8_t i2c_read_byte(bool ack)
{
    uint8_t byte = 0;

    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        if (i2c_read_bit())
            byte |= 1;
    }

    /* ACK = 0, NACK = 1 */
    i2c_write_bit(ack ? 0 : 1);

    return byte;
}

bool i2c_reg_write8(uint8_t dev7, uint8_t reg, uint8_t value)
{
    i2c_start();

    if (!i2c_write_byte((dev7 << 1) | 0)) goto fail;
    if (!i2c_write_byte(reg))             goto fail;
    if (!i2c_write_byte(value))           goto fail;

    i2c_stop();
    return true;

fail:
    i2c_stop();
    return false;
}

bool i2c_reg_read8(uint8_t dev7, uint8_t reg, uint8_t *value)
{
    i2c_start();

    if (!i2c_write_byte((dev7 << 1) | 0)) goto fail;
    if (!i2c_write_byte(reg))             goto fail;

    i2c_start();  // repeated start

    if (!i2c_write_byte((dev7 << 1) | 1)) goto fail;

    *value = i2c_read_byte(false);   // NACK after last byte
    i2c_stop();
    return true;

fail:
    i2c_stop();
    return false;
}

bool i2c_reg_read16(uint8_t dev7, uint8_t reg, uint8_t *lo, uint8_t *hi)
{
    i2c_start();

    if (!i2c_write_byte((dev7 << 1) | 0)) goto fail;
    if (!i2c_write_byte(reg))             goto fail;

    i2c_start();  // repeated start

    if (!i2c_write_byte((dev7 << 1) | 1)) goto fail;

    *lo = i2c_read_byte(true);    // ACK first byte
    *hi = i2c_read_byte(false);   // NACK last byte

    i2c_stop();
    return true;

fail:
    i2c_stop();
    return false;
}

void i2c_init() {
    gpio_od_init(I2C_SDA_GPIO);
    gpio_od_init(I2C_SCL_GPIO);
    sda_release();
    scl_release();
}
