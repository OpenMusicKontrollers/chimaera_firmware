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
#include <stdio.h>
#include <math.h>

#include <libmaple/bkp.h> // backup register

#include <chimaera.h>
#include <config.h>
#include <chimutil.h>
#include <wiz.h>
#include <eeprom.h>
#include <cmc.h>
#include <midi.h>
#include <calibration.h>
#include <sntp.h>
#include <ptp.h>
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

static const Socket_Enable_Cb socket_callbacks [WIZ_MAX_SOCK_NUM] = {
	[SOCK_DHCPC]	= NULL, // = SOCK_ARP
	[SOCK_SNTP]		= sntp_enable,
	[SOCK_PTP_EV]	= ptp_enable,
	[SOCK_PTP_GE]	= NULL,
	[SOCK_OUTPUT]	= output_enable,
	[SOCK_CONFIG]	= config_enable,
	[SOCK_DEBUG]	= debug_enable,
	[SOCK_MDNS]		= mdns_enable,
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
		.derivatives = 0
	},

	.tuio1 = {
		.enabled = 0,
		.custom_profile = 0
	},

	.dump = {
		.enabled = 0
	},

	.scsynth = {
		.enabled = 0,
		.derivatives = 0
	},

	.oscmidi = {
		.enabled = 0,
		.multi = 1,
		.format = OSC_MIDI_FORMAT_MIDI,
		.path = {'/', 'm', 'i', 'd', 'i', '\0'}
	},

	.dummy = {
		.enabled = 0,
		.redundancy = 0,
		.derivatives = 0
	},

	.custom = {
		.enabled = 0,
		/*
		.items = {
			[0] = {
				.dest = RPN_NONE,
				.path = {'\0'},
				.fmt = {'\0'},
				.vm = { .inst = { RPN_TERMINATOR } }
			}
		}
		*/
	},
	
	.output = {
		.osc = {
			.socket = {
				.sock = SOCK_OUTPUT,
				.enabled = 1,
				.port = {3333, 3333},
				.ip = IP_BROADCAST
			},
			.mode = OSC_MODE_UDP,
			.server = 0
		},
		.offset = 0.002ULLK, // := 2ms offset
		.invert = {
			.x = 0,
			.z = 0
		},
		.parallel = 1
	},

	.config = {
		.osc = {
			.socket = {
				.sock = SOCK_CONFIG,
				.enabled = 1,
				.port = {4444, 4444},
				.ip = IP_BROADCAST
			},
			.mode = OSC_MODE_UDP,
			.server = 1
		}
	},

	.ptp = {
		.multiplier = 4, // # of sync messages between PTP delay requests 
		.offset_stiffness = 16,
		.delay_stiffness = 16,
		.event = {
			.sock = SOCK_PTP_EV,
			.enabled = 0,
			.port = {319, 319},
			.ip = {224, 0, 1, 129} // PTPv2 multicast group, 224.0.0.107 for peer-delay measurements
		},
		.general = {
			.sock = SOCK_PTP_GE,
			.enabled = 0,
			.port = {320, 320},
			.ip = {224, 0, 1, 129} // PTPv2 multicast group, 224.0.0.107 for peer-delay measurements
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
		.osc = {
			.socket = {
				.sock = SOCK_DEBUG,
				.enabled = 1,
				.port = {6666, 6666},
				.ip = IP_BROADCAST
			},
			.mode = OSC_MODE_UDP,
			.server = 0
		}
	},

	.ipv4ll = {
		.enabled = 1
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

	.sensors = {
		.movingaverage_bitshift = 3,
		.interpolation_mode = INTERPOLATION_QUADRATIC,
		.velocity_stiffness = 32,
		.rate = 2000
	},

	// we only define attributes for two groups for factory settings
	.groups = {
		[0] = {
			.x0 = 0.f,
			.x1 = 1.f,
			.m = 1.f,
			.gid = 0,
			.pid = CMC_SOUTH
		},
		[1] = {
			.x0 = 0.f,
			.x1 = 1.f,
			.m = 1.f,
			.gid = 1,
			.pid = CMC_NORTH
		}
	},

	.scsynth_groups = {
		[0] = {
			.name = {'g', 'r', 'o', 'u', 'p', '0', '\0'},
			.sid = 1000,
			.group = 0,
			.out = 0, 
			.arg = 0,
			.alloc = 1,
			.gate = 1,
			.add_action = SCSYNTH_ADD_TO_HEAD,
			.is_group = 0
		},
		[1] = {
			.name = {'g', 'r', 'o', 'u', 'p', '1', '\0'},
			.sid = 1000,
			.group = 1,
			.out = 1, 
			.arg = 0,
			.alloc = 1,
			.gate = 1,
			.add_action = SCSYNTH_ADD_TO_HEAD,
			.is_group = 0
		}
	},

	.oscmidi_groups = {
		[0] = {
			.mapping = OSC_MIDI_MAPPING_CONTROL_CHANGE,
			.control = 0x07,
			.offset = MIDI_BOT,
			.range = MIDI_RANGE
		},
		[1] = {
			.mapping = OSC_MIDI_MAPPING_CONTROL_CHANGE,
			.control = 0x07,
			.offset = MIDI_BOT,
			.range = MIDI_RANGE
		}
	}
};

uint16_t
CONFIG_SUCCESS(const char *fmt, ...)
{
	osc_data_t *buf = BUF_O_OFFSET(buf_o_ptr);
	osc_data_t *end = BUF_O_MAX(buf_o_ptr);
	osc_data_t *buf_ptr = buf;
	osc_data_t *preamble = NULL;

	if(config.config.osc.mode == OSC_MODE_TCP)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &preamble);

  va_list args;
  va_start(args, fmt);
	buf_ptr = osc_set_varlist(buf_ptr, end, success_str, fmt, args);
  va_end(args);
	
	if(config.config.osc.mode == OSC_MODE_TCP)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, preamble);

	uint16_t size = osc_len(buf_ptr, buf);
	if(config.config.osc.mode == OSC_MODE_SLIP)
		size = slip_encode(buf, size);

	return size;
}

uint16_t
CONFIG_FAIL(const char *fmt, ...)
{
	osc_data_t *buf = BUF_O_OFFSET(buf_o_ptr);
	osc_data_t *end = BUF_O_MAX(buf_o_ptr);
	osc_data_t *buf_ptr = buf;
	osc_data_t *preamble = NULL;

	if(config.config.osc.mode == OSC_MODE_TCP)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &preamble);

  va_list args;
  va_start(args, fmt);
	buf_ptr = osc_set_varlist(buf_ptr, end, fail_str, fmt, args);
  va_end(args);

	uint16_t size = osc_len(buf_ptr, buf);
	if(config.config.osc.mode == OSC_MODE_TCP)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, preamble);
	else if(config.config.osc.mode == OSC_MODE_SLIP)
		size = slip_encode(buf, size); //FIXME overflow

	return size;
}

void
CONFIG_SEND(uint16_t size)
{
	osc_send(&config.config.osc, BUF_O_BASE(buf_o_ptr), size);
}

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
config_check_bool(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf, uint8_t *booly)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *booly ? 1 : 0);
	else
	{
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		*booly = i != 0 ? 1 : 0;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

uint_fast8_t
config_check_uint8(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf, uint8_t *val)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *val);
	else
	{
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		*val = i;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

uint_fast8_t
config_check_uint16(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf, uint16_t *val)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *val);
	else
	{
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		*val = i;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

uint_fast8_t
config_check_float(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf, float *val)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isf", uuid, path, *val);
	else
	{
		float f;
		buf_ptr = osc_get_float(buf_ptr, &f);
		*val = f;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_info_version(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	sprintf(string_buf, "%i.%i.%i-%i",
		config.version.major,
		config.version.minor,
		config.version.patch,
		config.version.revision);
	size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_info_uid(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	uid_str(string_buf);
	size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_info_name(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, config.name);
	else
	{
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		strcpy(config.name, s);

		//FIXME we should do a probe to look for an existing name
		if(config.mdns.socket.enabled)
			mdns_update(); // announce new name

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_config_load(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(config_load())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "loading of configuration from EEPROM failed");

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_config_save(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(config_save())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "saving configuration to EEPROM failed");

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_comm_mac(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
	{
		mac2str(config.comm.mac, string_buf);
		size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	}
	else
	{
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		if(str2mac(s, config.comm.mac))
		{
			wiz_mac_set(config.comm.mac);
			config.comm.custom_mac = 1;
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "wrong format");
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_comm_ip(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

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
		const char *cidr;
		buf_ptr = osc_get_string(buf_ptr, &cidr);

		if(argc == 3)
		{
			const char *gat;
			buf_ptr = osc_get_string(buf_ptr, &gat);
			if(str2ip(gat, config.comm.gateway))
				wiz_gateway_set(config.comm.gateway);
			else // return
			{
				size = CONFIG_FAIL("iss", uuid, path, "gateway invalid, format: x.x.x.x");
				CONFIG_SEND(size);
				return 1;
			}
		}

		uint8_t mask;
		if(str2ipCIDR(cidr, config.comm.ip, &mask))
		{
			wiz_ip_set(config.comm.ip);

			cidr_to_subnet(config.comm.subnet, mask);
			wiz_subnet_set(config.comm.subnet);

			if(argc == 2) // no custom gateway was given
			{
				*gateway_ptr =(*ip_ptr) & (*subnet_ptr); // default gateway =(ip & subnet)
				wiz_gateway_set(config.comm.gateway);
			}

			//FIXME disrupts PTP
			//FIXME we should do a probe to look for an existing name
			if(config.mdns.socket.enabled)
				mdns_update(); // announce new IP

			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "ip invalid, format: x.x.x.x/x");
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_comm_gateway(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
	{
		ip2str(config.comm.gateway, string_buf);
		size = CONFIG_SUCCESS("iss", uuid, path, string_buf);
	}
	else
	{
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		if(str2ip(s, config.comm.gateway))
		{
			wiz_gateway_set(config.comm.gateway);
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else //FIXME mDNS resolve
			size = CONFIG_FAIL("iss", uuid, path, "gateway invalid, format: x.x.x.x");
	}

	CONFIG_SEND(size);

	return 1;
}

uint_fast8_t
config_socket_enabled(Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, socket->enabled ? 1 : 0);
	else
	{
		Socket_Enable_Cb cb = socket_callbacks[socket->sock];
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		cb(i);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_socket_enabled(&config.output.osc.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_output_reset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	config.dump.enabled = 0;
	config.tuio2.enabled = 0;
	config.tuio1.enabled = 0;
	config.scsynth.enabled = 0;
	config.oscmidi.enabled = 0;
	config.dummy.enabled = 0;
	config.custom.enabled = 0;

	cmc_engines_update();

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_mode(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);
	uint8_t *mode = &config.output.osc.mode;
	uint8_t enabled = config.output.osc.socket.enabled;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, config_mode_args_values[*mode]);
	else
	{
		output_enable(0);
		uint_fast8_t i;
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		for(i=0; i<sizeof(config_mode_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(s, config_mode_args_values[i].s))
			{
				*mode = i;
				break;
			}
		output_enable(enabled);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_server(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	// needs to proceed _output_mode
	return config_check_uint8(path, fmt, argc, buf, &config.output.osc.server);
}

static uint_fast8_t
_config_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_socket_enabled(&config.config.osc.socket, path, fmt, argc, buf);
}

#define ADDRESS_CB_LEN 32
typedef struct _Address_Cb Address_Cb;

struct _Address_Cb {
	int32_t uuid;
	char path [ADDRESS_CB_LEN];
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
	
	CONFIG_SEND(size);
}

uint_fast8_t
config_address(Socket_Config *socket, const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	static Address_Cb address_cb;
	uint16_t size = 0;
	int32_t uuid;
	const char *hostname;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);
	buf_ptr = osc_get_string(buf_ptr, &hostname);

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
		strncpy(address_cb.path, path, ADDRESS_CB_LEN-1);
		address_cb.socket = socket;

		if(!strncmp(hostname, "this", 4)) // set IP to requesting IP
		{
			if(sscanf(hostname, "this:%hu", &port) == 1) // set port manually
				address_cb.port = port;
			else
				address_cb.port = config.config.osc.socket.port[DST_PORT]; // set port to requesting port
			memcpy(ip, config.config.osc.socket.ip, 4);
			_address_dns_cb(ip, &address_cb);
		}
		else if(str2addr(hostname, ip, &port))
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
		CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_config_mode(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);
	uint8_t *mode = &config.config.osc.mode;
	uint8_t enabled = config.config.osc.socket.enabled;

	if(argc == 1) // query
	{
		size = CONFIG_SUCCESS("iss", uuid, path, config_mode_args_values[*mode]);
		CONFIG_SEND(size);
	}
	else
	{
		if(config.mdns.socket.enabled)
			mdns_goodbye(); // invalidate currently registered services

		// XXX need to send reply before disabling socket...
		size = CONFIG_SUCCESS("is", uuid, path);
		CONFIG_SEND(size);

		config_enable(0);
		uint_fast8_t i;
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		for(i=0; i<sizeof(config_mode_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(s, config_mode_args_values[i].s))
			{
				*mode = i;
				break;
			}
		config_enable(enabled);

		if(config.mdns.socket.enabled)
			mdns_announce(); // announce new mode
	}

	return 1;
}

static uint_fast8_t
_output_address(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_address(&config.output.osc.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_output_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
	{
		float f = config.output.offset;
		size = CONFIG_SUCCESS("isf", uuid, path, f); // output timestamp, double, float?
	}
	else
	{
		float f;
		buf_ptr = osc_get_float(buf_ptr, &f);
		config.output.offset = f;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_invert_x(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
	{
		int32_t x = config.output.invert.x;
		size = CONFIG_SUCCESS("isi", uuid, path, x);
	}
	else
	{
		int32_t x;
		buf_ptr = osc_get_int32(buf_ptr, &x);
		config.output.invert.x = x;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_invert_z(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
	{
		int32_t z = config.output.invert.z;
		size = CONFIG_SUCCESS("isi", uuid, path, z);
	}
	else
	{
		int32_t z;
		buf_ptr = osc_get_int32(buf_ptr, &z);
		config.output.invert.z = z;
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_output_parallel(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.output.parallel);
}

static uint_fast8_t
_reset_soft(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(config.mdns.socket.enabled)
		mdns_goodbye(); // invalidate currently registered IP and services

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	// reset factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_SOFT);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t
_reset_hard(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(config.mdns.socket.enabled)
		mdns_goodbye(); // invalidate currently registered IP and services

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	// set factory reset flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_HARD);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t
_reset_flash(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(config.mdns.socket.enabled)
		mdns_goodbye(); // invalidate currently registered IP and services

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	// set bootloader flag
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_SYSTEM_FLASH);
	bkp_disable_writes();

	nvic_sys_reset();

	return 1;
}

static uint_fast8_t _query(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf);

// globals
const OSC_Method config_serv [] = {
	{NULL, NULL, _query},
	{NULL, NULL, NULL} // terminator
};

const OSC_Query_Value config_mode_args_values [] = {
	[OSC_MODE_UDP]	= { .s = "osc.udp" },
	[OSC_MODE_TCP]	= { .s = "osc.tcp" },
	[OSC_MODE_SLIP]	= { .s = "osc.slip.tcp" }
};

const OSC_Query_Argument config_mode_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("mode", OSC_QUERY_MODE_RW, config_mode_args_values)
};

// global arguments
const OSC_Query_Argument config_boolean_args [] = {
	OSC_QUERY_ARGUMENT_BOOL("Boolean", OSC_QUERY_MODE_RW)
};

const OSC_Query_Argument config_address_args [] = {
	OSC_QUERY_ARGUMENT_STRING("32-bit decimal dotted or mDNS .local domain with colon and port", OSC_QUERY_MODE_RW, 64)
};

// locals
static const OSC_Query_Argument comm_mac_args [] = {
	OSC_QUERY_ARGUMENT_STRING("EUI-48 hexadecimal colon", OSC_QUERY_MODE_RW, 17)
};

static const OSC_Query_Argument comm_ip_args [] = {
	OSC_QUERY_ARGUMENT_STRING("32-bit decimal dotted CIDR", OSC_QUERY_MODE_RW, 18)
};

static const OSC_Query_Argument comm_gateway_args [] = {
	OSC_QUERY_ARGUMENT_STRING("32-bit decimal dotted or mDNS .local domain", OSC_QUERY_MODE_RW, 64)
};

static const OSC_Query_Argument comm_address_args [] = {
	OSC_QUERY_ARGUMENT_STRING("32-bit decimal dotted or mDNS .local domain", OSC_QUERY_MODE_W, 64)
};

const OSC_Query_Item comm_tree [] = {
	OSC_QUERY_ITEM_METHOD("mac", "Hardware MAC address", _comm_mac, comm_mac_args),
	OSC_QUERY_ITEM_METHOD("ip", "IPv4 client address", _comm_ip, comm_ip_args),
	OSC_QUERY_ITEM_METHOD("gateway", "IPv4 gateway address", _comm_gateway, comm_gateway_args),
};

const OSC_Query_Item config_tree [] = {
	OSC_QUERY_ITEM_METHOD("save", "Save to EEPROM", _config_save, NULL),
	OSC_QUERY_ITEM_METHOD("load", "Load from EEPROM", _config_load, NULL),
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable socket", _config_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("mode", "Enable/disable UDP/TCP mode", _config_mode, config_mode_args)
};

const OSC_Query_Item reset_tree [] = {
	OSC_QUERY_ITEM_METHOD("soft", "Soft reset", _reset_soft, NULL),
	OSC_QUERY_ITEM_METHOD("hard", "Hard reset", _reset_hard, NULL),
	OSC_QUERY_ITEM_METHOD("flash", "Reset into flash mode", _reset_flash, NULL),
};

static const OSC_Query_Argument info_version_args [] = {
	OSC_QUERY_ARGUMENT_STRING("{Major}.{Minor}.{Patch level}-{Board revision}", OSC_QUERY_MODE_R, 32)
};

static const OSC_Query_Argument info_uid_args [] = {
	OSC_QUERY_ARGUMENT_STRING("Hexadecimal hyphen", OSC_QUERY_MODE_R, 32)
};

static const OSC_Query_Argument info_name_args [] = {
	OSC_QUERY_ARGUMENT_STRING("ASCII", OSC_QUERY_MODE_RW, NAME_LENGTH)
};

static const OSC_Query_Item info_tree [] = { //FIXME merge with comm?
	OSC_QUERY_ITEM_METHOD("version", "Firmware version", _info_version, info_version_args),
	OSC_QUERY_ITEM_METHOD("uid", "96-bit universal device identifier", _info_uid, info_uid_args),

	OSC_QUERY_ITEM_METHOD("name", "Device name", _info_name, info_name_args),
};

static const OSC_Query_Argument engines_offset_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Seconds", OSC_QUERY_MODE_RW, 0.f, INFINITY, 0.0001f)
};

static const OSC_Query_Argument engines_invert_args [] = {
	OSC_QUERY_ARGUMENT_BOOL("axis inversion", OSC_QUERY_MODE_RW)
};

static const OSC_Query_Item engines_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _output_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("address", "Single remote host", _output_address, config_address_args),
	OSC_QUERY_ITEM_METHOD("offset", "OSC bundle offset timestamp", _output_offset, engines_offset_args),
	OSC_QUERY_ITEM_METHOD("invert_x", "Enable/disable x-axis inversion", _output_invert_x, engines_invert_args),
	OSC_QUERY_ITEM_METHOD("invert_z", "Enable/disable z-axis inversion", _output_invert_z, engines_invert_args),
	OSC_QUERY_ITEM_METHOD("parallel", "Parallel processing", _output_parallel, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("reset", "Disable all engines", _output_reset, NULL),
	OSC_QUERY_ITEM_METHOD("mode", "Enable/disable UDP/TCP mode", _output_mode, config_mode_args),
	OSC_QUERY_ITEM_METHOD("server", "Enable/disable TCP server mode", _output_server, config_boolean_args),

	// engines
	OSC_QUERY_ITEM_NODE("dump/", "Dump output engine", dump_tree),
	OSC_QUERY_ITEM_NODE("dummy/", "Dummy output engine", dummy_tree),
	OSC_QUERY_ITEM_NODE("oscmidi/", "OSC MIDI output engine", oscmidi_tree),
	OSC_QUERY_ITEM_NODE("scsynth/", "SuperCollider output engine", scsynth_tree),
	OSC_QUERY_ITEM_NODE("tuio2/", "TUIO 2.0 output engine", tuio2_tree),
	OSC_QUERY_ITEM_NODE("tuio1/", "TUIO 1.0 output engine", tuio1_tree),
	OSC_QUERY_ITEM_NODE("custom/", "Custom output engine", custom_tree)
};

static const OSC_Query_Item root_tree [] = {
	OSC_QUERY_ITEM_NODE("info/", "Information", info_tree),
	OSC_QUERY_ITEM_NODE("comm/", "Communitation", comm_tree),
	OSC_QUERY_ITEM_NODE("reset/", "Reset", reset_tree),
	OSC_QUERY_ITEM_NODE("calibration/", "Calibration", calibration_tree),

	// sockets
	OSC_QUERY_ITEM_NODE("config/", "Configuration", config_tree),
	OSC_QUERY_ITEM_NODE("sntp/", "Simplified Network Time Protocol", sntp_tree),
	OSC_QUERY_ITEM_NODE("ptp/", "Precision Time Protocol", ptp_tree),
	OSC_QUERY_ITEM_NODE("ipv4ll/", "IPv4 Link Local Addressing", ipv4ll_tree),
	OSC_QUERY_ITEM_NODE("dhcpc/", "DHCP Client", dhcpc_tree),
	OSC_QUERY_ITEM_NODE("debug/", "Debug", debug_tree),
	OSC_QUERY_ITEM_NODE("mdns/", "Multicast DNS", mdns_tree),

	// output engines
	OSC_QUERY_ITEM_NODE("engines/", "Output engines", engines_tree),
	OSC_QUERY_ITEM_NODE("sensors/", "Sensor array", sensors_tree),
};

static const OSC_Query_Item root = OSC_QUERY_ITEM_NODE("/", "Root node", root_tree);

//FIXME check for overflows
static uint_fast8_t
_query(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	const char *nil = "nil";

	if(fmt[0] == OSC_INT32)
	{
		int32_t uuid;

		buf_ptr = osc_get_int32(buf_ptr, &uuid);

		char *query = strrchr(path, '!'); // last occurence
		if(query)
		{
			*query = '\0';
			const OSC_Query_Item *item = osc_query_find(&root, path, -1);
			*query = '!';
			if(item)
			{
				// serialize empty string
				size = CONFIG_SUCCESS("iss", uuid, path, nil);
				size -= 4;

				// wind back to beginning of empty string on buffer
				char *response = (char *)(BUF_O_OFFSET(buf_o_ptr) + size);

				// serialize query response directly to buffer
				*query = '\0';
				osc_query_response(response, item, path);

				// calculate new message size
				char *ptr = response;
				uint16_t len = strlen(response) + 1;
				ptr +=  len;
				uint16_t rem;
				if((rem=len%4))
				{
					memset(ptr, '\0', 4-rem);
					ptr += 4-rem;
				}
				size += ptr-response;

				switch(config.config.osc.mode)
				{
					case OSC_MODE_UDP:
						break;
					case OSC_MODE_TCP:
					{
						// update TCP preamble
						int32_t *tcp_size = (int32_t *)BUF_O_OFFSET(buf_o_ptr);
						*tcp_size = htonl(size - sizeof(int32_t));
						break;
					}
					case OSC_MODE_SLIP:
					{
						//slip_encode
						size = slip_encode(BUF_O_OFFSET(buf_o_ptr), size);
						break;
					}
				}
			}
			else
				size = CONFIG_FAIL("iss", uuid, path, "unknown query for path");
		}
		else
		{
			const OSC_Query_Item *item = osc_query_find(&root, path, -1);
			if(item && (item->type != OSC_QUERY_NODE) && (item->type != OSC_QUERY_ARRAY) )
			{
				OSC_Method_Cb cb = item->item.method.cb;
				if(cb && osc_query_check(item, fmt+1, buf_ptr))
					return cb(path, fmt, argc, buf);
				else
					size = CONFIG_FAIL("iss", uuid, path, "callback, format or range invalid");
			}
			else
				size = CONFIG_FAIL("iss", uuid, path, "unknown method for path or format");
		}
	}
	else
		size = CONFIG_FAIL("iss", 0, path, "wrong format, uuid(int32) expected");

	CONFIG_SEND(size);

	return 1;
}
