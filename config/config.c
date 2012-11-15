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

#include "config_private.h"

#include <string.h>
#include <stdio.h>

#include <chimaera.h>
#include <chimutil.h>
#include <udp.h>
#include <eeprom.h>
#include <cmc.h>

#define GLOB_BROADCAST {255, 255, 255, 255}
#define LAN_BROADCAST {192, 168, 1, 255}
#define LAN_HOST {192, 168, 1, 10}

uint16_t f_thresh1;
fix_0_16_t m_thresh1;
ADC_Range adc_range [MUX_MAX][ADC_LENGTH];

Config config = {
	.magic = MAGIC, // used to compare EEPROM and FLASH config versions

	.version = {
		.major = 0,
		.minor = 1,
		.patch_level = 0
	},

	.comm = {
		.mac = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
		.ip = {192, 168, 1, 177},
		.gateway = {192, 168, 1, 1},
		.subnet = {255, 255, 255, 0}
	},
	
	.tuio = {
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 0,
			.port = {3333, 3333},
			.ip = LAN_BROADCAST
		},
		.long_header = 0,
		.offset = nOSC_IMMEDIATE
	},

	.config = {
		.rate = 10, // rate in Hz
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 1,
			.port = {4444, 4444},
			.ip = LAN_BROADCAST
		}
	},

	.sntp = {
		.tau = 4, // delay between SNTP requests in seconds
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 2,
			.port = {123, 123},
			.ip = LAN_HOST
		}
	},

	.dump = {
		.enabled = 0, // disabled by default
		.socket = {
			.sock = 3,
			.port = {5555, 5555},
			.ip = LAN_BROADCAST
		}
	},

	.debug = {
		.enabled = 1,
		.socket = {
			.sock = 4,
			.port = {6666, 6666},
			.ip = LAN_BROADCAST
		}
	},

	.zeroconf = {
		.enabled = 1,
		.har = {0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb}, // hardware address for mDNS multicast
		.socket = {
			.sock = 5,
			.port = {5353, 5353}, // mDNS multicast port
			.ip = {224, 0, 0, 251} // mDNS multicast group
		}
	},

	.cmc = {
		.thresh0 = 0.04,
		.thresh1 = 0.08,
		.peak_thresh = 5,
	},

	.rate = 2000, // update rate in Hz
	.pacemaker = 0x0b // pacemaker rate 2^11=2048
};

static void
_thresh1_update ()
{
	f_thresh1 = 0x7ff * config.cmc.thresh1;
	m_thresh1 = 1.0uhr / (1.0uhr - config.cmc.thresh1);
}

uint8_t
config_load ()
{
	uint8_t magic;
	eeprom_byte_read (I2C1, 0x0000, &magic);

	if (magic == config.magic) // EEPROM and FLASH config versions match
		eeprom_bulk_read (I2C1, 0x0000, (uint8_t *)&config, sizeof (config));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
		config_save ();

	_thresh1_update ();

	return 1;
}

uint8_t
config_save ()
{
	eeprom_bulk_write (I2C1, 0x0000, (uint8_t *)&config, sizeof (config));
	return 1;
}

inline uint16_t
range_mean (uint8_t mux, uint8_t adc)
{
	return adc_range[mux][adc].mean;
}

uint8_t
range_load ()
{
	uint8_t magic;
	eeprom_byte_read (I2C1, 0x1000, &magic);

	if (magic == config.magic) // EEPROM and FLASH config versions match
		eeprom_bulk_read (I2C1, 0x1020, (uint8_t *)&adc_range, sizeof (adc_range));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
	{
		uint8_t p, i;
		for (p=0; p<MUX_MAX; p++)
			for (i=0; i<ADC_LENGTH; i++)
			{
				adc_range[p][i].south = 0xfff; // this is the ideal south maximum
				adc_range[p][i].mean = 0x7ff; // this is the ideal mean
				adc_range[p][i].north = 0x000; // this is the ideal north maximum
			}
		range_update ();
		range_save ();
	}

	return 1;
}

uint8_t
range_save ()
{
	uint8_t magic = config.magic;
	eeprom_byte_write (I2C1, 0x1000, magic);
	eeprom_bulk_write (I2C1, 0x1020, (uint8_t *)&adc_range, sizeof (adc_range));

	// debug
	// FIXME make a function out of this
	uint8_t p, i;
	char buf[64];
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			sprintf (buf, "%i %i %i %i %i", p, i, adc_range[p][i].south, adc_range[p][i].mean, adc_range[p][i].north);
			debug_str (buf);
		}

	return 1;
}

void
range_calibrate (int16_t raw[16][10])
{
	uint8_t p, i;
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].mean += raw[p][i];
			adc_range[p][i].mean /= 2;

			if (raw[p][i] > adc_range[p][i].south)
				adc_range[p][i].south = raw[p][i];

			if (raw[p][i] < adc_range[p][i].north)
				adc_range[p][i].north = raw[p][i];
		}
}

void
range_update ()
{
	uint8_t p, i;

	// calculate maximal SOUTH and NORTH pole values
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].south = adc_range[p][i].south - adc_range[p][i].mean; // maximal SOUTH pole range
			adc_range[p][i].north = adc_range[p][i].mean - adc_range[p][i].north; // maximal NORTH pole range

			adc_range[p][i].m_south = 2047.0 / (float)adc_range[p][i].south;
			adc_range[p][i].m_north = 2047.0 / (float)adc_range[p][i].north;

			adc_range[p][i].thresh0[0] = config.cmc.thresh0 * adc_range[p][i].south;
			adc_range[p][i].thresh0[1] = config.cmc.thresh0 * adc_range[p][i].north;
		}
}

uint8_t
groups_load ()
{
	uint8_t size;
	uint8_t *buf;

	//TODO use config.magic
	eeprom_byte_read (I2C1, 0x800, &size);
	if (size)
	{
		buf = cmc_group_buf_set (size);
		eeprom_bulk_read (I2C1, 0x820, buf, size);
	}
	else
		groups_save ();

	return 1;
}

uint8_t
groups_save ()
{
	uint8_t size;
	uint8_t *buf;

	buf = cmc_group_buf_get (&size);
	eeprom_byte_write (I2C1, 0x0800, size);
	eeprom_bulk_write (I2C1, 0x0820, buf, size);

	return 1;
}

_check_range8 (uint8_t *val, uint8_t min, uint8_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, *val);
	}
	else
	{
		uint8_t arg = args[1]->i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

_check_range16 (uint16_t *val, uint16_t min, uint16_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, *val);
	}
	else
	{
		uint16_t arg = args[1]->i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

_check_rangefloat (float *val, float min, float max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTf", id, *val);
	}
	else
	{
		float arg = args[1]->f;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_version (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiii", id, config.version.major, config.version.minor, config.version.patch_level);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (config_load ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "loading of configuration from EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (config_save ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "saving configuration to EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_mac (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiiiiii", id,
			config.comm.mac[0], config.comm.mac[1], config.comm.mac[2], config.comm.mac[3], config.comm.mac[4], config.comm.mac[5]);
	}
	else
	{
		if ( (args[1]->i < 0x100) && (args[2]->i < 0x100) && (args[3]->i < 0x100) && (args[4]->i < 0x100) && (args[5]->i < 0x100) && (args[6]->i < 0x100) )
		{
			config.comm.mac[0] = args[1]->i;
			config.comm.mac[1] = args[2]->i;
			config.comm.mac[2] = args[3]->i;
			config.comm.mac[3] = args[4]->i;
			config.comm.mac[4] = args[5]->i;
			config.comm.mac[5] = args[6]->i;

			udp_mac_set (config.comm.mac);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in MAC must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_ip (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiiii", id,
			config.comm.ip[0], config.comm.ip[1], config.comm.ip[2], config.comm.ip[3]);
	}
	else
	{
		if ( (args[1]->i < 0x100) && (args[2]->i < 0x100) && (args[3]->i < 0x100) && (args[4]->i < 0x100) )
		{
			config.comm.ip[0] = args[1]->i;
			config.comm.ip[1] = args[2]->i;
			config.comm.ip[2] = args[3]->i;
			config.comm.ip[3] = args[4]->i;

			udp_ip_set (config.comm.ip);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_gateway (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiiii", id,
			config.comm.gateway[0], config.comm.gateway[1], config.comm.gateway[2], config.comm.gateway[3]);
	}
	else
	{
		if ( (args[1]->i < 0x100) && (args[2]->i < 0x100) && (args[3]->i < 0x100) && (args[4]->i < 0x100) )
		{
			config.comm.gateway[0] = args[1]->i;
			config.comm.gateway[1] = args[2]->i;
			config.comm.gateway[2] = args[3]->i;
			config.comm.gateway[3] = args[4]->i;

			udp_gateway_set (config.comm.gateway);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_subnet (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiiii", id,
			config.comm.subnet[0], config.comm.subnet[1], config.comm.subnet[2], config.comm.subnet[3]);
	}
	else
	{
		if ( (args[1]->i < 0x100) && (args[2]->i < 0x100) && (args[3]->i < 0x100) && (args[4]->i < 0x100) )
		{
			config.comm.subnet[0] = args[1]->i;
			config.comm.subnet[1] = args[2]->i;
			config.comm.subnet[2] = args[3]->i;
			config.comm.subnet[3] = args[4]->i;

			udp_subnet_set (config.comm.subnet);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_get (uint8_t b, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (b)
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTF", id);

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_set (void (*cb) (uint8_t b), const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (fmt[1] == nOSC_TRUE)
	{
		cb (1);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}
	else if (fmt[1] == nOSC_FALSE)
	{
		cb (0);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
} 

static uint8_t
_tuio_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	if (argc == 1) // query
		return _enabled_get (config.tuio.enabled, path, fmt, argc, args);
	else
		return _enabled_set (tuio_enable, path, fmt, argc, args);
}

static uint8_t
_config_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	if (argc == 1) // query
		return _enabled_get (config.config.enabled, path, fmt, argc, args);
	else
		return _enabled_set (config_enable, path, fmt, argc, args);
}

static uint8_t
_sntp_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	if (argc == 1) // query
		return _enabled_get (config.sntp.enabled, path, fmt, argc, args);
	else
		return _enabled_set (sntp_enable, path, fmt, argc, args);
}

static uint8_t
_dump_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	if (argc == 1) // query
		return _enabled_get (config.dump.enabled, path, fmt, argc, args);
	else
		return _enabled_set (dump_enable, path, fmt, argc, args);
}

static uint8_t
_debug_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	if (argc == 1) // query
		return _enabled_get (config.debug.enabled, path, fmt, argc, args);
	else
		return _enabled_set (debug_enable, path, fmt, argc, args);
}

static uint8_t
_socket (Socket_Config *socket, void (*cb) (uint8_t b), uint8_t flag, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiiiiii", id,
			socket->port[0], socket->port[1], socket->ip[0], socket->ip[1], socket->ip[2], socket->ip[3]);
	}
	else
	{
		if ( (args[1]->i < 0x10000) && (args[2]->i < 0x10000) && (args[3]->i < 0x100) && (args[4]->i < 0x100) && (args[5]->i < 0x100) && (args[6]->i < 0x100) )
		{
			socket->port[0] = args[1]->i;
			socket->port[1] = args[2]->i;
			socket->ip[0] = args[3]->i;
			socket->ip[1] = args[4]->i;
			socket->ip[2] = args[5]->i;
			socket->ip[3] = args[6]->i;

			cb (flag);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: port number must be < 0x10000 and numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_socket (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket (&config.tuio.socket, tuio_enable, config.tuio.enabled, path, fmt, argc, args);
}

static uint8_t
_config_socket (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket (&config.config.socket, config_enable, config.config.enabled, path, fmt, argc, args);
}

static uint8_t
_sntp_socket (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket (&config.sntp.socket, sntp_enable, config.sntp.enabled, path, fmt, argc, args);
}

static uint8_t
_dump_socket (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket (&config.dump.socket, dump_enable, config.dump.enabled, path, fmt, argc, args);
}

static uint8_t
_debug_socket (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket (&config.debug.socket, debug_enable, config.debug.enabled, path, fmt, argc, args);
}

static uint8_t
_config_rate (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _check_range16 (&config.config.rate, 1, 10, path, fmt, argc, args);
}

static uint8_t
_sntp_tau (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _check_range8 (&config.sntp.tau, 1, 10, path, fmt, argc, args);
}

static uint8_t
_tuio_long_header (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		if (config.tuio.long_header)
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTT", id);
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTF", id);
	}
	else
	{
		switch (fmt[1])
		{
			case nOSC_TRUE:
				tuio2_frm_long_set ("chimaera", config.comm.ip, config.tuio.socket.port[1], 144, 1);
				break;
			case nOSC_FALSE:
				tuio2_frm_long_unset ();
				break;
		}
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_offset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTt", config.tuio.offset);
	}
	else
	{
		config.tuio.offset = args[1]->t;
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_rate (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (argc == 1) // query
	{
		if (config.rate)
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, config.rate);
		else // infinity
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTI", id);
	}
	else
	{
		switch (fmt[1])
		{
			case nOSC_INT32:
				config.rate = args[1]->i;
				break;
			case nOSC_INFTY:
				config.rate = 0;
				break;
		}

		if (config.rate)
		{
			timer_pause (adc_timer);
			adc_timer_reconfigure ();
			timer_resume (adc_timer);
		}
		else
			timer_pause (adc_timer);

		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_reset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;
	int32_t sec;

	if (argc > 1)
	{
		sec = args[1]->i;
		if (sec < 1)
			sec = 1;
	}
	else
		sec = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	delay_us (sec * 1e6); // delay sec seconds until reset
	nvic_sys_reset ();

	return 1;
}

static uint8_t
_thresh0 (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _check_rangefloat (&config.cmc.thresh0, 0.0, 1.0, path, fmt, argc, args);
	//FIXME run range_update or new calibration
}

static uint8_t
_thresh1 (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t res;
	res = _check_rangefloat (&config.cmc.thresh1, 0.0, 1.0, path, fmt, argc, args);
	_thresh1_update ();
	return res;
}

static uint8_t
_peak_thresh (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _check_range8 (&config.cmc.peak_thresh, 3, 9, path, fmt, argc, args);
}

static uint8_t
_group_clear (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	cmc_group_clear ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_add (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (cmc_group_add (args[1]->i, args[2]->i, args[3]->f, args[4]->f))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "group already existing or maximal number of groups overstept");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (cmc_group_set (args[1]->i, args[2]->i, args[3]->f, args[4]->f))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "group not found");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_del (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (cmc_group_del (args[1]->i))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (groups_load ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if (groups_save ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_start (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	// initialize sensor range
	uint8_t p, i;
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].south = 0x7ff;
			adc_range[p][i].mean = 0x7ff;
			adc_range[p][i].north = 0x7ff;
		}

	// enable calibration
	calibrating = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_stop (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	// disable calibration
	calibrating = 0;

	// update new range
	range_update ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	// store new calibration range to EEPROM
	range_save ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	// load calibration range from EEPROM
	range_load ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

nOSC_Method config_methods [] = {
	{"/chimaera/version", "i", _version},

	{"/chimaera/config/load", "i", _config_load},
	{"/chimaera/config/save", "i", _config_save},

	{"/chimaera/comm/mac", "i", _comm_mac},
	{"/chimaera/comm/mac", "iiiiiii", _comm_mac},
	{"/chimaera/comm/ip", "i", _comm_ip},
	{"/chimaera/comm/ip", "iiiii", _comm_ip},
	{"/chimaera/comm/gateway", "i", _comm_gateway},
	{"/chimaera/comm/gateway", "iiiii", _comm_gateway},
	{"/chimaera/comm/subnet", "i", _comm_subnet},
	{"/chimaera/comm/subnet", "iiiii", _comm_subnet},

	{"/chimaera/tuio/enabled", "i", _tuio_enabled},
	{"/chimaera/tuio/enabled", "iT", _tuio_enabled},
	{"/chimaera/tuio/enabled", "iF", _tuio_enabled},
	{"/chimaera/tuio/socket", "i", _tuio_socket},
	{"/chimaera/tuio/socket", "iiiiiii", _tuio_socket},
	{"/chimaera/tuio/long_header", "i", _tuio_long_header},
	{"/chimaera/tuio/long_header", "iT", _tuio_long_header},
	{"/chimaera/tuio/long_header", "iF", _tuio_long_header},
	{"/chimaera/tuio/offset", "i", _tuio_offset},
	{"/chimaera/tuio/offset", "it", _tuio_offset},

	{"/chimaera/config/enabled", "i", _config_enabled},
	{"/chimaera/config/enabled", "iT", _config_enabled},
	{"/chimaera/config/enabled", "iF", _config_enabled},
	{"/chimaera/config/socket", "i", _config_socket},
	{"/chimaera/config/socket", "iiiiiii", _config_socket},
	{"/chimaera/config/rate", "i", _config_rate},
	{"/chimaera/config/rate", "ii", _config_rate},

	{"/chimaera/sntp/enabled", "i", _sntp_enabled},
	{"/chimaera/sntp/enabled", "iT", _sntp_enabled},
	{"/chimaera/sntp/enabled", "iF", _sntp_enabled},
	{"/chimaera/sntp/socket", "i", _sntp_socket},
	{"/chimaera/sntp/socket", "iiiiiii", _sntp_socket},
	{"/chimaera/sntp/tau", "i", _sntp_tau},
	{"/chimaera/sntp/tau", "ii", _sntp_tau},

	{"/chimaera/dump/enabled", "i", _dump_enabled},
	{"/chimaera/dump/enabled", "iT", _dump_enabled},
	{"/chimaera/dump/enabled", "iF", _dump_enabled},
	{"/chimaera/dump/socket", "i", _dump_socket},
	{"/chimaera/dump/socket", "iiiiiii", _dump_socket},

	{"/chimaera/debug/enabled", "i", _debug_enabled},
	{"/chimaera/debug/enabled", "iT", _debug_enabled},
	{"/chimaera/debug/enabled", "iF", _debug_enabled},
	{"/chimaera/debug/socket", "i", _debug_socket},
	{"/chimaera/debug/socket", "iiiiiii", _debug_socket},

	//TODO
	//{"/chimaera/zeroconf/enabled", "i", _zeroconf_enabled},
	//{"/chimaera/zeroconf/enabled", "iT", _zeroconf_enabled},
	//{"/chimaera/zeroconf/enabled", "iF", _zeroconf_enabled},

	{"/chimaera/thresh0", "i", _thresh0},
	{"/chimaera/thresh0", "if", _thresh0},
	{"/chimaera/thresh1", "i", _thresh1},
	{"/chimaera/thresh1", "if", _thresh1},

	{"/chimaera/peak_thresh", "i", _peak_thresh},
	{"/chimaera/peak_thresh", "ii", _peak_thresh},

	{"/chimaera/group/clear", "i", _group_clear},
	{"/chimaera/group/add", "iiiff", _group_add},
	{"/chimaera/group/set", "iiiff", _group_set},
	{"/chimaera/group/del", "ii", _group_del},
	{"/chimaera/group/load", "i", _group_load},
	{"/chimaera/group/save", "i", _group_save},

	{"/chimaera/rate", "i", _rate},
	{"/chimaera/rate", "ii", _rate},
	{"/chimaera/rate", "iI", _rate},

	{"/chimaera/reset", "i", _reset},
	{"/chimaera/reset", "ii", _reset},

	{"/chimaera/calibration/start", "i", _calibration_start},
	{"/chimaera/calibration/stop", "i", _calibration_stop},
	{"/chimaera/calibration/save", "i", _calibration_save},
	{"/chimaera/calibration/load", "i", _calibration_load},

	{NULL, NULL, NULL} // terminator
};
