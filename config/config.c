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
#include <scsynth.h>
#include <midi.h>

const char *success_str = "/success";
const char *fail_str = "/fail";
const char *wrong_ip_port_error_str = "wrong range: all numbers in IP must be < 0x100";
const char *group_err_str = "group not found";

#define CONFIG_SUCCESS(...) (nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], success_str, __VA_ARGS__))
#define CONFIG_FAIL(...) (nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], fail_str, __VA_ARGS__))

#define GLOB_BROADCAST {255, 255, 255, 255}
#define LAN_BROADCAST {192, 168, 1, 255}
#define LAN_HOST {192, 168, 1, 10}

// FIXME set those for Zeroconf
//#define LAN_BROADCAST {169, 254, 255, 255}
//#define LAN_HOST {169, 254, 205, 27}

// FIXME set those for DHCP
//#define LAN_BROADCAST {46, 126, 89, 221}
//#define LAN_HOST {46, 126, 89, 221}

float Y1 = 0.7;

/*
static fix_16_16_t
By (fix_s15_16_t A, fix_s15_16_t B, fix_s15_16_t C)
{
	float a = A;
	float b = B;
	float c = C;
	float y = Y1;
	float _b = (a*a - 2.0*b*c + 2.0*b*y - a*sqrt(a*a - 4.0*b*c + 4.0*b*y)) / (2.0*b*b);
	return (fix_16_16_t)_b*0x7ff;
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

	//debug_str ("curvefit");
	//debug_float (Y1);
	//debug_int32 (b0);
	//debug_int32 (b1);
	//debug_int32 (b2);
	//debug_float (B0);
	//debug_float (B1);
	//debug_float (B2);
	//debug_float (sqrtB0);
	//debug_float (sqrtB1);
	//debug_float (sqrtB2);
	//debug_float (*A);
	//debug_float (*B);
	//debug_float (*C);
}
*/

/* rev 4 */
Range range;
Curve curve = { //FIXME make this configurable
	.A = 0.7700LLK,
	.B = 0.2289LLK,
	.C = 0.0000LLK
};
/* rev 4 */

float
_as (uint16_t qui, uint16_t out_s, uint16_t out_n, uint16_t b)
{
	float _qui = (float)qui;
	float _out_s = (float)out_s;
	float _out_n = (float)out_n;
	float _b = (float)b;

	return _qui / _b * (_out_s - _out_n) / (_out_s + _out_n);
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
		.locally = 0,
		.mac = {(0x1a | 0b00000010) & 0b11111110, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f}, // locally administered unicast MAC
		.ip = {192, 168, 1, 177},
		.gateway = {192, 168, 1, 1},
		.subnet = {255, 255, 255, 0},
		.subnet_check = 0 //TODO make this configurable
	},

	.tuio = {
		.enabled = 0,
		.version = 2, //TODO implement
		.long_header = 0
	},

	.dump = {
		.enabled = 0
	},

	.scsynth = {
		.enabled = 0,
		.instrument = {'c', 'h', 'i', 'm', 'i', 'n', 's', 't', '\0'},
		.offset = 1000,
		.modulo = 8,
		.prealloc = 1,
		.addaction = SCSYNTH_ADD_TO_HEAD
	},
	
	.output = {
		.enabled = 1,
		.socket = {
			.sock = 1,
			.port = {3333, 3333},
			.ip = LAN_BROADCAST
		},
		.offset = 0ULLK
	},

	.config = {
		.rate = 10, // rate in Hz
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 2,
			.port = {4444, 4444},
			.ip = LAN_BROADCAST
		}
	},

	.sntp = {
		.tau = 4, // delay between SNTP requests in seconds
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 3,
			.port = {123, 123},
			.ip = LAN_HOST
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

	.ipv4ll = {
		.enabled = 0
	},

	.mdns = {
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
		//.socket = {
		//	.sock = 7,
		//	.port = {7777, 7777},
		//	.ip = LAN_BROADCAST
		//}
	},

	.oscmidi = {
		.enabled = 0,
		.offset = 24,
		.effect = VOLUME
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
	eeprom_byte_read (eeprom_24LC64, EEPROM_CONFIG_OFFSET, &magic);

	return magic == config.magic; // check whether EEPROM and FLASH config magic number match
}

uint8_t
config_load ()
{
	if (magic_match ())
		eeprom_bulk_read (eeprom_24LC64, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
		config_save ();

	return 1;
}

uint8_t
config_save ()
{
	eeprom_bulk_write (eeprom_24LC64, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));
	return 1;
}

void
adc_fill (int16_t raw[16][10], uint8_t order[16][9], int16_t *rela, int16_t *swap, uint8_t relative)
{
	//NOTE conditionals have been taken out of the loop, makes it MUCH faster
	if (relative)
	{
		if (config.dump.enabled)
		{
			uint8_t p, i;
			for (p=0; p<MUX_MAX; p++)
				for (i=0; i<ADC_LENGTH; i++)
				{
					uint8_t pos = order[p][i];
					int16_t val = raw[p][i] - range.qui[pos];

					rela[pos] = val; 
					swap[pos] = hton (val);
				}
		}
		else // !config.dump.enabled
		{
			uint8_t p, i;
			for (p=0; p<MUX_MAX; p++)
				for (i=0; i<ADC_LENGTH; i++)
				{
					uint8_t pos = order[p][i];
					int16_t val = raw[p][i] - range.qui[pos];

					rela[pos] = val; 
				}
		}
	}
	else // absolute
	{
		if (config.dump.enabled)
		{
			uint8_t p, i;
			for (p=0; p<MUX_MAX; p++)
				for (i=0; i<ADC_LENGTH; i++)
				{
					uint8_t pos = order[p][i];
					int16_t val = raw[p][i];

					rela[pos] = val; 
					swap[pos] = hton (val);
				}
		}
		else // !config.dump.enabled
		{
			uint8_t p, i;
			for (p=0; p<MUX_MAX; p++)
				for (i=0; i<ADC_LENGTH; i++)
				{
					uint8_t pos = order[p][i];
					int16_t val = raw[p][i];

					rela[pos] = val; 
				}
		}
	}
}

uint8_t
range_load (uint8_t pos)
{
	if (magic_match ()) // EEPROM and FLASH config versions match
		eeprom_bulk_read (eeprom_24LC64, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&range, sizeof (range));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
	{
		uint8_t i;
		for (i=0; i<SENSOR_N; i++)
		{
			range.thresh[i] = 0;
			range.qui[i] = 0x7ff;
			range.as_1_sc_1[i] = 1ULR;
			range.bmin_sc_1 = 0ULR;
		}

		range_save (pos);
	}

	return 1;
}

uint8_t
range_save (uint8_t pos)
{
	eeprom_bulk_write (eeprom_24LC64, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&range, sizeof (range));

	return 1;
}

/*
nOSC_Arg _p [1];
nOSC_Arg _i [1];
nOSC_Arg _m [1];

nOSC_Arg _sa [1];
nOSC_Arg _sb [1];
nOSC_Arg _sc [1];
nOSC_Arg _st [1];

nOSC_Arg _na [1];
nOSC_Arg _nb [1];
nOSC_Arg _nc [1];
nOSC_Arg _nt [1];

const nOSC_Item _s [] = {
	nosc_message (_sa, "/A", "f"),
	nosc_message (_sb, "/B", "f"),
	nosc_message (_sc, "/C", "f"),
	nosc_message (_st, "/thresh", "i")
};

const nOSC_Item _n [] = {
	nosc_message (_na, "/A", "f"),
	nosc_message (_nb, "/B", "f"),
	nosc_message (_nc, "/C", "f"),
	nosc_message (_nt, "/thresh", "i")
};

const nOSC_Item calib_out [] = {
	nosc_message (_i, "/i", "i"),
	nosc_message (_m, "/mean", "i"),
	nosc_bundle ((nOSC_Item *)_s, nOSC_IMMEDIATE, "MMMM"),
	nosc_bundle ((nOSC_Item *)_n, nOSC_IMMEDIATE, "MMMM")
};

const char *calib_fmt = "MMBB";

uint8_t
range_print ()
{
	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		nosc_message_set_int32 (_i, 0, i);
		nosc_message_set_int32 (_m, 0, adc_range[i].mean);

		nosc_message_set_float (_sa, 0, adc_range[i].A[POLE_SOUTH].fix);
		nosc_message_set_float (_sb, 0, adc_range[i].A[POLE_SOUTH].fix);
		nosc_message_set_float (_sc, 0, adc_range[i].B[POLE_SOUTH].fix);
		nosc_message_set_int32 (_st, 0, adc_range[i].thresh[POLE_SOUTH]);

		nosc_message_set_float (_na, 0, adc_range[i].A[POLE_NORTH].fix);
		nosc_message_set_float (_nb, 0, adc_range[i].A[POLE_NORTH].fix);
		nosc_message_set_float (_nc, 0, adc_range[i].B[POLE_NORTH].fix);
		nosc_message_set_int32 (_nt, 0, adc_range[i].thresh[POLE_NORTH]);

		uint16_t size = nosc_bundle_serialize ((nOSC_Item *)calib_out, nOSC_IMMEDIATE, (char *)calib_fmt, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
		udp_send (config.config.socket.sock, buf_o_ptr, size);
	}

	return 1;
}
*/

uint16_t arr [2][SENSOR_N]; //FIXME reuse some other memory
uint8_t zeroing = 0;

void
range_calibrate (int16_t *raw)
{
	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		if (zeroing)
		{
			range.qui[i] += raw[i];
			range.qui[i] /= 2;
		}

		if (raw[i] > arr[POLE_SOUTH][i])
			arr[POLE_SOUTH][i] = raw[i];

		if (raw[i] < arr[POLE_NORTH][i])
			arr[POLE_NORTH][i] = raw[i];
	}
}

// calibrate quiescent current
void
range_update_quiescent ()
{
	uint8_t i;

	for (i=0; i<SENSOR_N; i++)
	{
		// reset array to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i];
		arr[POLE_NORTH][i] = range.qui[i];
	}
}

// calibrate threshold
void
range_update_b0 ()
{
	uint8_t i;
	uint16_t thresh_s, thresh_n;

	for (i=0; i<SENSOR_N; i++)
	{
		thresh_s = arr[POLE_SOUTH][i] - range.qui[i];
		thresh_n = range.qui[i] - arr[POLE_NORTH][i];

		range.thresh[i] = (thresh_s + thresh_n) / 2;

		// reset thresh to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i];
		arr[POLE_NORTH][i] = range.qui[i];
	}
}

// calibrate amplification and sensitivity
void
range_update_b1 ()
{
	uint8_t i;
	uint16_t b = (float)0x7ff * Y1;
	float as_1;
	float bmin, bmax_s, bmax_n;
	float sc_1;

	float m_bmin = 0;
	float m_bmax = 0;

	for (i=0; i<SENSOR_N; i++)
	{
		as_1 = 1.0 / _as (range.qui[i], arr[POLE_SOUTH][i], arr[POLE_NORTH][i], b);

		bmin = (float)range.thresh[i] * as_1;
		bmax_s = ((float)arr[POLE_SOUTH][i] - (float)range.qui[i]) * as_1 / Y1;
		bmax_n = ((float)range.qui[i] - (float)arr[POLE_NORTH][i]) * as_1 / Y1;

		m_bmin += bmin;
		m_bmax += (bmax_s + bmax_n) / 2.0;
	}

	m_bmin /= (float)SENSOR_N;
	m_bmax /= (float)SENSOR_N;

	sc_1 = 1.0 / (m_bmax - m_bmin);
	range.bmin_sc_1 = m_bmin * sc_1;

	for (i=0; i<SENSOR_N; i++)
	{
		as_1 = 1.0 / _as (range.qui[i], arr[POLE_SOUTH][i], arr[POLE_NORTH][i], b);
		range.as_1_sc_1[i] = as_1 * sc_1;

		// reset thresh to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i];
		arr[POLE_NORTH][i] = range.qui[i];
	}
}

uint8_t
groups_load ()
{
	uint16_t size;
	uint8_t *buf;

	if (magic_match ())
	{
		buf = cmc_group_buf_get (&size);
		eeprom_bulk_read (eeprom_24LC64, EEPROM_GROUP_OFFSET, buf, size);
	}
	else
		groups_save ();

	return 1;
}

uint8_t
groups_save ()
{
	uint16_t size;
	uint8_t *buf;

	buf = cmc_group_buf_get (&size);
	eeprom_bulk_write (eeprom_24LC64, EEPROM_GROUP_OFFSET, buf, size);

	return 1;
}

static uint8_t
_check_bool (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args, uint8_t *boolean)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("ii", id, *boolean ? 1 : 0);
	}
	else
	{
		switch (fmt[1])
		{
			case nOSC_INT32:
				*boolean = args[1].i ? 1 : 0;
				break;
			case nOSC_TRUE:
				*boolean = 1;
				break;
			case nOSC_FALSE:
				*boolean = 0;
				break;
		}
		size = CONFIG_SUCCESS ("i", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_check_range8 (uint8_t *val, uint8_t min, uint8_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("ii", id, *val);
	}
	else
	{
		uint8_t arg = args[1].i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = CONFIG_FAIL ("is", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_check_range16 (uint16_t *val, uint16_t min, uint16_t max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("ii", id, *val);
	}
	else
	{
		uint16_t arg = args[1].i;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = CONFIG_FAIL ("is", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_check_rangefloat (float *val, float min, float max, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("if", id, *val);
	}
	else
	{
		float arg = args[1].f;
		if ( (arg >= min) && (arg < max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
		{
			char buf[64];
			sprintf (buf, "value %i is out of range [%i, %i]", arg, min, max);
			size = CONFIG_FAIL ("is", id, buf);
		}
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_version (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	char version[16]; // FIXME share string buffer space between config methods
	sprintf (version, "%i.%i.%i", config.version.major, config.version.minor, config.version.patch_level);
	size = CONFIG_SUCCESS ("is", id, version);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_name (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("is", id, config.name);
	}
	else
	{
		if (strlen (args[1].s) < NAME_LENGTH)
		{
			strcpy (config.name, args[1].s);

			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, "name is too long");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (config_load ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "loading of configuration from EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_config_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (config_save ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "saving configuration to EEPROM failed");

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

//TODO move to chimutil
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
_comm_locally (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.comm.locally);
}

static uint8_t
_comm_mac (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		char mac_str[MAC_STR_LEN];
		mac2str (config.comm.mac, mac_str);
		size = CONFIG_SUCCESS ("is", id, mac_str);
	}
	else
	{
		uint8_t mac[6];
		if (str2mac (args[1].s, mac)) // TODO check if mac is valid
		{
			memcpy (config.comm.mac, mac, 6);
			wiz_mac_set (config.comm.mac);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, "wrong range: all numbers in MAC must be <0x100");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_ip (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.ip, ip_str);
		size = CONFIG_SUCCESS ("is", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].s, ip)) //TODO check if ip is valid
		{
			memcpy (config.comm.ip, ip, 4);
			wiz_ip_set (config.comm.ip);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, wrong_ip_port_error_str);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_gateway (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.gateway, ip_str);
		size = CONFIG_SUCCESS ("is", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].s, ip)) //TODO check if valid
		{
			memcpy (config.comm.gateway, ip, 4);
			wiz_gateway_set (config.comm.gateway);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, wrong_ip_port_error_str);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_comm_subnet (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		char ip_str[IP_STR_LEN];
		ip2str (config.comm.subnet, ip_str);
		size = CONFIG_SUCCESS ("is", id, ip_str);
	}
	else
	{
		uint8_t ip[4];
		if (str2ip (args[1].s, ip)) //TODO check if valid
		{
			memcpy (config.comm.subnet, ip, 4);
			wiz_subnet_set (config.comm.subnet);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, wrong_ip_port_error_str);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_get (uint8_t b, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_SUCCESS ("ii", id, b ? 1 : 0);

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_enabled_set (void (*cb) (uint8_t b), const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	cb (args[1].i);
	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
} 

static uint8_t
_output_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	if (argc == 1) // query
		return _enabled_get (config.output.enabled, path, fmt, argc, args);
	else
		return _enabled_set (output_enable, path, fmt, argc, args);
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

const char *addr_err_str = "wrong range: port number must be < 0x10000 and numbers in IP must be < 0x100"; //TODO move me up

static uint8_t
_address (Socket_Config *socket, void (*cb) (uint8_t b), const char *protocol, const char *transport, uint8_t flag, const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		char addr[ADDR_STR_LEN];
		addr2str (protocol, transport, socket->ip, socket->port[DST_PORT], addr);
		size = CONFIG_SUCCESS ("is", id, addr);
	}
	else
	{
		uint16_t port;
		uint8_t ip[4];
		if (str2addr(args[1].s, ip, &port)) // TODO check if valid
		{
			socket->port[DST_PORT] = port;
			memcpy (socket->ip, ip, 4);
			cb (flag);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, addr_err_str);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_output_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.output.socket, output_enable, "osc", "udp", config.output.enabled, path, fmt, argc, args);
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
_debug_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _address (&config.debug.socket, debug_enable, "osc", "udp", config.debug.enabled, path, fmt, argc, args);
}

static uint8_t
_host_address (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	uint8_t ip[4];
	if (str2ip (args[1].s, ip)) // TODO check if valid
	{
		memcpy (config.output.socket.ip, ip, 4);
		memcpy (config.config.socket.ip, ip, 4);
		memcpy (config.sntp.socket.ip, ip, 4);
		memcpy (config.debug.socket.ip, ip, 4);

		output_enable (config.output.enabled);
		config_enable (config.config.enabled);
		sntp_enable (config.sntp.enabled);
		debug_enable (config.debug.enabled);
	}

	size = CONFIG_SUCCESS ("i", id);
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
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("ii", id, config.tuio.long_header ? 1 : 0);
	}
	else
	{
		tuio2_long_header_enable (args[1].i);
		size = CONFIG_SUCCESS ("i", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_tuio_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.tuio.enabled);
}

static uint8_t
_dump_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.dump.enabled);
}

static uint8_t
_scsynth_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.scsynth.enabled);
}

//TODO make arbitrary function to read/write string
static uint8_t
_scsynth_instrument (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("is", id, config.scsynth.instrument);
	}
	else
	{
		if (strlen (args[1].s) < NAME_LENGTH)
		{
			strcpy (config.scsynth.instrument, args[1].s);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, "name is too long");
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_scsynth_offset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range16 (&config.scsynth.offset, 0x0000, 0xffff, path, fmt, argc, args);
}

static uint8_t
_scsynth_modulo (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range16 (&config.scsynth.modulo, 0x0000, 0xffff, path, fmt, argc, args);
}

static uint8_t
_scsynth_prealloc (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.scsynth.prealloc);
}

static uint8_t
_scsynth_addaction (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.scsynth.addaction, 0, 4, path, fmt, argc, args);
}

static uint8_t
_rtpmidi_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.rtpmidi.enabled);
}

static uint8_t
_oscmidi_enabled (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.oscmidi.enabled);
}

static uint8_t
_oscmidi_offset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.oscmidi.offset, 0, 0x7f, path, fmt, argc, args);
}

static uint8_t
_oscmidi_effect (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.oscmidi.effect, 0, 0x7f, path, fmt, argc, args);
}

static uint8_t
_output_offset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("id", id, (double)config.output.offset); // output timestamp, double, float?
	}
	else
	{
		switch (fmt[1])
		{
			case nOSC_TIMESTAMP:
				config.output.offset = args[1].t;
				break;
			case nOSC_FLOAT:
				config.output.offset = args[1].f;
				break;
			case nOSC_DOUBLE:
				config.output.offset = args[1].d;
				break;
		}
		size = CONFIG_SUCCESS ("i", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_rate (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		if (config.rate > 0)
			size = CONFIG_SUCCESS ("ii", id, config.rate);
		else // infinity
			size = CONFIG_SUCCESS ("ii", id, nOSC_Infty);
	}
	else
	{
		if (args[1].i < nOSC_Infty) // TODO also check 16bit size
			config.rate = args[1].i;
		else
			config.rate = 0;

		if (config.rate)
		{
			timer_pause (adc_timer);
			adc_timer_reconfigure ();
			timer_resume (adc_timer);
		}
		else
			timer_pause (adc_timer);

		size = CONFIG_SUCCESS ("i", id);
	}

	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_reset (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	int32_t sec;

	if (argc > 1)
	{
		sec = args[1].i;
		if (sec < 1)
			sec = 1;
	}
	else
		sec = 1;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	delay_us (sec * 1e6); // delay sec seconds until reset
	nvic_sys_reset ();

	return 1;
}

static uint8_t
_factory (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	int32_t sec;

	if (argc > 1)
	{
		sec = args[1].i;
		if (sec < 1)
			sec = 1;
	}
	else
		sec = 1;

	size = CONFIG_SUCCESS ("i", id);
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
	int32_t id = args[0].i;

	cmc_group_clear ();

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_get (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	char *name;
	uint16_t pid;
	float x0;
	float x1;

	if (cmc_group_get (args[1].i, &name, &pid, &x0, &x1))
		size = CONFIG_SUCCESS ("isiff", id, name, pid, x0, x1);
	else
		size = CONFIG_FAIL ("is", id, group_err_str);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_set (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (cmc_group_set (args[1].i, args[2].s, args[3].i, args[4].f, args[5].f))
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, group_err_str);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (groups_load ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "groups could not be loaded from EEPROM");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_group_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (groups_save ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "groups could not be saved to EEPROM");
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_start (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// initialize sensor range
	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		range.qui[i] = ADC_HALF_BITDEPTH;
		
		arr[POLE_SOUTH][i] = range.qui[i];
		arr[POLE_NORTH][i] = range.qui[i];
	}

	// enable calibration
	zeroing = 1;
	calibrating = 1;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_zero (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// update new range
	zeroing = 0;
	range_update_quiescent ();

	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/range/qui", "iii", i, range.qui[i], range.qui[i]-0x7ff);
		udp_send (config.config.socket.sock, buf_o_ptr, size);
	}

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_min (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// update new range
	range_update_b0 ();

	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/range/thresh", "ii", i, range.thresh[i]);
		udp_send (config.config.socket.sock, buf_o_ptr, size);
	}

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_mid (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	Y1 = args[1].f;

	// update new range
	range_update_b1 ();

	uint8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/range/sc_1_sc_1", "if", i, (float)range.as_1_sc_1[i]);
		udp_send (config.config.socket.sock, buf_o_ptr, size);
	}

	calibrating = 0;

	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/range/bmin_sc_1", "f", (float)range.bmin_sc_1);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_save (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint8_t pos = config.calibration; // use default calibration

	if (argc == 2)
		pos = args[1].i; // use given calibration

	if (pos > EEPROM_RANGE_MAX)
		pos = EEPROM_RANGE_MAX;

	// store new calibration range to EEPROM
	range_save (pos);

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_load (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint8_t pos = config.calibration; // use default calibration

	if (argc == 2)
		pos = args[1].i; // use given calibration

	if (pos > EEPROM_RANGE_MAX)
		pos = EEPROM_RANGE_MAX;

	// load calibration range from EEPROM
	range_load (pos);

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_calibration_print (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// print calibration in RAM
	//range_print (); FIXME

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_uid (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_SUCCESS ("is", id, EUI_96_STR);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

/*
static uint8_t
_test (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

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
	debug_int32 (sizeof (nOSC_Arg));
	debug_int32 (sizeof (nOSC_Item));
	debug_int32 (sizeof (nOSC_Method));
	debug_int32 (sizeof (nOSC_Timestamp));
	debug_int32 (sizeof (int32_t));
	debug_int32 (sizeof (float));
	debug_int32 (sizeof (char *));
	debug_int32 (sizeof (nOSC_Blob));
	debug_int32 (sizeof (int64_t));
	debug_int32 (sizeof (double));
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

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

static uint8_t
_echo (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	uint8_t i;
	for (i=1; i<argc; i++)
		switch (fmt[i])
		{
			case nOSC_INT32:
				CONFIG_SUCCESS ("ii", id, args[i].i);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_FLOAT:
				CONFIG_SUCCESS ("if", id, args[i].f);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_STRING:
				CONFIG_SUCCESS ("is", id, args[i].s);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_BLOB:
				CONFIG_SUCCESS ("ib", id, args[i].b.size, args[i].b.data);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_TRUE:
				CONFIG_SUCCESS ("iT", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_FALSE:
				CONFIG_SUCCESS ("iF", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_NIL:
				CONFIG_SUCCESS ("iN", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_INFTY:
				CONFIG_SUCCESS ("iI", id);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_INT64:
				CONFIG_SUCCESS ("ih", id, args[i].h);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_DOUBLE:
				CONFIG_SUCCESS ("id", id, args[i].d);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_TIMESTAMP:
				CONFIG_SUCCESS ("it", id, args[i].t);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;

			case nOSC_MIDI:
				CONFIG_SUCCESS ("im", id, args[i].m);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_SYMBOL:
				CONFIG_SUCCESS ("iS", id, args[i].S);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
			case nOSC_CHAR:
				CONFIG_SUCCESS ("ic", id, args[i].c);
				udp_send (config.config.socket.sock, buf_o_ptr, size);
				break;
		}

	return 1;
}
*/

static uint8_t
_non (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_FAIL ("isss", id, "unknown method for path or format", path, fmt);
	udp_send (config.config.socket.sock, buf_o_ptr, size);

	return 1;
}

const nOSC_Method config_serv [] = {
	{"/chimaera/version", "i", _version},

	{"/chimaera/name", "i*", _name},

	{"/chimaera/config/load", "i", _config_load},
	{"/chimaera/config/save", "i", _config_save},

	{"/chimaera/comm/locally", "i*", _comm_locally},
	{"/chimaera/comm/mac", "i*", _comm_mac},
	{"/chimaera/comm/ip", "i*", _comm_ip},
	{"/chimaera/comm/gateway", "i*", _comm_gateway},
	{"/chimaera/comm/subnet", "i*", _comm_subnet},

	{"/chimaera/output/enabled", "i*", _output_enabled},
	{"/chimaera/output/address", "i*", _output_address},
	{"/chimaera/output/offset", "i*", _output_offset},

	{"/chimaera/tuio/enabled", "i*", _tuio_enabled},
	{"/chimaera/tuio/long_header", "i*", _tuio_long_header},

	{"/chimaera/dump/enabled", "i*", _dump_enabled},

	{"/chimaera/scsynth/enabled", "i*", _scsynth_enabled},
	{"/chimaera/scsynth/instrument", "i*", _scsynth_instrument},
	{"/chimaera/scsynth/offset", "i*", _scsynth_offset},
	{"/chimaera/scsynth/modulo", "i*", _scsynth_modulo},
	{"/chimaera/scsynth/prealloc", "i*", _scsynth_prealloc},
	{"/chimaera/scsynth/addaction", "i*", _scsynth_addaction},

	{"/chimaera/rtpmidi/enabled", "i*", _rtpmidi_enabled},

	{"/chimaera/oscmidi/enabled", "i*", _oscmidi_enabled},
	{"/chimaera/oscmidi/offset", "i*", _oscmidi_offset},
	{"/chimaera/oscmidi/effect", "i*", _oscmidi_effect},

	{"/chimaera/config/enabled", "i*", _config_enabled},
	{"/chimaera/config/address", "i*", _config_address},
	{"/chimaera/config/rate", "i*", _config_rate},

	{"/chimaera/sntp/enabled", "i*", _sntp_enabled},
	{"/chimaera/sntp/address", "i*", _sntp_address},
	{"/chimaera/sntp/tau", "i*", _sntp_tau},

	{"/chimaera/debug/enabled", "i*", _debug_enabled},
	{"/chimaera/debug/address", "i*", _debug_address},

	{"/chimaera/dhcpc/enabled", "i*", _dhcpc_enabled},

	{"/chimaera/host/address", "is", _host_address},

	//TODO
	//{"/chimaera/mdns/enabled", "i*", _mdns_enabled},

	{"/chimaera/peak_thresh", "i*", _peak_thresh},

	{"/chimaera/group/clear", "i", _group_clear},
	{"/chimaera/group/get", "ii", _group_get},
	{"/chimaera/group/set", "iisiff", _group_set},

	{"/chimaera/group/load", "i", _group_load},
	{"/chimaera/group/save", "i", _group_save},

	{"/chimaera/rate", "i*", _rate},

	{"/chimaera/reset", "i*", _reset},

	{"/chimaera/factory", "i*", _factory},

	{"/chimaera/calibration/start", "i", _calibration_start},
	{"/chimaera/calibration/zero", "i", _calibration_zero},
	{"/chimaera/calibration/min", "i", _calibration_min},
	{"/chimaera/calibration/mid", "if", _calibration_mid},
	{"/chimaera/calibration/print", "i", _calibration_print},
	{"/chimaera/calibration/save", "i*", _calibration_save},
	{"/chimaera/calibration/load", "i*", _calibration_load},

	{"/chimaera/uid", "i", _uid},

	//TODO remove
	/*
	{"/chimaera/test", "i", _test},
	{"/chimaera/echo", NULL, _echo},
	*/

	{NULL, NULL, _non}, // if nothing else matches, we give back an error saying so

	{NULL, NULL, NULL} // terminator
};
