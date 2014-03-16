/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
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
