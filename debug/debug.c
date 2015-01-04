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
		osc_data_t *end = BUF_O_MAX(buf_o_ptr);
		osc_data_t *buf_ptr = buf;
		osc_data_t *preamble = NULL;

		if(config.debug.osc.mode == OSC_MODE_TCP)
			buf_ptr = osc_start_bundle_item(buf_ptr, end, &preamble);

		va_list args;
		va_start(args, fmt);
		buf_ptr = osc_set_varlist(buf_ptr, end, "/debug", fmt, args);
		va_end(args);

		if(config.debug.osc.mode == OSC_MODE_TCP)
			buf_ptr = osc_end_bundle_item(buf_ptr, end, preamble);
		
		uint16_t size = osc_len(buf_ptr, buf);
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

/*
 * Query
 */

const OSC_Query_Item debug_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _debug_enabled, config_boolean_args)
};
