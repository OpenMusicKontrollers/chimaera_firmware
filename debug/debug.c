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

#include <debug.h>
#include <config.h>

/*
 * Config
 */

static uint_fast8_t
_debug_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_socket_enabled (&config.debug.socket, path, fmt, argc, args);
}

static uint_fast8_t
_debug_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_address (&config.debug.socket, path, fmt, argc, args);
}

/*
 * Query
 */

static const nOSC_Query_Argument debug_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("bool", nOSC_QUERY_MODE_RW, 0, 1)
};

static const nOSC_Query_Argument debug_address_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("ascii", nOSC_QUERY_MODE_RW)
};

const nOSC_Query_Item debug_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "enable/disable", _debug_enabled, debug_enabled_args),
	nOSC_QUERY_ITEM_METHOD("address", "remote address", _debug_address, debug_address_args),
};
