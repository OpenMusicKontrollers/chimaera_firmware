/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <stdint.h>

#include <libmaple/i2c.h>

typedef struct _EEPROM_24xx EEPROM_24xx;

struct _EEPROM_24xx {
	i2c_dev *dev;
	uint16_t slave_addr;
	uint8_t page_size;
	uint8_t address_size;
	uint32_t storage_size;
	uint8_t page_write_time; // ms
};

extern EEPROM_24xx *eeprom_24LC64;
extern EEPROM_24xx *eeprom_24AA025E48;

// TODO return success or failure flags
void eeprom_init(i2c_dev *dev);
void eeprom_slave_init(EEPROM_24xx *eeprom, i2c_dev *dev, uint16_t slav_addr);

void eeprom_byte_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t byte);
void eeprom_byte_read(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *byte);

void eeprom_page_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *page, uint8_t len);

void eeprom_bulk_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len);
void eeprom_bulk_read(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len);

#endif // _EEPROM_H_
