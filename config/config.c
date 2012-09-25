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

#include <chimaera.h>
#include <chimutil.h>
#include <udp.h>
#include <eeprom.h>
#include <cmc.h>

#define GLOB_BROADCAST {255, 255, 255, 255}
#define LAN_BROADCAST {192, 168, 1, 255}
#define LAN_HOST {192, 168, 1, 10}

ADC_Range adc_range [MUX_MAX][ADC_LENGTH];

Config config = {
	.magic = 0x03, // used to compare EEPROM and FLASH config versions

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
		.thresh0 = 60,
		.thresh1 = 160,
		.thresh2 = 2048,
		.peak_thresh = 5,
	},

	.rate = 2000, // update rate in Hz
	.pacemaker = 0x0b // pacemaker rate 2^11=2048
};

static uint8_t
_version_get (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiii", id, config.version.major, config.version.minor, config.version.patch_level);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
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

	if (magic == 0xab) // EEPROM and FLASH config versions match
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
	uint8_t magic = 0xab;
	eeprom_byte_write (I2C1, 0x1000, magic);
	eeprom_bulk_write (I2C1, 0x1020, (uint8_t *)&adc_range, sizeof (adc_range));

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
		}
}

uint8_t
groups_load ()
{
	uint8_t size;
	uint8_t *buf;

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
_comm_mac_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	config.comm.mac[0] = args[1]->i;
	config.comm.mac[0] = args[2]->i;
	config.comm.mac[0] = args[3]->i;
	config.comm.mac[0] = args[4]->i;
	config.comm.mac[0] = args[5]->i;
	config.comm.mac[0] = args[6]->i;

	udp_mac_set (config.comm.mac);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_ip_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	config.comm.ip[0] = args[1]->i;
	config.comm.ip[0] = args[2]->i;
	config.comm.ip[0] = args[3]->i;
	config.comm.ip[0] = args[4]->i;

	udp_ip_set (config.comm.ip);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_gateway_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	config.comm.gateway[0] = args[1]->i;
	config.comm.gateway[0] = args[2]->i;
	config.comm.gateway[0] = args[3]->i;
	config.comm.gateway[0] = args[4]->i;

	udp_gateway_set (config.comm.gateway);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_subnet_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	config.comm.subnet[0] = args[1]->i;
	config.comm.subnet[0] = args[2]->i;
	config.comm.subnet[0] = args[3]->i;
	config.comm.subnet[0] = args[4]->i;

	udp_subnet_set (config.comm.subnet);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
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
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong type: boolean expected at position [2]");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
} 

static uint8_t
_tuio_enabled_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (tuio_enable, path, fmt, argc, args);
}

static uint8_t
_config_enabled_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (config_enable, path, fmt, argc, args);
}

static uint8_t
_sntp_enabled_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (sntp_enable, path, fmt, argc, args);
}

static uint8_t
_dump_enabled_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (dump_enable, path, fmt, argc, args);
}

static uint8_t
_debug_enabled_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (debug_enable, path, fmt, argc, args);
}

static uint8_t
_socket_set (Socket_Config *socket, void (*cb) (uint8_t b), uint8_t flag, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	if ( (args[1]->i < 0x100) && (args[2]->i < 0x100) && (args[3]->i < 0x10) && (args[4]->i < 0x10) && (args[5]->i < 0x10) && (args[6]->i < 0x10) )
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
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: port number must be < 0x100 and numbers in IP must be < 0x10");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_socket_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.tuio.socket, tuio_enable, config.tuio.enabled, path, fmt, argc, args);
}

static uint8_t
_config_socket_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.config.socket, config_enable, config.config.enabled, path, fmt, argc, args);
}

static uint8_t
_sntp_socket_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.sntp.socket, sntp_enable, config.sntp.enabled, path, fmt, argc, args);
}

static uint8_t
_dump_socket_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.dump.socket, dump_enable, config.dump.enabled, path, fmt, argc, args);
}

static uint8_t
_debug_socket_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.debug.socket, debug_enable, config.debug.enabled, path, fmt, argc, args);
}

static uint8_t
_rate_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

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
		adc_timer_pause ();
		adc_timer_reconfigure ();
		adc_timer_resume ();
	}
	else
		adc_timer_pause ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_reset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint16_t size;
	int32_t id = args[0]->i;

	int32_t sec = args[1]->i;
	if (sec < 1)
		sec = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	delay_us (sec * 1e6); // delay sec seconds until reset
	nvic_sys_reset ();

	return 1;
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

nOSC_Method config_methods [] = {
	{"/chimaera/version/get", "i", _version_get},

	{"/chimaera/config/load", "i", _config_load},
	{"/chimaera/config/save", "i", _config_save},

	{"/chimaera/comm/mac/set", "iiiiiii", _comm_mac_set},
	{"/chimaera/comm/ip/set", "iiiii", _comm_ip_set},
	{"/chimaera/comm/gateway/set", "iiiii", _comm_gateway_set},
	{"/chimaera/comm/subnet/set", "iiiii", _comm_subnet_set},

	{"/chimaera/tuio/enabled/set", "iT", _tuio_enabled_set},
	{"/chimaera/tuio/enabled/set", "iF", _tuio_enabled_set},
	{"/chimaera/tuio/socket/set", "iiiiiii", _tuio_socket_set},
	//{"/chimaera/tuio/long_header/set", "iT", _tuio_long_header_set},
	//{"/chimaera/tuio/long_header/set", "iF", _tuio_long_header_set},

	{"/chimaera/config/enabled/set", "iT", _config_enabled_set},
	{"/chimaera/config/enabled/set", "iF", _config_enabled_set},
	{"/chimaera/config/socket/set", "iiiiiii", _config_socket_set},
	//{"/chimaera/config/rate/set", "iiiiiii", _config_rate_set},

	{"/chimaera/sntp/enabled/set", "iT", _sntp_enabled_set},
	{"/chimaera/sntp/enabled/set", "iF", _sntp_enabled_set},
	{"/chimaera/sntp/socket/set", "iiiiiii", _sntp_socket_set},
	//{"/chimaera/sntp/tau/set", "iiiiiii", _sntp_tau_set},

	{"/chimaera/dump/enabled/set", "iT", _dump_enabled_set},
	{"/chimaera/dump/enabled/set", "iF", _dump_enabled_set},
	{"/chimaera/dump/socket/set", "iiiiiii", _dump_socket_set},

	{"/chimaera/debug/enabled/set", "iT", _debug_enabled_set},
	{"/chimaera/debug/enabled/set", "iF", _debug_enabled_set},
	{"/chimaera/debug/socket/set", "iiiiiii", _debug_socket_set},

	//{"/chimaera/thresh0", "i*", _thresh0},
	//{"/chimaera/thresh1", "i*", _thresh1},
	//{"/chimaera/thresh2", "i*", _thresh2},

	{"/chimaera/group/clear", "i", _group_clear},
	{"/chimaera/group/add", "iiiff", _group_add},
	{"/chimaera/group/set", "iiiff", _group_set},
	{"/chimaera/group/del", "ii", _group_del},
	{"/chimaera/group/load", "i", _group_load},
	{"/chimaera/group/save", "i", _group_save},

	{"/chimaera/rate/set", "ii", _rate_set}, //TODO use "i*"
	{"/chimaera/rate/set", "iI", _rate_set},

	{"/chimaera/reset", "ii", _reset},

	{"/chimaera/calibration/start", "i", _calibration_start},
	{"/chimaera/calibration/stop", "i", _calibration_stop},
	{"/chimaera/calibration/save", "i", _calibration_save},

	{NULL, NULL, NULL}
};
