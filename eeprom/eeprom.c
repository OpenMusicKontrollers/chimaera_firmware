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
#define EEPROM_24xx_BASE_ADDR 0b1010000
#define EEPROM_24xx_ADDR_SIZE 0x2

EEPROM_24xx _24LC64 = {
	.page_size = 0x20,			// = 32byte
	.storage_size = 0x2000	// = 64kbit
};

EEPROM_24xx _24AA025E48 = {
	.page_size = 0x10,			// = 16byte 
	.storage_size = 0x100		// = 2kbit
};

EEPROM_24xx *eeprom_24LC64 = &_24LC64;
EEPROM_24xx *eeprom_24AA025E48 = &_24AA025E48;

static i2c_msg write_msg;
static uint8_t write_msg_data [48]; // = address_size + page_size

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
eeprom_init (EEPROM_24xx *eeprom, i2c_dev *dev, uint8_t slave_addr)
{
	i2c_master_enable (dev, 0);

	write_msg.flags = 0; // write
	write_msg.length = 0;
	write_msg.xferred = 0;
	write_msg.data = write_msg_data;

	read_msg.flags = I2C_MSG_READ;
	read_msg.length = 0;
	read_msg.xferred = 0;
	read_msg.data = NULL;

	eeprom->dev = dev;
	eeprom->slave_addr = EEPROM_24xx_BASE_ADDR | slave_addr;
}

void
eeprom_byte_write (EEPROM_24xx *eeprom, uint16_t addr, uint8_t byte)
{
	if (addr + 1 > eeprom->storage_size)
		return;

	write_msg.addr = eeprom->slave_addr;
	_eeprom_set_address (addr);
	write_msg_data[2] = byte;
	write_msg.length = EEPROM_24xx_ADDR_SIZE + 1;

	int32_t res;
	res = i2c_master_xfer (eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);

	_eeprom_ack_poll (eeprom->dev); // wait until written
}

void
eeprom_page_write (EEPROM_24xx *eeprom, uint16_t addr, uint8_t *page, uint8_t len)
{
	if ( (addr % eeprom->page_size) || (len > eeprom->page_size) || (addr + len > eeprom->storage_size))
		return;

	write_msg.addr = eeprom->slave_addr;
	_eeprom_set_address (addr);
	memcpy (&write_msg_data[2], page, len);
	write_msg.length = EEPROM_24xx_ADDR_SIZE + len;

	int32_t res;
	res = i2c_master_xfer (eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);

	_eeprom_ack_poll (eeprom->dev); // wait until written
}

void
eeprom_bulk_write (EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if ( (addr % eeprom->page_size) || (addr + len > eeprom->storage_size))
		return;

	uint16_t addr_ptr = addr;
	uint8_t *bulk_ptr = bulk;
	uint16_t remaining = len;
	
	while (remaining > 0)
	{
		uint16_t size = remaining < eeprom->page_size ? remaining : eeprom->page_size;
		eeprom_page_write (eeprom, addr_ptr, bulk_ptr, size);

		addr_ptr += size;
		bulk_ptr += size;
		remaining -= size;
	}
}

void
eeprom_byte_read (EEPROM_24xx *eeprom, uint16_t addr, uint8_t *byte)
{
	if (addr + 1 > eeprom->storage_size)
		return;

	write_msg.addr = eeprom->slave_addr;
	_eeprom_set_address (addr);
	write_msg.length = EEPROM_24xx_ADDR_SIZE;

	read_msg.addr = eeprom->slave_addr;
	read_msg.length = 1;
	read_msg.data = byte;

	int32_t res;
	res = i2c_master_xfer (eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);
	res = i2c_master_xfer (eeprom->dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);
}

void
eeprom_bulk_read (EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if ( addr + len > eeprom->storage_size)
		return;

	write_msg.addr = eeprom->slave_addr;
	_eeprom_set_address (addr);
	write_msg.length = EEPROM_24xx_ADDR_SIZE;

	read_msg.addr = eeprom->slave_addr;
	read_msg.length = len;
	read_msg.data = bulk;

	int32_t res;
	res = i2c_master_xfer (eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);
	res = i2c_master_xfer (eeprom->dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res (eeprom->dev, res);
}
