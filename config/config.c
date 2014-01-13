/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
#include <calibration.h>
#include <sntp.h>

static char string_buf [64];
static const char *success_str = "/success";
static const char *fail_str = "/fail";
static const char *local_str = ".local";

#define CONFIG_SUCCESS(...) (nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), success_str, __VA_ARGS__))
#define CONFIG_FAIL(...) (nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), fail_str, __VA_ARGS__))

#define IP_BROADCAST {255, 255, 255, 255}

static void _host_address_dns_cb (uint8_t *ip, void *data); // forwared declaration

Config config = {
	.version = {
		//.all = VERSION;
		.part = {
			.revision = REVISION,
			.major = VERSION_MAJOR,
			.minor = VERSION_MINOR,
			.patch = VERSION_PATCH
		}
	},

	.name = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a', '\0'},

	.comm = {
		.custom_mac = 0,
		.mac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.ip = {192, 168, 1, 177},
		.gateway = {192, 168, 1, 0},
		.subnet = {255, 255, 255, 0},
	},

	.tuio2 = {
		.enabled = 0,
		.long_header = 0
	},

	.tuio1 = {
		.enabled = 0,
		.custom_profile = 0
	},

	.dump = {
		.enabled = 0
	},

	.scsynth = {
		.enabled = 0
	},

	.rtpmidi = {
		.enabled = 0,
	},

	.oscmidi = {
		.enabled = 0,
		.offset = 23.5,
		.range = 48.0,
		.mul = 170.65, // 0x1fff / 48
		.effect = VOLUME
	},

	.dummy = {
		.enabled = 0,
	},
	
	.output = {
		.socket = {
			.sock = 1,
			.enabled = 1,
			.cb = output_enable,
			.port = {3333, 3333},
			.ip = IP_BROADCAST
		},
		.offset = 0.001ULLK // := 1ms offset
	},

	.config = {
		.rate = 10, // rate in Hz
		.socket = {
			.sock = 2,
			.enabled = 1,
			.cb = config_enable,
			.port = {4444, 4444},
			.ip = IP_BROADCAST
		}
	},

	.sntp = {
		.tau = 4, // delay between SNTP requests in seconds
		.socket = {
			.sock = 3,
			.enabled = 0,
			.cb = sntp_enable,
			.port = {123, 123},
			.ip = IP_BROADCAST
		}
	},

	.debug = {
		.socket = {
			.sock = 4,
			.enabled = 1,
			.cb = debug_enable,
			.port = {6666, 6666},
			.ip = IP_BROADCAST
		}
	},

	.ipv4ll = {
		.enabled = 0
	},

	.mdns = {
		.socket = {
			.sock = 5,
			.enabled = 1,
			.cb = mdns_enable,
			.port = {5353, 5353}, // mDNS multicast port
			.ip = {224, 0, 0, 251} // mDNS multicast group
		}
	},

	.dhcpc = {
		.socket = {
			.sock = 6,
			.enabled = 0,
			.cb = dhcpc_enable,
			.port = {68, 67}, // BOOTPclient, BOOTPserver
			.ip = IP_BROADCAST
		}
	},

	.movingaverage = {
		.enabled = 1,
		.bitshift = 3 // moving average over 8(2³) samples
	},

	.interpolation = {
		//.order = 0, // use no interpolation at all
		//.order = 1, // use linear interpolation
		.order = 2, // use quadratic, aka parabolic interpolation
		//.order = 3, // use cubic interpolation
	},

	.rate = 2000, // update rate in Hz
};

uint_fast8_t
version_match ()
{
	struct {
		uint8_t revision;	// board layout revision
		uint8_t major;		// major version
		uint8_t minor;		// minor version
		uint8_t patch;		// patch level
	} part;
	eeprom_bulk_read (eeprom_24LC64, EEPROM_CONFIG_OFFSET, (uint8_t *)&part, sizeof(part));

	// check whether EEPROM and FLASH version numbers match
	// board revision is excluded from the check
	return (part.major == config.version.part.major)
		&& (part.minor == config.version.part.minor)
		&& (part.patch == config.version.part.patch);
}

uint_fast8_t
config_load ()
{
	if (version_match ())
		eeprom_bulk_read (eeprom_24LC64, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
		config_save ();

	// update callbacks
	config.output.socket.cb = output_enable;
	config.config.socket.cb = config_enable;
	config.sntp.socket.cb = sntp_enable;
	config.debug.socket.cb = debug_enable;
	config.mdns.socket.cb = mdns_enable;
	config.dhcpc.socket.cb = dhcpc_enable;

	return 1;
}

uint_fast8_t
config_save ()
{
	eeprom_bulk_write (eeprom_24LC64, EEPROM_CONFIG_OFFSET, (uint8_t *)&config, sizeof (config));

	return 1;
}

uint_fast8_t
groups_load ()
{
	if (version_match ())
		eeprom_bulk_read (eeprom_24LC64, EEPROM_GROUP_OFFSET, (uint8_t *)cmc_groups, GROUP_MAX*sizeof(CMC_Group));
	else
		groups_save ();

	return 1;
}

uint_fast8_t
groups_save ()
{
	eeprom_bulk_write (eeprom_24LC64, EEPROM_GROUP_OFFSET, (uint8_t *)cmc_groups, GROUP_MAX*sizeof(CMC_Group));

	return 1;
}

static uint_fast8_t
_check_bool (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args, uint8_t *boolean)
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
				*boolean = args[1].i != 0 ? 1 : 0;
				break;
			case nOSC_FLOAT:
				*boolean = args[1].f != 0.f ? 1 : 0;
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

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_check_range8 (uint8_t *val, uint8_t min, uint8_t max, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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
		if ( (arg >= min) && (arg <= max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("isiii", id, "out of range", arg, min, max);
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_check_range16 (uint16_t *val, uint16_t min, uint16_t max, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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
		if ( (arg >= min) && (arg <= max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("isiii", id, "out of range", arg, min, max);
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_check_rangefloat (float *val, float min, float max, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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
		if ( (arg >= min) && (arg <= max) )
		{
			*val = arg;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("isfff", id, "out of range", arg, min, max);
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_version (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	sprintf (string_buf, "%i.%i.%i Rev%i",
		config.version.part.major,
		config.version.part.minor,
		config.version.part.patch,
		config.version.part.revision);
	size = CONFIG_SUCCESS ("is", id, string_buf);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_name (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_sensors (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_SUCCESS("ii", id, SENSOR_N);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_load (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (config_load ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "loading of configuration from EEPROM failed");

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_save (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (config_save ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "saving configuration to EEPROM failed");

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_comm_mac (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		mac2str (config.comm.mac, string_buf);
		size = CONFIG_SUCCESS ("is", id, string_buf);
	}
	else
	{
		if (str2mac (args[1].s, config.comm.mac))
		{
			wiz_mac_set (config.comm.mac);
			config.comm.custom_mac = 1;
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, "wrong format");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_comm_ip (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	uint32_t *ip_ptr = (uint32_t *)config.comm.ip;
	uint32_t *subnet_ptr = (uint32_t *)config.comm.subnet;
	uint32_t *gateway_ptr = (uint32_t *)config.comm.gateway;

	if (argc == 1) // query
	{
		uint8_t mask = subnet_to_cidr(config.comm.subnet);
		ip2strCIDR (config.comm.ip, mask, string_buf);
		size = CONFIG_SUCCESS ("is", id, string_buf);
	}
	else
	{
		if(argc == 3)
		{
			if(str2ip(args[2].s, config.comm.gateway))
				wiz_gateway_set(config.comm.gateway);
			else // return
			{
				CONFIG_FAIL("is", id, "gateway invalid, format: x.x.x.x");
				udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
				return 1;
			}
		}

		uint8_t mask;
		if(str2ipCIDR(args[1].s, config.comm.ip, &mask))
		{
			wiz_ip_set (config.comm.ip);

			cidr_to_subnet(config.comm.subnet, mask);
			wiz_subnet_set (config.comm.subnet);

			if(argc == 2) // no custom gateway was given
			{
				*gateway_ptr = (*ip_ptr) & (*subnet_ptr); // default gateway = (ip & subnet)
				wiz_gateway_set (config.comm.gateway);
			}
			size = CONFIG_SUCCESS ("i", id);

			uint8_t brd [4];
			broadcast_address(brd, config.comm.ip, config.comm.subnet);
			_host_address_dns_cb(brd, NULL); //TODO maybe we want to reset all sockets here instead of changing to broadcast?

			if(config.mdns.socket.enabled)
				mdns_announce(); // announce new IP
		}
		else
			size = CONFIG_FAIL ("is", id, "ip invalid, format: x.x.x.x/x");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

/*
static uint_fast8_t
_comm_gateway (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		ip2str (config.comm.gateway, string_buf);
		size = CONFIG_SUCCESS ("is", id, string_buf);
	}
	else
	{
		if (str2ip (args[1].s, config.comm.gateway))
		{
			wiz_gateway_set (config.comm.gateway);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
			size = CONFIG_FAIL ("is", id, "invalid format");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}
*/

static uint_fast8_t
_socket_enabled (Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
		size = CONFIG_SUCCESS ("ii", id, socket->enabled ? 1 : 0);
	else
	{
		if(socket->cb (args[1].i))
			size = CONFIG_SUCCESS ("i", id);
		else
			size = CONFIG_FAIL("is", "socket could not be enabled");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_output_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _socket_enabled (&config.output.socket, path, fmt, argc, args);
}

static uint_fast8_t
_output_reset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	config.dump.enabled = 0;
	config.tuio2.enabled = 0;
	config.tuio1.enabled = 0;
	config.scsynth.enabled = 0;
	config.oscmidi.enabled = 0;
	config.dummy.enabled = 0;
	config.rtpmidi.enabled = 0;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _socket_enabled (&config.config.socket, path, fmt, argc, args);
}

static uint_fast8_t
_sntp_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _socket_enabled (&config.sntp.socket, path, fmt, argc, args);
}

static uint_fast8_t
_debug_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _socket_enabled (&config.debug.socket, path, fmt, argc, args);
}

static uint_fast8_t
_dhcpc_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	// needs a config save and reboot to take action
	return _check_bool (path, fmt, argc, args, &config.dhcpc.socket.enabled);
}

static uint_fast8_t
_mdns_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _socket_enabled (&config.mdns.socket, path, fmt, argc, args);
}

static void
_address_dns_cb (uint8_t *ip, void *data)
{
	uint16_t size;

	Socket_Config *socket = data;

	memcpy (socket->ip, ip, 4);
	socket->cb (socket->enabled);

	ip2str (ip, string_buf);
	DEBUG("ss", "_address_dns_cb", string_buf);

	//size = CONFIG_SUCCESS ("s", string_buf);
	//udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
}

static uint_fast8_t
_address (Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	char *hostname = args[1].s;

	if (argc == 1) // query
	{
		addr2str (socket->ip, socket->port[DST_PORT], string_buf);
		size = CONFIG_SUCCESS ("is", id, string_buf);
	}
	else
	{
		uint16_t port;
		uint8_t ip[4];
		if (str2addr(hostname, ip, &port))
		{
			socket->port[DST_PORT] = port;
			_address_dns_cb(ip, socket);
			size = CONFIG_SUCCESS ("i", id);
		}
		else
		{
			if (strstr (hostname, local_str)) // in the .local domain ?
			{
				char *port_str = strchr (hostname, ':');
				*port_str = 0x0; // end name here
				port = atoi (port_str+1);
				socket->port[DST_PORT] = port; //TODO only do this when resolve successful

				if(mdns_resolve (hostname, _address_dns_cb, socket))
					size = CONFIG_SUCCESS ("i", id);
				else
					size = CONFIG_FAIL ("is", id, "there is a mDNS request already ongoing");
			}
			else
			{
				size = CONFIG_FAIL ("is", id, "can only resolve raw IP and mDNS addresses");
			}
		}
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_output_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _address (&config.output.socket, path, fmt, argc, args);
}

static uint_fast8_t
_config_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _address (&config.config.socket, path, fmt, argc, args);
}

static uint_fast8_t
_sntp_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _address (&config.sntp.socket, path, fmt, argc, args);
}

static uint_fast8_t
_debug_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _address (&config.debug.socket, path, fmt, argc, args);
}

static void
_host_address_dns_cb (uint8_t *ip, void *data)
{
	uint16_t size;

	memcpy (config.output.socket.ip, ip, 4);
	memcpy (config.config.socket.ip, ip, 4);
	memcpy (config.sntp.socket.ip, ip, 4);
	memcpy (config.debug.socket.ip, ip, 4);

	output_enable (config.output.socket.enabled);
	config_enable (config.config.socket.enabled);
	sntp_enable (config.sntp.socket.enabled);
	debug_enable (config.debug.socket.enabled);

	ip2str (ip, string_buf);
	DEBUG("ss", "_host_address_dns_cb", string_buf);

	//size = CONFIG_SUCCESS ("s", string_buf);
	//udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
}

static uint_fast8_t
_host_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	char *hostname = args[1].s;

	uint8_t ip[4];
	if (str2ip (hostname, ip)) // an IPv4 was given in string format
	{
		_host_address_dns_cb (ip, NULL);
		size = CONFIG_SUCCESS ("i", id);
	}
	else
	{
		if (strstr (hostname, local_str)) // resolve via mDNS
		{
			if(mdns_resolve (hostname, _host_address_dns_cb, NULL))
				size = CONFIG_SUCCESS ("i", id);
			else
				size = CONFIG_FAIL ("is", id, "there is a mDNS request already ongoing");
		}
		else // resolve via unicast DNS
		{
			size = CONFIG_FAIL ("is", "can only resolve raw IP and mDNS addresses");
		}
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_ipv4ll_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	// needs a config save and reboot to take action
	return _check_bool (path, fmt, argc, args, &config.ipv4ll.enabled);
}

static uint_fast8_t
_sntp_tau (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.sntp.tau, 1, 10, path, fmt, argc, args);
}

static uint_fast8_t
_sntp_offset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	float offset = clock_offset;
	size = CONFIG_SUCCESS ("if", id, offset);

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_sntp_roundtrip (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	float trip = roundtrip_delay;
	size = CONFIG_SUCCESS ("if", id, trip);

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_tuio2_long_header (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.tuio2.long_header);
}

static uint_fast8_t
_tuio2_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.tuio2.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_tuio1_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.tuio1.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_tuio1_custom_profile (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_bool(path, fmt, argc, args, &config.tuio1.custom_profile);
}

static uint_fast8_t
_dump_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.dump.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_scsynth_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.scsynth.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_scsynth_group (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint16_t gid = args[1].i;

	char *name;
	uint16_t sid;
	uint16_t group;
	uint16_t out;
	uint8_t arg;
	uint8_t alloc;
	uint8_t gate;
	uint8_t add_action;
	uint8_t is_group;

	if(argc == 2)
	{
		if(scsynth_group_get(gid, &name, &sid, &group, &out, &arg, &alloc, &gate, &add_action, &is_group))
			size = CONFIG_SUCCESS ("isiiiiiiii", id, name, sid, group, out, arg, alloc, gate, add_action, is_group);
		else
			size = CONFIG_FAIL ("is", id, "argument out of bounds");
	}
	else // argc == 11
	{
		name = args[2].s;
		sid = args[3].i;
		group = args[4].i;
		out = args[5].i;
		arg = args[6].i;
		alloc = args[7].i;
		gate = args[8].i;
		add_action = args[9].i;
		is_group = args[10].i;

		if(scsynth_group_set(gid, name, sid, group, out, arg, alloc, gate, add_action, is_group))
			size = CONFIG_SUCCESS ("i", id);
		else
			size = CONFIG_FAIL ("is", id, "argument out of bounds");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_rtpmidi_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.rtpmidi.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_oscmidi_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.oscmidi.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_oscmidi_offset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_rangefloat (&config.oscmidi.offset, 0.0, 127.0, path, fmt, argc, args);
}

static uint_fast8_t
_oscmidi_range (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res;
	res = _check_rangefloat (&config.oscmidi.range, 0.0, 127.0, path, fmt, argc, args);
	if(res)
		config.oscmidi.mul = (float)0x1fff / config.oscmidi.range;
	return res;
}

static uint_fast8_t
_oscmidi_effect (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.oscmidi.effect, 0, 0x7f, path, fmt, argc, args);
}

static uint_fast8_t
_dummy_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = _check_bool (path, fmt, argc, args, &config.dummy.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_output_offset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_rate (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
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

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_reset_soft (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// reset factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_SOFT);
	bkp_disable_writes();

	nvic_sys_reset ();

	return 1;
}

static uint_fast8_t
_reset_hard (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// set factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_HARD);
	bkp_disable_writes();

	nvic_sys_reset ();

	return 1;
}

static uint_fast8_t
_reset_flash (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// set bootloader flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_SYSTEM_FLASH);
	bkp_disable_writes();

	nvic_sys_reset ();

	return 1;
}

static uint_fast8_t
_movingaverage_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_bool (path, fmt, argc, args, &config.movingaverage.enabled);
}

static uint_fast8_t
_movingaverage_samples (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (argc == 1) // query
	{
		size = CONFIG_SUCCESS ("ii", id, 1U << config.movingaverage.bitshift);
	}
	else
		switch (args[1].i)
		{
			case 2:
				config.movingaverage.bitshift = 1;
				size = CONFIG_SUCCESS ("i", id);
				break;
			case 4:
				config.movingaverage.bitshift = 2;
				size = CONFIG_SUCCESS ("i", id);
				break;
			case 8:
				config.movingaverage.bitshift = 3;
				size = CONFIG_SUCCESS ("i", id);
				break;
			default:
				size = CONFIG_FAIL ("is", id, "valid sample windows are 2, 4 and 8");
		}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_interpolation_order (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return _check_range8 (&config.interpolation.order, 0, 4, path, fmt, argc, args);
}

static uint_fast8_t
_group_clear (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	cmc_group_clear ();

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint16_t gid = args[1].i;

	uint16_t pid;
	float x0;
	float x1;
	uint_fast8_t scale;

	if(argc == 2) // request group info
	{

		if (cmc_group_get (gid, &pid, &x0, &x1, &scale))
			size = CONFIG_SUCCESS ("iiffi", id, pid, x0, x1, scale);
		else
			size = CONFIG_FAIL ("is", id, "group not found");
	}
	else // set group info
	{
		pid = args[2].i;
		x0 = args[3].f;
		x1 = args[4].f;
		scale = args[5].i;

		if(cmc_group_set (gid, pid, x0, x1, scale))
			size = CONFIG_SUCCESS ("i", id);
		else
			size = CONFIG_FAIL ("is", id, "group not found or wrong parameter range");
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_load (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (groups_load ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "groups could not be loaded from EEPROM");
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_save (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	if (groups_save ())
		size = CONFIG_SUCCESS ("i", id);
	else
		size = CONFIG_FAIL ("is", id, "groups could not be saved to EEPROM");
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_start (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	range_init ();

	// enable calibration
	zeroing = 1;
	calibrating = 1;

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_zero (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// update new range
	zeroing = 0;
	range_update_quiescent ();

	uint_fast8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/range/qui", "iii", i, range.qui[i], range.qui[i]-0x7ff);
		udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
	}

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_min (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// update new range
	uint_fast8_t si = range_update_b0 ();

	uint_fast8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/range/thresh", "ii", i, range.thresh[i]);
		udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
	}

	size = CONFIG_SUCCESS ("ii", id, si);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_mid (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	float y = args[1].f;

	if( (y >= 0.f) && (y <= 1.f) )
	{

		// update mid range
		range_update_b1 (y);

		size = CONFIG_SUCCESS ("i", id);
	}
	else
		size = CONFIG_FAIL ("is", id, "argument out of bounds");

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_max (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// update max range
	range_update_b2 ();

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_end (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	float y = args[1].f;

	// update max range
	range_update_b3 (y);

	// end calibration procedure
	calibrating = 0;

	// debug output
	uint_fast8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/range/U", "if", i, range.U[i]);
		udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
	}

	// output minimal offset
	size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/range/W", "f", range.W);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// output curve parameters
	size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/range/C", "fff", range.C[0], range.C[1], range.C[2]);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

// get calibration data per sensor
static uint_fast8_t
_calibration_sensor (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	int32_t n = args[1].i;
	if (n < SENSOR_N)
		size = CONFIG_SUCCESS ("iiif", id, range.qui[n], range.thresh[n], range.U[n]);
	else
		size = CONFIG_FAIL ("is", id, "requested sensor is out of bounds");
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_offset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_SUCCESS ("if", id, range.W);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_curve (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_SUCCESS ("ifff", id, range.C[0], range.C[1], range.C[2]);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_save (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint_fast8_t pos = args[1].i; // use given calibration

	if (pos < EEPROM_RANGE_MAX)
	{
		range_save (pos);
		size = CONFIG_SUCCESS ("i", id);
	}
	else
		size = CONFIG_FAIL ("is", id, "slot out of range");

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_load (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;
	uint_fast8_t pos = args[1].i;

	if (pos < EEPROM_RANGE_MAX)
	{
		range_load (pos);
		size = CONFIG_SUCCESS ("i", id);
	}
	else
		size = CONFIG_FAIL ("is", id, "slot out of range");

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_calibration_reset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	// reset calibration range
	range_reset ();

	size = CONFIG_SUCCESS ("i", id);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_uid (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	uid_str (string_buf);
	size = CONFIG_SUCCESS ("is", id, string_buf);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_ping (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	Socket_Config *socket = &config.config.socket;

	// set remote to broadcast address
	udp_set_remote (socket->sock, (uint8_t *)wiz_broadcast_ip, socket->port[DST_PORT]);

	//TODO what should we send here, and how?
	_uid(path, fmt, argc, args);
	_comm_mac(path, fmt, argc, args);
	_comm_ip(path, fmt, argc, args);

	// reset remote to configured address
	udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);

	return 1;
}

static uint_fast8_t
_non (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	size = CONFIG_FAIL ("isss", id, "unknown method for path or format", path, fmt);
	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

const nOSC_Method config_serv [] = {
	{"/chimaera/version", "i", _version},
	{"/chimaera/name", "i*", _name},
	{"/chimaera/uid", "i", _uid},
	{"/chimaera/sensors", "i", _sensors},
	{"/chimaera/rate", "i*", _rate},

	{"/chimaera/reset/soft", "i", _reset_soft},
	{"/chimaera/reset/hard", "i", _reset_hard},
	{"/chimaera/reset/flash", "i", _reset_flash},

	{"/chimaera/config/load", "i", _config_load},
	{"/chimaera/config/save", "i", _config_save},
	{"/chimaera/config/enabled", "i*", _config_enabled},
	{"/chimaera/config/address", "i*", _config_address},

	{"/chimaera/comm/mac", "i*", _comm_mac},
	{"/chimaera/comm/ip", "i*", _comm_ip},
	//{"/chimaera/comm/gateway", "i*", _comm_gateway},

	{"/chimaera/output/enabled", "i*", _output_enabled},
	{"/chimaera/output/address", "i*", _output_address},
	{"/chimaera/output/offset", "i*", _output_offset},
	{"/chimaera/output/reset", "i", _output_reset},

	{"/chimaera/dump/enabled", "i*", _dump_enabled},

	{"/chimaera/tuio2/enabled", "i*", _tuio2_enabled},
	{"/chimaera/tuio2/long_header", "i*", _tuio2_long_header},

	{"/chimaera/tuio1/enabled", "i*", _tuio1_enabled},
	{"/chimaera/tuio1/custom_profile", "i*", _tuio1_custom_profile},

	{"/chimaera/scsynth/enabled", "i*", _scsynth_enabled},
	{"/chimaera/scsynth/group", "ii", _scsynth_group},
	{"/chimaera/scsynth/group", "iisiiiiiiii", _scsynth_group},

	{"/chimaera/oscmidi/enabled", "i*", _oscmidi_enabled},
	{"/chimaera/oscmidi/offset", "i*", _oscmidi_offset},
	{"/chimaera/oscmidi/range", "i*", _oscmidi_range},
	{"/chimaera/oscmidi/effect", "i*", _oscmidi_effect},

	{"/chimaera/dummy/enabled", "i*", _dummy_enabled},

	{"/chimaera/rtpmidi/enabled", "i*", _rtpmidi_enabled},

	{"/chimaera/sntp/enabled", "i*", _sntp_enabled},
	{"/chimaera/sntp/address", "i*", _sntp_address},
	{"/chimaera/sntp/tau", "i*", _sntp_tau},
	{"/chimaera/sntp/offset", "i", _sntp_offset},
	{"/chimaera/sntp/roundtrip", "i", _sntp_roundtrip},

	{"/chimaera/debug/enabled", "i*", _debug_enabled},
	{"/chimaera/debug/address", "i*", _debug_address},

	{"/chimaera/host/address", "is", _host_address},

	{"/chimaera/dhcpc/enabled", "i*", _dhcpc_enabled},

	{"/chimaera/mdns/enabled", "i*", _mdns_enabled},

	{"/chimaera/ipv4ll/enabled", "i*", _ipv4ll_enabled},

	{"/chimaera/movingaverage/enabled", "i*", _movingaverage_enabled},
	{"/chimaera/movingaverage/samples", "i*", _movingaverage_samples},

	{"/chimaera/interpolation/order", "i*", _interpolation_order},

	{"/chimaera/group/load", "i", _group_load},
	{"/chimaera/group/save", "i", _group_save},
	{"/chimaera/group/clear", "i", _group_clear},
	{"/chimaera/group", "ii", _group},
	{"/chimaera/group", "iiiffi", _group},

	{"/chimaera/calibration/load", "i", _calibration_load},
	{"/chimaera/calibration/load", "ii", _calibration_load},
	{"/chimaera/calibration/save", "i", _calibration_save},
	{"/chimaera/calibration/save", "ii", _calibration_save},
	{"/chimaera/calibration/reset", "i", _calibration_reset},

	{"/chimaera/calibration/start", "i", _calibration_start},
	{"/chimaera/calibration/zero", "i", _calibration_zero},
	{"/chimaera/calibration/min", "i", _calibration_min},
	{"/chimaera/calibration/mid", "if", _calibration_mid},
	{"/chimaera/calibration/max", "i", _calibration_max},
	{"/chimaera/calibration/end", "if", _calibration_end},

	{"/chimaera/calibration/sensor", "ii", _calibration_sensor},
	{"/chimaera/calibration/offset", "i", _calibration_offset},
	{"/chimaera/calibration/curve", "i", _calibration_curve},

	{"/chimaera/ping", "i", _ping},

	{NULL, NULL, _non}, // if nothing else matches, we give back an error saying so

	{NULL, NULL, NULL} // terminator
};
