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

#include <debug.h>
#include <chimaera.h>
#include <config.h>

void
DEBUG(const char *fmt, ...)
{
	if(config.debug.osc.socket.enabled && (wiz_socket_state[SOCK_DEBUG] == WIZ_SOCKET_STATE_OPEN) )
	{
		va_list args;
		
		va_start(args, fmt);
		uint16_t size = nosc_message_varlist_serialize(BUF_O_OFFSET(buf_o_ptr),
			config.debug.osc.tcp,
			"/debug", fmt, args);
		va_end(args);

		osc_send(&config.debug.osc, BUF_O_BASE(buf_o_ptr), size);
	}
}

/*
 * Config
 */

static uint_fast8_t
_debug_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_socket_enabled(&config.debug.osc.socket, path, fmt, argc, args);
}

static uint_fast8_t
_debug_address(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_address(&config.debug.osc.socket, path, fmt, argc, args);
}

static uint_fast8_t
_debug_tcp(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	uint8_t *boolean = &config.debug.osc.tcp;
	uint8_t enabled = config.debug.osc.socket.enabled;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, *boolean);
	else
	{
		debug_enable(0);
		*boolean = args[1].i;
		debug_enable(enabled);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

const nOSC_Query_Item debug_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _debug_enabled, config_boolean_args),
	nOSC_QUERY_ITEM_METHOD("address", "Single remote address", _debug_address, config_address_args),
	nOSC_QUERY_ITEM_METHOD("tcp", "Enable/disable TCP mode", _debug_tcp, config_tcp_args)
};
