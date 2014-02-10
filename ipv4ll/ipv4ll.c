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

#include <stdlib.h>
#include <string.h>

#include <ipv4ll.h>
#include <config.h>

static const uint8_t link_local_gateway [] = {169, 254, 0, 0};
static const uint8_t link_local_subnet [] = {255, 255, 0, 0};

static void 
_IPv4LL_random (uint8_t *ip)
{
	ip[0] = 169;
	ip[1] = 254;
	ip[2] = 1 + rand () / (RAND_MAX / 253);
	ip[3] = rand () / (RAND_MAX / 255);
}

void
IPv4LL_claim (uint8_t *ip, uint8_t *gateway, uint8_t *subnet)
{
	uint8_t link_local_ip [4];

	do {
		_IPv4LL_random (link_local_ip);
	} while (arp_probe (0, link_local_ip)); // collision?

	arp_announce (0, link_local_ip);

	memcpy (ip, link_local_ip, 4);
	memcpy (gateway, link_local_gateway, 4);
	memcpy (subnet, link_local_subnet, 4);

	//FIXME actually, the user should do this before enabling IPv4LL, not?
	uint8_t brd [4];
	broadcast_address(brd, ip, subnet);
	memcpy (config.output.socket.ip, brd, 4);
	memcpy (config.config.socket.ip, brd, 4);
	memcpy (config.sntp.socket.ip, brd, 4);
	memcpy (config.debug.socket.ip, brd, 4);
}

/*
 * Config
 */

static uint_fast8_t
_ipv4ll_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	// needs a config save and reboot to take action
	return config_check_bool (path, fmt, argc, args, &config.ipv4ll.enabled);
}

/*
 * Query
 */

static const nOSC_Query_Argument ipv4ll_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", 1)
};

const nOSC_Query_Item ipv4ll_tree [] = {
	nOSC_QUERY_ITEM_METHOD_RW("enabled", "enable/disable", _ipv4ll_enabled, ipv4ll_enabled_args),
};
