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

#include <config.h>

#include <string.h>

#include <dma_udp.h>

#define LAN_BROADCAST {192, 168, 1, 255}
#define LAN_HOST {192, 168, 1, 10}

Config config = {
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
		.sock = 0,
		.port = 3333,
		.ip = LAN_BROADCAST
	},

	.config = {
		.enabled = 1, // enabled by default
		.sock = 1,
		.port = 4444,
		.ip = LAN_BROADCAST
	},

	.sntp = {
		.enabled = 1, // enabled by default
		.sock = 2,
		.port = 123,
		.ip = LAN_HOST
	},

	.dump = {
		.enabled = 0, // disabled by default
		.sock = 3,
		.port = 5555,
		.ip = LAN_BROADCAST
	},

	.rtpmidi = {
		.payload = {
			.enabled = 0, // disabled by default
			.sock = 4,
			.port = 6666,
			.ip = LAN_BROADCAST
		},

		.session = {
			.enabled = 0, // disabled by default
			.sock = 5,
			.port = 7777,
			.ip = LAN_BROADCAST
		}
	},

	.cmc = {
		.rate = 800, // update rate in Hz
		.diff = 0,
		.thresh0 = 60,
		.thresh1 = 120
	},

	.tuio_long_header = 0,
	.cmc_max_groups = 32
};

uint8_t
_version_get (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = data;
	uint16_t size;
	int32_t id = args[0]->i;

	size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iTiii", id, config.version.major, config.version.minor, config.version.patch_level);
	dma_udp_send (config.config.sock, buf, size);

	return 1;
}

uint8_t
_dump_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = data;
	uint16_t size;
	int32_t id = args[0]->i;

	if (fmt[1] == nOSC_TRUE)
	{
		config.dump.enabled = 1;
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	}
	else if (fmt[1] == nOSC_FALSE)
	{
		config.dump.enabled = 0;
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	}
	else
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iFs", id, "wrong type: boolean expected at position [2]");

	dma_udp_send (config.config.sock, buf, size);

	return 1;
}

nOSC_Server *
config_methods_add (nOSC_Server *serv, void *data)
{
	// read-only
	serv = nosc_server_method_add (serv, "/chimaera/version/get", "i", _version_get, data);

	/* TODO implement all _set and _get functions for config struct
	serv = nosc_server_method_add (serv, "/chimaera/comm/mac/get", "i", _comm_mac_get, data);
	serv = nosc_server_method_add (serv, "/chimaera/comm/mac/set", "iiiiiii", _comm_mac_set, data);
	...
	*/

	serv = nosc_server_method_add (serv, "/chimaera/dump/enabled/set", "iT", _dump_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/dump/enabled/set", "iF", _dump_enabled_set, data);

	return serv;
}
