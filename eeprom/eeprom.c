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
	(void)eeprom;
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
eeprom_byte_write(EEPROM_24xx *eeprom, uint16_t addr, uint8_t byt)
{
	if( (addr + 1U) > eeprom->storage_size)
		return;

	_set_address(eeprom, addr);
	write_msg_data[eeprom->address_size] = byt;
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
eeprom_byte_read(EEPROM_24xx *eeprom, uint16_t addr, uint8_t *byt)
{
	if( (addr + 1U) > eeprom->storage_size)
		return;

	_set_address(eeprom, addr);
	write_msg.length = eeprom->address_size;

	read_msg.length = 1;
	read_msg.data = byt;

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
