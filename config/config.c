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

#include <chimutil.h>
#include <udp.h>

#define GLOB_BROADCAST {255, 255, 255, 255}

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
		.socket = {
			.sock = 0,
			.port = 3333,
			.ip = LAN_BROADCAST
		},
		.long_header = 0,
	},

	.config = {
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 1,
			.port = 4444,
			.ip = LAN_BROADCAST
		}
	},

	.sntp = {
		.enabled = 1, // enabled by default
		.socket = {
			.sock = 2,
			.port = 123,
			.ip = LAN_HOST
		}
	},

	.dump = {
		.enabled = 0, // disabled by default
		.socket = {
			.sock = 3,
			.port = 5555,
			.ip = LAN_BROADCAST
		}
	},

	.debug = {
		.enabled = 0,
		.socket = {
			.sock = 4,
			.port = 6666,
			.ip = LAN_BROADCAST
		}
	},

	.rtpmidi = {
		.enabled = 0, // disabled by default

		.payload = {
			.socket = {
				.sock = 5,
				.port = 7777,
				.ip = LAN_BROADCAST
			}
		},

		.session = {
			.socket = {
				.sock = 6,
				.port = 8888,
				.ip = LAN_BROADCAST
			}
		}
	},

	.ping = {
		.enabled = 0, // disabled by default
		.socket = {
			.sock = 7,
			.port = 255,
			.ip = GLOB_BROADCAST
		}
	},

	.cmc = {
		.diff = 0,
		.thresh0 = 60,
		.thresh1 = 120,
		.max_groups = 32
	},

	.rate = 1500 // update rate in Hz
};

static uint8_t
_version_get (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = data;
	uint16_t size;
	int32_t id = args[0]->i;

	size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iTiii", id, config.version.major, config.version.minor, config.version.patch_level);
	udp_send (config.config.socket.sock, buf, size);

	return 1;
}

static uint8_t
_enabled_set (void (*cb) (uint8_t b), void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = data;
	uint16_t size;
	int32_t id = args[0]->i;
	if (fmt[1] == nOSC_TRUE)
	{
		cb (1);
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	}
	else if (fmt[1] == nOSC_FALSE)
	{
		cb (0);
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	}
	else
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iFs", id, "wrong type: boolean expected at position [2]");

	udp_send (config.config.socket.sock, buf, size);

	return 1;
} 

static uint8_t
_tuio_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (tuio_enable, data, path, fmt, argc, args);
}

static uint8_t
_config_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (config_enable, data, path, fmt, argc, args);
}

static uint8_t
_sntp_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (sntp_enable, data, path, fmt, argc, args);
}

static uint8_t
_dump_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (dump_enable, data, path, fmt, argc, args);
}

static uint8_t
_debug_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (debug_enable, data, path, fmt, argc, args);
}

static uint8_t
_rtpmidi_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (rtpmidi_enable, data, path, fmt, argc, args);
}

static uint8_t
_ping_enabled_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _enabled_set (ping_enable, data, path, fmt, argc, args);
}

static uint8_t
_socket_set (Socket_Config *socket, void (*cb) (uint8_t b), void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = data;
	uint16_t size;
	int32_t id = args[0]->i;

	if ( (args[1]->i < 0x100) && (args[2]->i < 0x10) && (args[3]->i < 0x10) && (args[4]->i < 0x10) && (args[5]->i < 0x10) )
	{
		socket->port = args[1]->i;
		socket->ip[0] = args[2]->i;
		socket->ip[1] = args[3]->i;
		socket->ip[2] = args[4]->i;
		socket->ip[3] = args[5]->i;

		cb (config.tuio.enabled);

		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	}
	else
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iFs", id, "wrong range: port number must be < 0x100 and numbers in IP must be < 0x10");

	udp_send (config.config.socket.sock, buf, size);

	return 1;
}

static uint8_t
_tuio_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.tuio.socket, tuio_enable, data, path, fmt, argc, args);
}

static uint8_t
_config_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.config.socket, config_enable, data, path, fmt, argc, args);
}

static uint8_t
_sntp_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.sntp.socket, sntp_enable, data, path, fmt, argc, args);
}

static uint8_t
_dump_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.dump.socket, dump_enable, data, path, fmt, argc, args);
}

static uint8_t
_debug_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.debug.socket, debug_enable, data, path, fmt, argc, args);
}

static uint8_t
_rtpmidi_payload_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.rtpmidi.payload.socket, rtpmidi_enable, data, path, fmt, argc, args);
}

static uint8_t
_rtpmidi_session_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.rtpmidi.session.socket, rtpmidi_enable, data, path, fmt, argc, args);
}

static uint8_t
_ping_socket_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	return _socket_set (&config.ping.socket, ping_enable, data, path, fmt, argc, args);
}

static uint8_t
_rate_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = (uint8_t *)data;
	uint16_t size;
	int32_t id = args[0]->i;

	// TODO implement infinite rate (run at maximal rate), we need a working _irq_adc for this, though
	config.rate = args[1]->i;

	adc_timer_pause ();
	adc_timer_reconfigure ();
	adc_timer_resume ();

	size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf, size);

	return 1;
}

static uint8_t
_cmc_group_clear (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = (uint8_t *)data;
	uint16_t size;
	int32_t id = args[0]->i;

	cmc_group_clear (cmc);

	size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	udp_send (config.config.socket.sock, buf, size);

	return 1;
}

static uint8_t
_cmc_group_add (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = (uint8_t *)data;
	uint16_t size;
	int32_t id = args[0]->i;

	if (cmc_group_add (cmc, args[1]->i, args[2]->i, args[3]->f, args[4]->f))
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iF", id, "maximal number of groups reached");
	udp_send (config.config.socket.sock, buf, size);

	return 1;
}

static uint8_t
_cmc_group_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	uint8_t *buf = (uint8_t *)data;
	uint16_t size;
	int32_t id = args[0]->i;

	if (cmc_group_set (cmc, args[1]->i, args[2]->i, args[3]->f, args[4]->f))
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iT", id);
	else
		size = nosc_message_vararg_serialize (buf, CONFIG_REPLY_PATH, "iF", id, "group not found");
	udp_send (config.config.socket.sock, buf, size);

	return 1;
}


nOSC_Server *
config_methods_add (nOSC_Server *serv, void *data)
{
	// read-only
	serv = nosc_server_method_add (serv, "/chimaera/version/get", "i", _version_get, data);

	// config TODO
	//serv = nosc_server_method_add (serv, "/chimaera/config/get", "i", _config_get, data);
	//serv = nosc_server_method_add (serv, "/chimaera/config/save", "i", _save, data), TODO

	//TODO comm/mac/set, comm/ip/set, comm/gateway/set, comm/subnet/set

	// enable/disable sockets
	serv = nosc_server_method_add (serv, "/chimaera/tuio/enabled/set", "iT", _tuio_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/tuio/enabled/set", "iF", _tuio_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/tuio/socket/set", "iiiiii", _tuio_socket_set, data);
	//TODO tuio/long_header/enabled/set

	serv = nosc_server_method_add (serv, "/chimaera/config/enabled/set", "iT", _config_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/config/enabled/set", "iF", _config_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/config/socket/set", "iiiiii", _config_socket_set, data);

	serv = nosc_server_method_add (serv, "/chimaera/sntp/enabled/set", "iT", _sntp_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/sntp/enabled/set", "iF", _sntp_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/sntp/socket/set", "iiiiii", _sntp_socket_set, data);

	serv = nosc_server_method_add (serv, "/chimaera/dump/enabled/set", "iT", _dump_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/dump/enabled/set", "iF", _dump_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/dump/socket/set", "iiiiii", _dump_socket_set, data);

	serv = nosc_server_method_add (serv, "/chimaera/debug/enabled/set", "iT", _debug_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/debug/enabled/set", "iF", _debug_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/debug/socket/set", "iiiiii", _debug_socket_set, data);

	serv = nosc_server_method_add (serv, "/chimaera/rtpmidi/enabled/set", "iT", _rtpmidi_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/rtpmidi/enabled/set", "iF", _rtpmidi_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/rtpmidi/payload/socket/set", "iiiiii", _rtpmidi_payload_socket_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/rtpmidi/session/socket/set", "iiiiii", _rtpmidi_session_socket_set, data);

	serv = nosc_server_method_add (serv, "/chimaera/ping/enabled/set", "iT", _ping_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/ping/enabled/set", "iF", _ping_enabled_set, data);
	serv = nosc_server_method_add (serv, "/chimaera/ping/socket/set", "iiiiii", _ping_socket_set, data);

	// cmc TODO cmc/diff/set, cmc/thresh0/set, cmc/thresh1/set, cmc/max_groups/set

	serv = nosc_server_method_add (serv, "/chimaera/group/clear", "i", _cmc_group_clear, data);
	serv = nosc_server_method_add (serv, "/chimaera/group/add", "iiiff", _cmc_group_add, data);
	serv = nosc_server_method_add (serv, "/chimaera/group/set", "iiiff", _cmc_group_set, data);

	// sample rate
	serv = nosc_server_method_add (serv, "/chimaera/rate/set", "ii", _rate_set, data);

	return serv;
}
