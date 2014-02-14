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
#include <midi.h>
#include <calibration.h>
#include <sntp.h>
#include <engines.h>
#include <ipv4ll.h>
#include <dhcpc.h>
#include <mdns-sd.h>
#include <debug.h>
#include <sensors.h>

static char string_buf [64];
const char *success_str = "/success";
const char *fail_str = "/fail";
static const char *local_str = ".local";

#define IP_BROADCAST {255, 255, 255, 255}

static void _host_address_dns_cb(uint8_t *ip, void *data); // forwared declaration

static const Socket_Enable_Cb socket_callbacks [WIZ_MAX_SOCK_NUM] = {
	[SOCK_ARP]		= NULL,
	[SOCK_OUTPUT]	= output_enable,
	[SOCK_CONFIG]	= config_enable,
	[SOCK_SNTP]		= sntp_enable,
	[SOCK_DEBUG]	= debug_enable,
	[SOCK_MDNS]		= mdns_enable,
	[SOCK_DHCPC]	= dhcpc_enable,
	[SOCK_RTP]		= NULL // not yet used
};

Config config = {
	.version = {
		.revision = REVISION,
		.major = VERSION_MAJOR,
		.minor = VERSION_MINOR,
		.patch = VERSION_PATCH
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
		.offset = MIDI_BOT,
		.range = MIDI_RANGE,
		.mul = 0x2000 / MIDI_RANGE,
		.effect = MIDI_CONTROLLER_VOLUME
	},

	.dummy = {
		.enabled = 0,
	},
	
	.output = {
		.socket = {
			.sock = SOCK_OUTPUT,
			.enabled = 1,
			.port = {3333, 3333},
			.ip = IP_BROADCAST
		},
		.offset = 0.001ULLK // := 1ms offset
	},

	.config = {
		.rate = 10, // rate in Hz
		.socket = {
			.sock = SOCK_CONFIG,
			.enabled = 1,
			.port = {4444, 4444},
			.ip = IP_BROADCAST
		}
	},

	.sntp = {
		.tau = 4, // delay between SNTP requests in seconds
		.socket = {
			.sock = SOCK_SNTP,
			.enabled = 0,
			.port = {123, 123},
			.ip = IP_BROADCAST
		}
	},

	.debug = {
		.socket = {
			.sock = SOCK_DEBUG,
			.enabled = 1,
			.port = {6666, 6666},
			.ip = IP_BROADCAST
		}
	},

	.ipv4ll = {
		.enabled = 0
	},

	.mdns = {
		.socket = {
			.sock = SOCK_MDNS,
			.enabled = 1,
			.port = {5353, 5353}, // mDNS multicast port
			.ip = {224, 0, 0, 251} // mDNS multicast group
		}
	},

	.dhcpc = {
		.socket = {
			.sock = SOCK_DHCPC,
			.enabled = 0,
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
version_match()
{
	Firmware_Version version;
	eeprom_bulk_read(eeprom_24LC64, EEPROM_CONFIG_OFFSET,(uint8_t *)&version, sizeof(Firmware_Version));

	// check whether EEPROM and FLASH version numbers match
	// board revision is excluded from the check
	return(version.major == config.version.major)
		&& (version.minor == config.version.minor)
		&& (version.patch == config.version.patch);
}

uint_fast8_t
config_load()
{
	if(version_match())
		eeprom_bulk_read(eeprom_24LC64, EEPROM_CONFIG_OFFSET,(uint8_t *)&config, sizeof(config));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
		config_save();

	return 1;
}

uint_fast8_t
config_save()
{
	eeprom_bulk_write(eeprom_24LC64, EEPROM_CONFIG_OFFSET,(uint8_t *)&config, sizeof(config));

	return 1;
}

uint_fast8_t
groups_load()
{
	if(version_match())
		eeprom_bulk_read(eeprom_24LC64, EEPROM_GROUP_OFFSET,(uint8_t *)cmc_groups, GROUP_MAX*sizeof(CMC_Group));
	else
		groups_save();

	return 1;
}

uint_fast8_t
groups_save()
{
	eeprom_bulk_write(eeprom_24LC64, EEPROM_GROUP_OFFSET,(uint8_t *)cmc_groups, GROUP_MAX*sizeof(CMC_Group));

	return 1;
}

uint_fast8_t
config_check_bool(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args, uint8_t *boolean)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *boolean ? 1 : 0);
	else
	{
		*boolean = args[1].i != 0 ? 1 : 0;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

uint_fast8_t
config_check_uint8(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args, uint8_t *val)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *val);
	else
	{
		*val = args[1].i;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

uint_fast8_t
config_check_float(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args, float *val)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isf", uuid, path, *val);
	else
	{
		*val = args[1].f;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_info_version(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	sprintf(string_buf, "%i.%i.%i-%i",
		config.version.major,
		config.version.minor,
		config.version.patch,
		config.version.revision);
	size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_info_uid(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	uid_str(string_buf);
	size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_info_name(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, config.name);
	else
	{
		if(strlen(args[1].s) < NAME_LENGTH)
		{
			strcpy(config.name, args[1].s);
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "name is too long");
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_load(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(config_load())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "loading of configuration from EEPROM failed");

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_save(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(config_save())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "saving configuration to EEPROM failed");

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_comm_mac(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
	{
		mac2str(config.comm.mac, string_buf);
		size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	}
	else
	{
		if(str2mac(args[1].s, config.comm.mac))
		{
			wiz_mac_set(config.comm.mac);
			config.comm.custom_mac = 1;
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "wrong format");
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_comm_ip(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	uint32_t *ip_ptr =(uint32_t *)config.comm.ip;
	uint32_t *subnet_ptr =(uint32_t *)config.comm.subnet;
	uint32_t *gateway_ptr =(uint32_t *)config.comm.gateway;

	if(argc == 1) // query
	{
		uint8_t mask = subnet_to_cidr(config.comm.subnet);
		ip2strCIDR(config.comm.ip, mask, string_buf);
		size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	}
	else
	{
		if(argc == 3)
		{
			if(str2ip(args[2].s, config.comm.gateway))
				wiz_gateway_set(config.comm.gateway);
			else // return
			{
				CONFIG_FAIL("iss", uuid, path, "gateway invalid, format: x.x.x.x");
				udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
				return 1;
			}
		}

		uint8_t mask;
		if(str2ipCIDR(args[1].s, config.comm.ip, &mask))
		{
			wiz_ip_set(config.comm.ip);

			cidr_to_subnet(config.comm.subnet, mask);
			wiz_subnet_set(config.comm.subnet);

			if(argc == 2) // no custom gateway was given
			{
				*gateway_ptr =(*ip_ptr) & (*subnet_ptr); // default gateway =(ip & subnet)
				wiz_gateway_set(config.comm.gateway);
			}
			size = CONFIG_SUCCESS("is", uuid, path);

			uint8_t brd [4];
			broadcast_address(brd, config.comm.ip, config.comm.subnet);
			_host_address_dns_cb(brd, NULL); //TODO maybe we want to reset all sockets here instead of changing to broadcast?

			if(config.mdns.socket.enabled)
				mdns_announce(); // announce new IP
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "ip invalid, format: x.x.x.x/x");
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

uint_fast8_t
config_socket_enabled(Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, socket->enabled ? 1 : 0);
	else
	{
		Socket_Enable_Cb cb = socket_callbacks[socket->sock];
		if(cb(args[1].i))
			size = CONFIG_SUCCESS("is", uuid, path);
		else
			size = CONFIG_FAIL("iss", uuid, path, "socket could not be enabled");
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_output_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_socket_enabled(&config.output.socket, path, fmt, argc, args);
}

static uint_fast8_t
_output_reset(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	config.dump.enabled = 0;
	config.tuio2.enabled = 0;
	config.tuio1.enabled = 0;
	config.scsynth.enabled = 0;
	config.oscmidi.enabled = 0;
	config.dummy.enabled = 0;
	config.rtpmidi.enabled = 0;

	size = CONFIG_SUCCESS("is", uuid, path);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_config_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_socket_enabled(&config.config.socket, path, fmt, argc, args);
}

typedef struct _Address_Cb Address_Cb;

struct _Address_Cb {
	int32_t uuid;
	char path [32];
	Socket_Config *socket;
	uint16_t port;
};

static void
_address_dns_cb(uint8_t *ip, void *data)
{
	uint16_t size;

	Address_Cb *address_cb = data;
	Socket_Config *socket = address_cb->socket;

	if(ip)
	{
		socket->port[DST_PORT] = address_cb->port;
		memcpy(socket->ip, ip, 4);
		Socket_Enable_Cb cb = socket_callbacks[socket->sock];
		cb(socket->enabled);

		ip2str(ip, string_buf);
		DEBUG("ss", "_address_dns_cb", string_buf);
		
		size = CONFIG_SUCCESS("is", address_cb->uuid, address_cb->path);
	}
	else // timeout occured
		size = CONFIG_FAIL("iss", address_cb->uuid, address_cb->path, "mDNS resolve timed out");
	
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
}

uint_fast8_t
config_address(Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	static Address_Cb address_cb;
	uint16_t size = 0;
	int32_t uuid = args[0].i;
	char *hostname = args[1].s;

	if(argc == 1) // query
	{
		addr2str(socket->ip, socket->port[DST_PORT], string_buf);
		size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	}
	else
	{
		uint16_t port;
		uint8_t ip[4];

		address_cb.uuid = uuid;
		strcpy(address_cb.path, path); //TODO check length
		address_cb.socket = socket;

		if(str2addr(hostname, ip, &port))
		{
			address_cb.port = port;
			_address_dns_cb(ip, &address_cb);
		}
		else
		{
			if(strstr(hostname, local_str)) // in the .local domain ?
			{
				char *port_str = strchr(hostname, ':');
				*port_str = 0x0; // end name here
				port = atoi(port_str+1);
				address_cb.port = port;

				if(!mdns_resolve(hostname, _address_dns_cb, &address_cb))
					size = CONFIG_FAIL("iss", uuid, path, "there is a mDNS request already ongoing");
			}
			else
				size = CONFIG_FAIL("iss", uuid, path, "can only resolve raw IP and mDNS addresses");
		}
	}

	if(size > 0)
		udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_output_address(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_address(&config.output.socket, path, fmt, argc, args);
}

static uint_fast8_t
_config_address(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_address(&config.config.socket, path, fmt, argc, args);
}

static void
_host_address_dns_cb(uint8_t *ip, void *data)
{
	uint16_t size;

	Address_Cb *address_cb = data;

	if(ip)
	{
		memcpy(config.output.socket.ip, ip, 4);
		memcpy(config.config.socket.ip, ip, 4);
		memcpy(config.sntp.socket.ip, ip, 4);
		memcpy(config.debug.socket.ip, ip, 4);

		output_enable(config.output.socket.enabled);
		config_enable(config.config.socket.enabled);
		sntp_enable(config.sntp.socket.enabled);
		debug_enable(config.debug.socket.enabled);

		ip2str(ip, string_buf);
		DEBUG("ss", "_host_address_dns_cb", string_buf);
		
		size = CONFIG_SUCCESS("is", address_cb->uuid, address_cb->path);
	}
	else // timeout occured
		size = CONFIG_FAIL("iss", address_cb->uuid, address_cb->path, "mDNS resolve timed out");
	
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);
}

static uint_fast8_t
_comm_address(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	static Address_Cb address_cb;
	uint16_t size = 0;
	int32_t uuid = args[0].i;
	char *hostname = args[1].s;

	address_cb.uuid = uuid;
	strcpy(address_cb.path, path); //TODO check length

	uint8_t ip[4];
	if(str2ip(hostname, ip)) // an IPv4 was given in string format
		_host_address_dns_cb(ip, &address_cb);
	else
	{
		if(strstr(hostname, local_str)) // resolve via mDNS
		{
			if(!mdns_resolve(hostname, _host_address_dns_cb, &address_cb))
				size = CONFIG_FAIL("iss", uuid, path, "there is a mDNS request already ongoing");
		}
		else // resolve via unicast DNS
			size = CONFIG_FAIL("iss", uuid, path, "can only resolve raw IP and mDNS addresses");
	}

	if(size > 0)
		udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_output_offset(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
	{
		float f = config.output.offset;
		size = CONFIG_SUCCESS("isf", uuid, path, f); // output timestamp, double, float?
	}
	else
	{
		config.output.offset = args[1].f;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_reset_soft(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS("is", uuid, path);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// reset factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_SOFT);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t
_reset_hard(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS("is", uuid, path);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// set factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_HARD);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t
_reset_flash(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	int32_t sec;

	size = CONFIG_SUCCESS("is", uuid, path);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	// set bootloader flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_SYSTEM_FLASH);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t
_ping(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t id = args[0].i;

	Socket_Config *socket = &config.config.socket;

	// set remote to broadcast address
	udp_set_remote(socket->sock,(uint8_t *)wiz_broadcast_ip, socket->port[DST_PORT]);

	//TODO what should we send here, and how?
	_info_uid(path, fmt, argc, args);
	_comm_mac(path, fmt, argc, args);
	_comm_ip(path, fmt, argc, args);

	// reset remote to configured address
	udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);

	return 1;
}

static uint_fast8_t _query(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args);

const nOSC_Method config_serv [] = {
	//{"/chimaera/ping", "i", _ping},
	{NULL, NULL, _query},
	{NULL, NULL, NULL} // terminator
};


static const nOSC_Query_Argument comm_mac_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument comm_ip_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument comm_address_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_WX)
};

const nOSC_Query_Item comm_tree [] = {
	nOSC_QUERY_ITEM_METHOD("mac", "hardware address", _comm_mac, comm_mac_args),
	nOSC_QUERY_ITEM_METHOD("ip", "IPv4 address", _comm_ip, comm_ip_args),
	nOSC_QUERY_ITEM_METHOD("address", "remote IPv4 address", _comm_address, comm_address_args),
};

static const nOSC_Query_Argument config_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument config_address_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_RW)
};

const nOSC_Query_Item config_tree [] = {
	nOSC_QUERY_ITEM_METHOD("save", "save to EEPROM", _config_save, NULL),
	nOSC_QUERY_ITEM_METHOD("load", "load from EEPROM", _config_load, NULL),
	nOSC_QUERY_ITEM_METHOD("enabled", "enable/disable socket", _config_enabled, config_enabled_args),
	nOSC_QUERY_ITEM_METHOD("address", "remote address", _config_address, config_address_args),
};

const nOSC_Query_Item reset_tree [] = {
	nOSC_QUERY_ITEM_METHOD("soft", "soft reset", _reset_soft, NULL),
	nOSC_QUERY_ITEM_METHOD("hard", "hard reset", _reset_hard, NULL),
	nOSC_QUERY_ITEM_METHOD("flash", "reset into flash mode", _reset_flash, NULL),
};

static const nOSC_Query_Argument info_version_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_R)
};

static const nOSC_Query_Argument info_uid_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_R)
};

static const nOSC_Query_Argument info_name_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Item info_tree [] = {
	nOSC_QUERY_ITEM_METHOD("version", "firmware version", _info_version, info_version_args),
	nOSC_QUERY_ITEM_METHOD("uid", "universal identifier", _info_uid, info_uid_args),

	nOSC_QUERY_ITEM_METHOD("name", "device name", _info_name, info_name_args),
};

static const nOSC_Query_Argument output_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument output_address_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("host:port", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument output_offset_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("seconds", nOSC_QUERY_MODE_RW, 0.f, INFINITY)
};

static const nOSC_Query_Item engines_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "enable/disable", _output_enabled, output_enabled_args),
	nOSC_QUERY_ITEM_METHOD("address", "remote host", _output_address, output_address_args),
	nOSC_QUERY_ITEM_METHOD("offset", "bundle offset", _output_offset, output_offset_args),
	nOSC_QUERY_ITEM_METHOD("reset", "disable engines", _output_reset, NULL),

	// engines
	nOSC_QUERY_ITEM_NODE("dump/", "Dump output engine", dump_tree),
	nOSC_QUERY_ITEM_NODE("dummy/", "Dummy output engine", dummy_tree),
	nOSC_QUERY_ITEM_NODE("tuio2/", "TUIO 2.0 output engine", tuio2_tree),
	nOSC_QUERY_ITEM_NODE("tuio1/", "TUIO 1.0 output engine", tuio1_tree),
	nOSC_QUERY_ITEM_NODE("scsynth/", "SuperCollider output engine", scsynth_tree),
	nOSC_QUERY_ITEM_NODE("oscmidi/", "OSC MIDI output engine", oscmidi_tree),
	nOSC_QUERY_ITEM_NODE("rtpmidi/", "RTP MIDI output engine", rtpmidi_tree),
};

static const nOSC_Query_Item root_tree [] = {
	nOSC_QUERY_ITEM_NODE("info/", "Information", info_tree),
	nOSC_QUERY_ITEM_NODE("comm/", "Communitation", comm_tree),
	nOSC_QUERY_ITEM_NODE("reset/", "Device reset", reset_tree),
	nOSC_QUERY_ITEM_NODE("calibration/", "Calibration", calibration_tree),

	// sockets
	nOSC_QUERY_ITEM_NODE("config/", "Configuration", config_tree),
	nOSC_QUERY_ITEM_NODE("sntp/", "Simplified Network Time Protocol Socket", sntp_tree),
	nOSC_QUERY_ITEM_NODE("ipv4ll/", "IPv4 Link Local Addressing Socket", ipv4ll_tree),
	nOSC_QUERY_ITEM_NODE("dhcpc/", "DHCP Client Socket", dhcpc_tree),
	nOSC_QUERY_ITEM_NODE("debug/", "Debug Socket", debug_tree),
	nOSC_QUERY_ITEM_NODE("mdns/", "Multicast DNS Socket", mdns_tree),

	// output engines
	nOSC_QUERY_ITEM_NODE("engines/", "Output engines", engines_tree),
	nOSC_QUERY_ITEM_NODE("sensors/", "Sensor array", sensors_tree),
};

static const nOSC_Query_Item root = nOSC_QUERY_ITEM_NODE("/", "Root node", root_tree);

/*
"/!" "s" "reply address"
"/reply" "ss" "/!" "{}"
*/

static uint_fast8_t
_query(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	char *nil = "nil";

	if(fmt[0] == nOSC_INT32)
	{
		int32_t uuid = args[0].i;

		char *query = strrchr(path, '!'); // last occurence
		if(query)
		{
			// serialize empty string
			size = CONFIG_SUCCESS("iss", uuid, path, nil);
			size -= 4;

			// wind back to beginning of empty string on buffer
			uint8_t *response = BUF_O_OFFSET(buf_o_ptr) + size;

			// serialize query response directly to buffer
			*query = 0;
			const nOSC_Query_Item *item = nosc_query_find(&root, path);
			if(item)
			{
				nosc_query_response(response, item, path);

				// calculate new message size
				uint8_t *ptr = response;
				uint16_t len = strlen(response) + 1;
				ptr +=  len;
				uint16_t rem;
				if(rem=len%4)
				{
					memset(ptr, '\0', 4-rem);
					ptr += 4-rem;
				}
				size += ptr-response;
			}
			else
				size = CONFIG_FAIL("iss", uuid, path, "unknown query for path");
		}
		else
		{
			const nOSC_Query_Item *item = nosc_query_find(&root, path);
			if(item && (item->type != nOSC_QUERY_NODE) )
			{
				nOSC_Method_Cb cb = item->item.method.cb;
				if(cb && nosc_query_check(item, fmt+1, args+1))
					return cb(path, fmt, argc, args);
				else
					size = CONFIG_FAIL("iss", uuid, path, "callback, format or range invalid");
			}
			else
				size = CONFIG_FAIL("iss", uuid, path, "unknown method for path or format");
		}
	}
	else
		size = CONFIG_FAIL("iss", 0, path, "wrong format, uuid(int32) expected");

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}
