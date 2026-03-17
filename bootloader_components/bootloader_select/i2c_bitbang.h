/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

bool i2c_reg_write8(uint8_t dev7, uint8_t reg, uint8_t value);
bool i2c_reg_read8(uint8_t dev7, uint8_t reg, uint8_t *value);
bool i2c_reg_read16(uint8_t dev7, uint8_t reg, uint8_t *lo, uint8_t *hi);
void i2c_init();
