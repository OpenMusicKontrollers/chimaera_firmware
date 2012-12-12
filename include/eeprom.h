/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

/*
 * This library is for the Microchip 24LC64
 * 64K I2C Serial EEPROM
 *
 */

#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <stdint.h>

#include <libmaple/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _24LC64_SLAVE_ADDR 0b1010000
#define _24LC64_ADDR_SIZE 2
#define _24LC64_PAGE_SIZE 0x20
#define _24LC64_STORAGE 0x2000

// TODO return success or failure flags
void eeprom_init (i2c_dev *dev, uint8_t slave_addr);

void eeprom_byte_write (i2c_dev *dev, uint16_t addr, uint8_t byte);
void eeprom_byte_read (i2c_dev *dev, uint16_t addr, uint8_t *byte);

void eeprom_page_write (i2c_dev *dev, uint16_t addr, uint8_t *page, uint8_t len);

void eeprom_bulk_write (i2c_dev *dev, uint16_t addr, uint8_t *bulk, uint16_t len);
void eeprom_bulk_read (i2c_dev *dev, uint16_t addr, uint8_t *bulk, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
