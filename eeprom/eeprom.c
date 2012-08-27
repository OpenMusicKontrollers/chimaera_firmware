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

#include <string.h>

#include <libmaple/delay.h>

#include <eeprom.h>

#define TIMEOUT 10 // ms

static i2c_msg write_msg;
static uint8_t write_msg_data [_24LC64_ADDR_SIZE + _24LC64_PAGE_SIZE];

static i2c_msg read_msg;

inline void
_eeprom_set_address (uint16_t addr)
{
	write_msg_data[0] = addr >> 8;
	write_msg_data[1] = addr & 0xff;
}

inline void
_eeprom_check_res (i2c_dev *dev, int32_t res)
{
	if (res != 0)
	{
		i2c_disable (dev);
		i2c_master_enable (dev, I2C_BUS_RESET); // or 0
	}
}
	
inline void
_eeprom_ack_poll (i2c_dev *dev)
{
	/* TODO implement acknowledge polling to see whether chip is ready for further data
	write_msg.length = 0;
	while (i2c_master_xfer (dev, &write_msg, 1, TIMEOUT) != 0)
		;
	*/
	delay_us (10e3); // 2 ms typical write cycle time for page write
}

void
eeprom_init (i2c_dev *dev, uint8_t slave_addr)
{
	i2c_master_enable (dev, 0);

	write_msg.addr = slave_addr;
	write_msg.flags = 0; // write
	write_msg.length = 0;
	write_msg.xferred = 0;
	write_msg.data = write_msg_data;

	read_msg.addr = slave_addr;
	read_msg.flags = I2C_MSG_READ;
	read_msg.length = 0;
	read_msg.xferred = 0;
	read_msg.data = NULL;
}

void
eeprom_byte_write (i2c_dev *dev, uint16_t addr, uint8_t byte)
{
	_eeprom_set_address (addr);
	write_msg_data[2] = byte;
	write_msg.length = _24LC64_ADDR_SIZE + 1;

	int32_t res;
	res = i2c_master_xfer (dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);

	_eeprom_ack_poll (dev); // wait until written
}

void
eeprom_page_write (i2c_dev *dev, uint16_t addr, uint8_t *page, uint8_t len)
{
	if ( (addr % _24LC64_PAGE_SIZE) || (len > _24LC64_PAGE_SIZE))
		return;

	_eeprom_set_address (addr);
	memcpy (&write_msg_data[2], page, len);
	write_msg.length = _24LC64_ADDR_SIZE + len;

	int32_t res;
	res = i2c_master_xfer (dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);

	_eeprom_ack_poll (dev); // wait until written
}

void
eeprom_bulk_write (i2c_dev *dev, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if ( (addr % _24LC64_PAGE_SIZE) || (addr + len > _24LC64_STORAGE))
		return;

	uint16_t addr_ptr = addr;
	uint8_t *bulk_ptr = bulk;
	uint16_t remaining = len;
	
	while (remaining > 0)
	{
		uint16_t size = remaining < _24LC64_PAGE_SIZE ? remaining : _24LC64_PAGE_SIZE;
		eeprom_page_write (dev, addr_ptr, bulk_ptr, size);

		addr_ptr += size;
		bulk_ptr += size;
		remaining -= size;
	}
}

void
eeprom_byte_read (i2c_dev *dev, uint16_t addr, uint8_t *byte)
{
	_eeprom_set_address (addr);
	write_msg.length = _24LC64_ADDR_SIZE;

	read_msg.length = 1;
	read_msg.data = byte;

	int32_t res;
	res = i2c_master_xfer (dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);
	res = i2c_master_xfer (dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);
}

void
eeprom_bulk_read (i2c_dev *dev, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if ( addr + len > _24LC64_STORAGE)
		return;

	_eeprom_set_address (addr);
	write_msg.length = _24LC64_ADDR_SIZE;

	read_msg.length = len;
	read_msg.data = bulk;

	int32_t res;
	res = i2c_master_xfer (dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);
	res = i2c_master_xfer (dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res (dev, res);
}
