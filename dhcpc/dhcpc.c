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

#include "dhcpc_private.h"

#include <string.h>

static uint32_t xid = 0x3903F326;

DHCP_Request request = {
	.packet = {
		.op = 1,
		.htype = 1,
		.hlen = 6,

		.flags = {0x80, 0x00} // multicast
	},

	.magic_cookie = {0x63, 0x82, 0x53, 0x63},

	.option_53 = {
		.code = 53,
		.len = 1,
		.dat = DHCPDISCOVER
	},

	.option_61 = {
		.code = 61,
		.len = 7,
		.typ = 1
	},

	.option_12 = {
		.code = 12,
		.len = 8,
		.dat = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a'}
	},

	.option_55 = {
		.code = 55,
		.len = 4,
		.dat = {1, 3, 15, 6}
	},

	.option_end = {
		.code = 255
	}
};

uint16_t
dhcpc_discover (uint8_t *buf, uint16_t secs)
{
	request.packet.xid = htonl (xid);
	request.packet.secs = hton (secs);
	memcpy (request.packet.chaddr, config.comm.mac, 6);
	request.option_53.dat = DHCPDISCOVER;
	memcpy (request.option_61.dat, config.comm.mac, 6);

	uint16_t len = sizeof (DHCP_Request);
	memcpy (buf, &request, len);
	return len;
}

void
dhcpc_dispatch (uint8_t *buf, uint16_t size)
{

}
