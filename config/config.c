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
#include "../cmc/cmc_private.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <chimaera.h>
#include <chimutil.h>
#include <wiz.h>
#include <eeprom.h>
#include <cmc.h>

#define GLOB_BROADCAST {255, 255, 255, 255}
#define LAN_BROADCAST {192, 168, 1, 255}
#define LAN_HOST {192, 168, 1, 10}

ADC_Range adc_range [MUX_MAX][ADC_LENGTH];
float Y1 = 0.5;

static uint16_t
By (fix_s7_8_t A, fix_s7_8_t B, fix_s7_8_t C)
{
	float a = A;
	float b = B;
	float c = C;
	float y = 0.0;
	return ADC_HALF_BITDEPTH * (a*a - 2.0*b*c + 2.0*b*y - a*sqrt(a*a - 4.0*b*c + 4.0*b*y)) / (2.0*b*b);
}

static void
curvefit (uint16_t b0, uint16_t b1, uint16_t b2, fix_s7_8_t *A, fix_s7_8_t *B, fix_s7_8_t *C)
{
	float B0 = lookup[b0];
	float B1 = lookup[b1];
	float B2 = lookup[b2];

	float sqrtB0 = lookup_sqrt[b0];
	float sqrtB1 = lookup_sqrt[b1];
	float sqrtB2 = lookup_sqrt[b2];

	*A = (B1 + B0*(-1.0 + Y1) - B2*Y1) / (B0*(sqrtB1 - sqrtB2) + B1*sqrtB2 - sqrtB1*B2 + sqrtB0*(-B1 + B2));
  *B = (1.0/(sqrtB0 - sqrtB2) + Y1/(-sqrtB0 + sqrtB1)) / (sqrtB1 - sqrtB2);
  *C = (B0*(sqrtB1 - sqrtB2*Y1) + sqrtB0*(-B1 + B2*Y1)) / ((sqrtB0 - sqrtB1)*(sqrtB0 - sqrtB2)*(sqrtB1 - sqrtB2));

	debug_str ("curvefit");
	debug_float (Y1);
	debug_int32 (b0);
	debug_int32 (b1);
	debug_int32 (b2);
	debug_float (B0);
	debug_float (B1);
	debug_float (B2);
	debug_float (sqrtB0);
	debug_float (sqrtB1);
	debug_float (sqrtB2);
	debug_float (*A);
	debug_float (*B);
	debug_float (*C);
}

Config config = {
	.magic = MAGIC, // used to compare EEPROM and FLASH config versions

	.name = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a', '\0'},

	.version = {
		.major = 0,
		.minor = 1,
		.patch_level = 0
	},

	.comm = {
		.mac = {(0x1a | 0b00000010) & 0b11111110, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f}, // locally administered unicast MAC
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

	.dhcpc = {
		.enabled = 0,
		.socket = {
			.sock = 6,
			.port = {68, 67}, // BOOTPclient, BOOTPserver
			.ip = {255, 255, 255, 255} // broadcast
		}
	},

	.rtpmidi = {
		.enabled = 0,
		.socket = {
			.sock = 7,
			.port = {7777, 7777},
			.ip = LAN_BROADCAST
		}
	},

	.cmc = {
		.peak_thresh = 5,
	},

	.rate = 2000, // update rate in Hz
	.pacemaker = 0x0b, // pacemaker rate 2^11=2048
	.calibration = 0, // use slot 0 as standard calibration
};

static uint8_t
magic_match ()
{
	uint8_t magic;
	eeprom_byte_read (I2C1, EEPROM_CONFIG_OFFSET, &magic);

	return magic == config.magic; // check whether EEPROM and FLASH config magic number match
}

uint8_t
config_load ()
{
	if (magic_match ())
		eeprom_bulk_read (I2C1, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
		config_save ();

	return 1;
}

uint8_t
config_save ()
{
	eeprom_bulk_write (I2C1, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));
	return 1;
}

inline uint16_t
range_mean (uint8_t mux, uint8_t adc)
{
	return adc_range[mux][adc].mean;
}

uint8_t
range_load (uint8_t pos)
{
	if (magic_match ()) // EEPROM and FLASH config versions match
		eeprom_bulk_read (I2C1, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&adc_range, sizeof (adc_range));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
	{
		uint8_t p, i;
		for (p=0; p<MUX_MAX; p++)
			for (i=0; i<ADC_LENGTH; i++)
			{
				adc_range[p][i].mean = 0x7ff; // this is the ideal mean

				adc_range[p][i].A[POLE_SOUTH].fix = 1.0hk;
				adc_range[p][i].A[POLE_NORTH].fix = 1.0hk;

				adc_range[p][i].B[POLE_SOUTH].fix = 0.0hk;
				adc_range[p][i].B[POLE_NORTH].fix = 0.0hk;

				adc_range[p][i].C[POLE_SOUTH].fix = 0.0hk;
				adc_range[p][i].C[POLE_NORTH].fix = 0.0hk;

				adc_range[p][i].thresh[POLE_SOUTH] = 0;
				adc_range[p][i].thresh[POLE_NORTH] = 0;
			}

		range_save (pos);
	}

	return 1;
}

uint8_t
range_save (uint8_t pos)
{
	eeprom_bulk_write (I2C1, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&adc_range, sizeof (adc_range));

	return 1;
}

uint8_t
range_print ()
{
	uint8_t p, i;
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			debug_int32 (p);
			debug_int32 (i);

			debug_int32 (adc_range[p][i].mean);

			debug_float (adc_range[p][i].A[POLE_SOUTH].fix);
			debug_float (adc_range[p][i].A[POLE_NORTH].fix);

			debug_float (adc_range[p][i].B[POLE_SOUTH].fix);
			debug_float (adc_range[p][i].B[POLE_NORTH].fix);

			debug_float (adc_range[p][i].C[POLE_SOUTH].fix);
			debug_float (adc_range[p][i].C[POLE_NORTH].fix);

			debug_int32 (adc_range[p][i].thresh[POLE_SOUTH]);
			debug_int32 (adc_range[p][i].thresh[POLE_NORTH]);
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

			if (raw[p][i] > adc_range[p][i].thresh[POLE_SOUTH])
				adc_range[p][i].thresh[POLE_SOUTH] = raw[p][i];

			if (raw[p][i] < adc_range[p][i].thresh[POLE_NORTH])
				adc_range[p][i].thresh[POLE_NORTH] = raw[p][i];
		}
}

void
range_update_quiescent ()
{
	uint8_t p, i;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			// reset thresh to quiescent value
			adc_range[p][i].thresh[POLE_SOUTH] = adc_range[p][i].mean;
			adc_range[p][i].thresh[POLE_NORTH] = adc_range[p][i].mean;
		}
}

void
range_update_b0 ()
{
	uint8_t p, i;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].A[POLE_SOUTH].uint = adc_range[p][i].thresh[POLE_SOUTH] - adc_range[p][i].mean;
			adc_range[p][i].A[POLE_NORTH].uint = adc_range[p][i].mean - adc_range[p][i].thresh[POLE_NORTH];

			// reset thresh to quiescent value
			adc_range[p][i].thresh[POLE_SOUTH] = adc_range[p][i].mean;
			adc_range[p][i].thresh[POLE_NORTH] = adc_range[p][i].mean;
		}
}

void
range_update_b1 ()
{
	uint8_t p, i;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].B[POLE_SOUTH].uint = adc_range[p][i].thresh[POLE_SOUTH] - adc_range[p][i].mean;
			adc_range[p][i].B[POLE_NORTH].uint = adc_range[p][i].mean - adc_range[p][i].thresh[POLE_NORTH];

			// reset thresh to quiescent value
			adc_range[p][i].thresh[POLE_SOUTH] = adc_range[p][i].mean;
			adc_range[p][i].thresh[POLE_NORTH] = adc_range[p][i].mean;
		}
}

void
range_update_b2 ()
{
	uint8_t p, i;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].C[POLE_SOUTH].uint = adc_range[p][i].thresh[POLE_SOUTH] - adc_range[p][i].mean;
			adc_range[p][i].C[POLE_NORTH].uint = adc_range[p][i].mean - adc_range[p][i].thresh[POLE_NORTH];

			// reset thresh to quiescent value
			adc_range[p][i].thresh[POLE_SOUTH] = adc_range[p][i].mean;
			adc_range[p][i].thresh[POLE_NORTH] = adc_range[p][i].mean;
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
			ADC_Union *b0 = adc_range[p][i].A;
			ADC_Union *b1 = adc_range[p][i].B;
			ADC_Union *b2 = adc_range[p][i].C;

			ADC_Union *A = adc_range[p][i].A;
			ADC_Union *B = adc_range[p][i].B;
			ADC_Union *C = adc_range[p][i].C;

			curvefit (b0[POLE_SOUTH].uint, b1[POLE_SOUTH].uint, b2[POLE_SOUTH].uint, &A[POLE_SOUTH].fix, &B[POLE_SOUTH].fix, &C[POLE_SOUTH].fix);
			curvefit (b0[POLE_NORTH].uint, b1[POLE_NORTH].uint, b2[POLE_NORTH].uint, &A[POLE_NORTH].fix, &B[POLE_NORTH].fix, &C[POLE_NORTH].fix);

			adc_range[p][i].thresh[POLE_SOUTH] = By (A[POLE_SOUTH].fix, B[POLE_SOUTH].fix, C[POLE_SOUTH].fix);
			adc_range[p][i].thresh[POLE_NORTH] = By (A[POLE_NORTH].fix, B[POLE_NORTH].fix, C[POLE_NORTH].fix);
		}
}

uint8_t
groups_load ()
{
	uint8_t size;
	uint8_t *buf;

	if (magic_match ())
	{
		eeprom_byte_read (I2C1, EEPROM_GROUP_OFFSET_SIZE, &size);
		if (size)
		{
			buf = cmc_group_buf_set (size);
			eeprom_bulk_read (I2C1, EEPROM_GROUP_OFFSET_DATA, buf, size);
		}
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
	eeprom_byte_write (I2C1, EEPROM_GROUP_OFFSET_SIZE, size);
	eeprom_bulk_write (I2C1, EEPROM_GROUP_OFFSET_DATA, buf, size);

	return 1;
}

_check_range8 (uint8_t *val, uint8_t min, uint8_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, *val);
	}
	else
	{
		uint8_t arg = args[1].val.i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

_check_range16 (uint16_t *val, uint16_t min, uint16_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, *val);
	}
	else
	{
		uint16_t arg = args[1].val.i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

_check_rangefloat (float *val, float min, float max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTf", id, *val);
	}
	else
	{
		float arg = args[1].val.f;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_version (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTiii", id, config.version.major, config.version.minor, config.version.patch_level);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_name (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, config.name);
	}
	else
	{
		if (strlen (args[1].val.s) < NAME_LENGTH)
		{
			strcpy (config.name, args[1].val.s);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "name is too long");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (config_load ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "loading of configuration from EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (config_save ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "saving configuration to EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

#define MAC_STR_LEN 18
#define IP_STR_LEN 16
#define ADDR_STR_LEN 32

static uint8_t
str2mac (char *str, uint8_t *mac)
{
	return sscanf (str, "%02x:%02x:%02x:%02x:%02x:%02x",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}

static void
mac2str (uint8_t *mac, char *str)
{
	sprintf (str, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static uint8_t
str2ip (char *str, uint8_t *ip)
{
	return sscanf (str, "%hhi.%hhi.%hhi.%hhi",
		&ip[0], &ip[1], &ip[2], &ip[3]) == 4;
}

static void
ip2str (uint8_t *ip, char *str)
{
	sprintf (str, "%i.%i.%i.%i",
		ip[0], ip[1], ip[2], ip[3]);
}

static uint8_t
str2addr (char *str, uint8_t *ip, uint16_t *port)
{
	return sscanf (str, "%hhi.%hhi.%hhi.%hhi:%hi",
		&ip[0], &ip[1], &ip[2], &ip[3], port) == 5;
}

static void
addr2str (const char *protocol, const char *transport, uint8_t *ip, uint16_t port, char *str)
{
	sprintf (str, "%s.%s://%i.%i.%i.%i:%i",
		protocol, transport,
		ip[0], ip[1], ip[2], ip[3], port);
}

static uint8_t
_comm_mac (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		char mac_str[MAC_STR_LEN];
		mac2str (config.comm.mac, mac_str);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, mac_str);
	}
	else
	{
		uint8_t mac[6];
		if (str2mac (args[1].val.s, mac)) // TODO check if mac is valid
		{
			memcpy (config.comm.mac, mac, 6);

			wiz_mac_set (config.comm.mac);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in MAC must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_ip (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.ip, ip_str);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].val.s, ip)) //TODO check if ip is valid
		{
			memcpy (config.comm.ip, ip, 4);

			wiz_ip_set (config.comm.ip);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_gateway (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.gateway, ip_str);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].val.s, ip)) //TODO check if valid
		{
			memcpy (config.comm.gateway, ip, 4);

			wiz_gateway_set (config.comm.gateway);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_subnet (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.subnet, ip_str);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].val.s, ip)) //TODO check if valid
		{
			memcpy (config.comm.subnet, ip, 4);

			wiz_subnet_set (config.comm.subnet);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: all numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_get (uint8_t b, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (b)
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTF", id);

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_set (void (*cb) (uint8_t b), const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	cb (fmt[1] == nOSC_TRUE ? 1 : 0);
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
} 

static uint8_t
_tuio_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.tuio.enabled, path, fmt, argc, args);
	else
		return _enabled_set (tuio_enable, path, fmt, argc, args);
}

static uint8_t
_config_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.config.enabled, path, fmt, argc, args);
	else
		return _enabled_set (config_enable, path, fmt, argc, args);
}

static uint8_t
_sntp_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.sntp.enabled, path, fmt, argc, args);
	else
		return _enabled_set (sntp_enable, path, fmt, argc, args);
}

static uint8_t
_dump_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.dump.enabled, path, fmt, argc, args);
	else
		return _enabled_set (dump_enable, path, fmt, argc, args);
}

static uint8_t
_debug_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.debug.enabled, path, fmt, argc, args);
	else
		return _enabled_set (debug_enable, path, fmt, argc, args);
}

static uint8_t
_dhcpc_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.dhcpc.enabled, path, fmt, argc, args);
	else
		return _enabled_set (dhcpc_enable, path, fmt, argc, args);
}

static uint8_t
_address (Socket_Config *socket, void (*cb) (uint8_t b), const char *protocol, const char *transport, uint8_t flag, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		char addr[ADDR_STR_LEN];
		addr2str (protocol, transport, socket->ip, socket->port[DST_PORT], addr);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, addr);
	}
	else
	{
		uint16_t port;
		uint8_t ip[4];
		if (str2addr(args[1].val.s, ip, &port)) // TODO check if valid
		{
			debug_int32 (ip[0]);
			debug_int32 (ip[1]);
			debug_int32 (ip[2]);
			debug_int32 (ip[3]);
			debug_int32 (port);

			socket->port[DST_PORT] = port;
			memcpy (socket->ip, ip, 4);

			cb (flag);

			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
		}
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iFs", id, "wrong range: port number must be < 0x10000 and numbers in IP must be < 0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.tuio.socket, tuio_enable, "osc", "udp", config.tuio.enabled, path, fmt, argc, args);
}

static uint8_t
_config_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.config.socket, config_enable, "osc", "udp", config.config.enabled, path, fmt, argc, args);
}

static uint8_t
_sntp_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.sntp.socket, sntp_enable, "ntp", "udp", config.sntp.enabled, path, fmt, argc, args);
}

static uint8_t
_dump_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.dump.socket, dump_enable, "osc", "udp", config.dump.enabled, path, fmt, argc, args);
}

static uint8_t
_debug_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.debug.socket, debug_enable, "osc", "udp", config.debug.enabled, path, fmt, argc, args);
}

/*
static uint8_t
_dhcpc_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.dhcpc.socket, dhcpc_enable, "bootp", "udp", config.dhcpc.enabled, path, fmt, argc, args);
}
*/

static uint8_t
_host_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	uint8_t ip[4];
	if (str2ip (args[1].val.s, ip)) // TODO check if valid
	{
		memcpy (config.tuio.socket.ip, ip, 4);
		memcpy (config.config.socket.ip, ip, 4);
		memcpy (config.sntp.socket.ip, ip, 4);
		memcpy (config.dump.socket.ip, ip, 4);
		memcpy (config.debug.socket.ip, ip, 4);

		tuio_enable (config.tuio.enabled);
		config_enable (config.config.enabled);
		sntp_enable (config.sntp.enabled);
		dump_enable (config.dump.enabled);
		debug_enable (config.debug.enabled);
	}

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_rate (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range16 (&config.config.rate, 1, 10, path, fmt, argc, args);
}

static uint8_t
_sntp_tau (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.sntp.tau, 1, 10, path, fmt, argc, args);
}

static uint8_t
_tuio_long_header (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		if (config.tuio.long_header)
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTT");
		else
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTF");
	}
	else
	{
		tuio2_long_header_enable (fmt[1] == nOSC_TRUE ? 1 : 0);
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_offset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTt", config.tuio.offset);
	}
	else
	{
		config.tuio.offset = args[1].val.t;
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_rate (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (argc == 1) // query
	{
		if (config.rate)
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, config.rate);
		else // infinity
			size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTI", id);
	}
	else
	{
		switch (fmt[1])
		{
			case nOSC_INT32:
				config.rate = args[1].val.i;
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

		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_reset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;
	int32_t sec;

	if (argc > 1)
	{
		sec = args[1].val.i;
		if (sec < 1)
			sec = 1;
	}
	else
		sec = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	delay_us (sec * 1e6); // delay sec seconds until reset
	nvic_sys_reset ();

	return 1;
}

static uint8_t
_factory (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;
	int32_t sec;

	if (argc > 1)
	{
		sec = args[1].val.i;
		if (sec < 1)
			sec = 1;
	}
	else
		sec = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	// FIXME does not work as intended without VBAT powered up, needs a change on the PCB
	bkp_init ();
	bkp_enable_writes ();
	bkp_write (FACTORY_RESET_REG, FACTORY_RESET_VAL);
	bkp_disable_writes ();

	delay_us (sec * 1e6); // delay sec seconds until reset
	nvic_sys_reset ();

	return 1;
}

static uint8_t
_peak_thresh (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.cmc.peak_thresh, 3, 9, path, fmt, argc, args);
}

static uint8_t
_group_clear (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	cmc_group_clear ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_add (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (cmc_group_add (args[1].val.i, args[2].val.i, args[3].val.f, args[4].val.f))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "group already existing or maximal number of groups overstept");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (cmc_group_set (args[1].val.i, args[2].val.i, args[3].val.f, args[4].val.f))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "group not found");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_del (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (cmc_group_del (args[1].val.i))
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (groups_load ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	if (groups_save ())
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iF", id, "there was an error");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_start (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	// initialize sensor range
	uint8_t p, i;
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++)
		{
			adc_range[p][i].mean = ADC_HALF_BITDEPTH;

			adc_range[p][i].thresh[POLE_SOUTH] = ADC_HALF_BITDEPTH;
			adc_range[p][i].thresh[POLE_NORTH] = ADC_HALF_BITDEPTH;
		}

	// enable calibration
	calibrating = 1;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_zero (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	// update new range
	range_update_quiescent ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}


static uint8_t
_calibration_min (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	// update new range
	range_update_b0 ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_mid (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	Y1 = args[1].val.f;

	// update new range
	range_update_b1 ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_max (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	// disable calibration
	calibrating = 0;

	// update new range
	range_update_b2 ();
	range_update ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;
	uint8_t pos = config.calibration; // use default calibration

	if (argc == 2)
		pos = args[1].val.i; // use given calibration

	if (pos > EEPROM_RANGE_MAX)
		pos = EEPROM_RANGE_MAX;

	// store new calibration range to EEPROM
	range_save (pos);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;
	uint8_t pos = config.calibration; // use default calibration

	if (argc == 2)
		pos = args[1].val.i; // use given calibration

	if (pos > EEPROM_RANGE_MAX)
		pos = EEPROM_RANGE_MAX;

	// load calibration range from EEPROM
	range_load (pos);

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_print (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	// print calibration in RAM
	range_print ();

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_uid (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, EUI_96_STR);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_test (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	debug_int32 (sizeof (sat unsigned short fract));				// size = 1
	debug_int32 (sizeof (sat unsigned fract));							// size = 2
	debug_int32 (sizeof (sat unsigned long fract));					// size = 4
	debug_int32 (sizeof (sat unsigned long long fract));		// size = 8

	debug_int32 (sizeof (sat short fract));									// size = 1
	debug_int32 (sizeof (sat fract));												// size = 2
	debug_int32 (sizeof (sat long fract));									// size = 4
	debug_int32 (sizeof (sat long long fract));							// size = 8

	debug_int32 (sizeof (sat unsigned short accum));				// size = 2
	debug_int32 (sizeof (sat unsigned accum));							// size = 4
	debug_int32 (sizeof (sat unsigned long accum));					// size = 8
	debug_int32 (sizeof (sat unsigned long long accum));		// size = 8

	debug_int32 (sizeof (sat short accum));									// size = 2
	debug_int32 (sizeof (sat accum));												// size = 4
	debug_int32 (sizeof (sat long accum));									// size = 8
	debug_int32 (sizeof (sat long long accum));							// size = 8

	debug_str ("nOSC_Arg");
	debug_int32 (sizeof (nOSC_Type));
	debug_int32 (sizeof (nOSC_Union));
	debug_int32 (sizeof (nOSC_Arg));
	debug_int32 (sizeof (nOSC_Item));
	debug_int32 (sizeof (nOSC_Method));
	debug_int32 (sizeof (int32_t));
	debug_int32 (sizeof (float));
	debug_int32 (sizeof (char *));
	debug_int32 (sizeof (nOSC_Blob));
	debug_int32 (sizeof (int64_t));
	debug_int32 (sizeof (double));
	debug_int32 (sizeof (uint64_t));
	debug_int32 (sizeof (uint8_t [4]));
	debug_int32 (sizeof (char *));
	debug_int32 (sizeof (char));

	struct {
		union {
			uint32_t i;
		} u;
		char c;
	} s;
	debug_int32 (sizeof (s));

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_echo (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].val.i;

	uint8_t i;
	for (i=1; i<argc; i++)
		switch (args[i].type)
		{
			case nOSC_INT32:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTi", id, args[i].val.i);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_FLOAT:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTf", id, args[i].val.f);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_STRING:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTs", id, args[i].val.s);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_BLOB:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTb", id, args[i].val.b.size, args[i].val.b.data);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_TRUE:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTT", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_FALSE:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTF", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_NIL:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTN", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_INFTY:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTI", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_INT64:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTh", id, args[i].val.h);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_DOUBLE:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTd", id, args[i].val.d);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_TIMESTAMP:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTt", id, args[i].val.t);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_MIDI:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTm", id, args[i].val.m);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_SYMBOL:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTS", id, args[i].val.S);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_CHAR:
				size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], CONFIG_REPLY_PATH, "iTc", id, args[i].val.c);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
		}

	return 1;
}

nOSC_Method config_serv [] = {
	{"/chimaera/version", "i", _version},

	{"/chimaera/name", "i", _name},
	{"/chimaera/name", "is", _name},

	{"/chimaera/config/load", "i", _config_load},
	{"/chimaera/config/save", "i", _config_save},

	{"/chimaera/comm/mac", "i", _comm_mac},
	{"/chimaera/comm/mac", "is", _comm_mac},
	{"/chimaera/comm/ip", "i", _comm_ip},
	{"/chimaera/comm/ip", "is", _comm_ip},
	{"/chimaera/comm/gateway", "i", _comm_gateway},
	{"/chimaera/comm/gateway", "is", _comm_gateway},
	{"/chimaera/comm/subnet", "i", _comm_subnet},
	{"/chimaera/comm/subnet", "is", _comm_subnet},

	{"/chimaera/tuio/enabled", "i", _tuio_enabled},
	{"/chimaera/tuio/enabled", "iT", _tuio_enabled},
	{"/chimaera/tuio/enabled", "iF", _tuio_enabled},
	{"/chimaera/tuio/address", "i", _tuio_address},
	{"/chimaera/tuio/address", "is", _tuio_address},
	{"/chimaera/tuio/long_header", "i", _tuio_long_header},
	{"/chimaera/tuio/long_header", "iT", _tuio_long_header},
	{"/chimaera/tuio/long_header", "iF", _tuio_long_header},
	{"/chimaera/tuio/offset", "i", _tuio_offset},
	{"/chimaera/tuio/offset", "it", _tuio_offset},

	{"/chimaera/config/enabled", "i", _config_enabled},
	{"/chimaera/config/enabled", "iT", _config_enabled},
	{"/chimaera/config/enabled", "iF", _config_enabled},
	{"/chimaera/config/address", "i", _config_address},
	{"/chimaera/config/address", "is", _config_address},
	{"/chimaera/config/rate", "i", _config_rate},
	{"/chimaera/config/rate", "ii", _config_rate},

	{"/chimaera/sntp/enabled", "i", _sntp_enabled},
	{"/chimaera/sntp/enabled", "iT", _sntp_enabled},
	{"/chimaera/sntp/enabled", "iF", _sntp_enabled},
	{"/chimaera/sntp/address", "i", _sntp_address},
	{"/chimaera/sntp/address", "is", _sntp_address},
	{"/chimaera/sntp/tau", "i", _sntp_tau},
	{"/chimaera/sntp/tau", "ii", _sntp_tau},

	{"/chimaera/dump/enabled", "i", _dump_enabled},
	{"/chimaera/dump/enabled", "iT", _dump_enabled},
	{"/chimaera/dump/enabled", "iF", _dump_enabled},
	{"/chimaera/dump/address", "i", _dump_address},
	{"/chimaera/dump/address", "is", _dump_address},

	{"/chimaera/debug/enabled", "i", _debug_enabled},
	{"/chimaera/debug/enabled", "iT", _debug_enabled},
	{"/chimaera/debug/enabled", "iF", _debug_enabled},
	{"/chimaera/debug/address", "i", _debug_address},
	{"/chimaera/debug/address", "is", _debug_address},

	{"/chimaera/dhcpc/enabled", "i", _dhcpc_enabled},
	{"/chimaera/dhcpc/enabled", "iT", _dhcpc_enabled},
	{"/chimaera/dhcpc/enabled", "iF", _dhcpc_enabled},
	//{"/chimaera/dhcpc/socket", "i", _dhcpc_socket},
	//{"/chimaera/dhcpc/socket", "iiiiiii", _dhcpc_socket},

	{"/chimaera/host/address", "is", _host_address},

	//TODO
	//{"/chimaera/zeroconf/enabled", "i", _zeroconf_enabled},
	//{"/chimaera/zeroconf/enabled", "iT", _zeroconf_enabled},
	//{"/chimaera/zeroconf/enabled", "iF", _zeroconf_enabled},

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

	{"/chimaera/factory", "i", _factory},
	{"/chimaera/factory", "ii", _factory},

	{"/chimaera/calibration/start", "i", _calibration_start},
	{"/chimaera/calibration/zero", "i", _calibration_zero},
	{"/chimaera/calibration/min", "i", _calibration_min},
	{"/chimaera/calibration/mid", "if", _calibration_mid},
	{"/chimaera/calibration/max", "i", _calibration_max},
	{"/chimaera/calibration/print", "i", _calibration_print},
	{"/chimaera/calibration/save", "i", _calibration_save},
	{"/chimaera/calibration/save", "ii", _calibration_save},
	{"/chimaera/calibration/load", "i", _calibration_load},
	{"/chimaera/calibration/load", "ii", _calibration_load},

	{"/chimaera/uid", "i", _uid},

	//TODO remove
	{"/chimaera/test", "i", _test},
	{"/chimaera/echo", NULL, _echo},

	{NULL, NULL, NULL} // terminator
};
