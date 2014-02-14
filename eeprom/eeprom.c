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

#include <string.h>

#include <libmaple/delay.h>

#include <eeprom.h>
#include <chimaera.h>
#include <wiz.h>

#define TIMEOUT 10 // ms
#define EEPROM_24xx_BASE_ADDR 0b1010000

static EEPROM_24xx _24LC64 = {
	.page_size = 0x20,			// = 32byte
	.storage_size = 0x2000,	// = 64kbit
	.address_size = 2,			// 2byte
	.page_write_time = 2		// ms
};

static EEPROM_24xx _24AA025E48 = {
	.page_size = 0x10,			// = 16byte 
	.storage_size = 0x100,	// = 2kbit
	.address_size = 1,			// 1byte
	.page_write_time = 3		// ms
};

EEPROM_24xx *eeprom_24LC64 = &_24LC64;
EEPROM_24xx *eeprom_24AA025E48 = &_24AA025E48;

static i2c_msg write_msg;
static uint8_t write_msg_data [0x22]; // = address_size + page_size
static i2c_msg read_msg;

static inline void
_set_address(EEPROM_24xx *eeprom, uint16_t addr)
{
	write_msg.addr = eeprom->slave_addr;
	read_msg.addr = eeprom->slave_addr;

	switch(eeprom->address_size)
	{
		case 1:
			write_msg_data[0] = addr;
			break;
		case 2:
			write_msg_data[0] = addr >> 8;
			write_msg_data[1] = addr & 0xff;
			break;
	}
}

static inline void
_eeprom_check_res(i2c_dev *dev, int32_t res)
{
	if(res != 0)
	{
		i2c_disable(dev);
		i2c_master_enable(dev, I2C_BUS_RESET); // or 0
	}
}
	
static inline void
_eeprom_ack_poll(EEPROM_24xx *eeprom)
{
	//delay_us(eeprom->page_write_time*1e3);
	delay_us(10e3);
}

void
eeprom_init(i2c_dev *dev)
{
	i2c_master_enable(dev, 0);

	write_msg.flags = 0; // write
	write_msg.length = 0;
	write_msg.xferred = 0;
	write_msg.data = write_msg_data;

	read_msg.flags = I2C_MSG_READ;
	read_msg.length = 0;
	read_msg.xferred = 0;
	read_msg.data = NULL;
}

void
eeprom_slave_init(EEPROM_24xx *eeprom, i2c_dev *dev, uint16_t slave_addr)
{
	eeprom->dev = dev;
	eeprom->slave_addr = EEPROM_24xx_BASE_ADDR | slave_addr;
}

void
eeprom_byte_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t byte)
{
	if(addr + 1 > eeprom->storage_size)
		return;

	_set_address(eeprom, addr);
	write_msg_data[eeprom->address_size] = byte;
	write_msg.length = eeprom->address_size + 1;

	int32_t res;
	res = i2c_master_xfer(eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);

	_eeprom_ack_poll(eeprom); // wait until written
}

void
eeprom_page_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *page, uint8_t len)
{
	if( (addr % eeprom->page_size) || (len > eeprom->page_size) || (addr + len > eeprom->storage_size))
		return;

	_set_address(eeprom, addr);
	memcpy(&write_msg_data[eeprom->address_size], page, len);
	write_msg.length = eeprom->address_size + len;

	int32_t res;
	res = i2c_master_xfer(eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);

	_eeprom_ack_poll(eeprom); // wait until written
}

void
eeprom_bulk_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if( (addr % eeprom->page_size) || (addr + len > eeprom->storage_size))
		return;

	uint16_t addr_ptr = addr;
	uint8_t *bulk_ptr = bulk;
	uint16_t remaining = len;
	
	while(remaining > 0)
	{
		uint16_t size = remaining < eeprom->page_size ? remaining : eeprom->page_size;
		eeprom_page_write(eeprom, addr_ptr, bulk_ptr, size);

		addr_ptr += size;
		bulk_ptr += size;
		remaining -= size;
	}
}

void
eeprom_byte_read(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *byte)
{
	if(addr + 1 > eeprom->storage_size)
		return;

	_set_address(eeprom, addr);
	write_msg.length = eeprom->address_size;

	read_msg.length = 1;
	read_msg.data = byte;

	int32_t res;
	res = i2c_master_xfer(eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);
	res = i2c_master_xfer(eeprom->dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);
}

void
eeprom_bulk_read(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *bulk, uint16_t len)
{
	if( addr + len > eeprom->storage_size)
		return;

	_set_address(eeprom, addr);
	write_msg.length = eeprom->address_size;

	read_msg.length = len;
	read_msg.data = bulk;

	int32_t res;
	res = i2c_master_xfer(eeprom->dev, &write_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);
	res = i2c_master_xfer(eeprom->dev, &read_msg, 1, TIMEOUT);
	_eeprom_check_res(eeprom->dev, res);
}
