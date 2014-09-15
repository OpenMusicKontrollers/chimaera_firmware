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
#include <chimutil.h>
#include <config.h>

void
DEBUG(const char *fmt, ...)
{
	if(config.debug.osc.socket.enabled && (wiz_socket_state[SOCK_DEBUG] == WIZ_SOCKET_STATE_OPEN) )
	{
		osc_data_t *buf = BUF_O_OFFSET(buf_o_ptr);
		osc_data_t *buf_ptr = buf;
		osc_data_t *preamble = NULL;

		if(config.debug.osc.mode == OSC_MODE_TCP)
			buf_ptr = osc_start_bundle_item(buf_ptr, &preamble);

		va_list args;
		va_start(args, fmt);
		buf_ptr = osc_varlist_set(buf_ptr, "/debug", fmt, args);
		va_end(args);

		if(config.debug.osc.mode == OSC_MODE_TCP)
			buf_ptr = osc_end_bundle_item(buf_ptr, preamble);
		
		uint16_t size = buf_ptr - buf;
		if(config.debug.osc.mode == OSC_MODE_SLIP)
			size = slip_encode(buf, size);

		osc_send(&config.debug.osc, BUF_O_BASE(buf_o_ptr), size);
	}
}

/*
 * Config
 */

static uint_fast8_t
_debug_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_socket_enabled(&config.debug.osc.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_debug_address(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_address(&config.debug.osc.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_debug_mode(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;
	uint8_t *mode = &config.debug.osc.mode;
	uint8_t enabled = config.debug.osc.socket.enabled;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, config_mode_args_values[*mode]);
	else
	{
		debug_enable(0);
		uint_fast8_t i;
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		for(i=0; i<sizeof(config_mode_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(s, config_mode_args_values[i].s))
			{
				*mode = i;
				break;
			}
		debug_enable(enabled);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

const OSC_Query_Item debug_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _debug_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("address", "Single remote address", _debug_address, config_address_args),
	OSC_QUERY_ITEM_METHOD("mode", "Enable/disable UDP/TCP mode", _debug_mode, config_mode_args)
};
