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

uint8_t dat_53 [1] = {DHCPDISCOVER};
uint8_t dat_61 [7] = {1, 0, 0, 0, 0, 0, 0};
uint8_t dat_12 [8] = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a'};
uint8_t dat_55 [4] = {1, 3, 15, 6};

BOOTP_Option dhcp_discover_options [] = {
	BOOTP_OPTION(53, dat_53),
	BOOTP_OPTION(61, dat_61),
	BOOTP_OPTION(12, dat_12),
	BOOTP_OPTION(55, dat_55),
	BOOTP_OPTION_END
};

DHCP_Packet dhcp_packet = {
	.bootp = {
		.op = BOOTP_OP_REQUEST,
		.htype = BOOTP_HTYPE_ETHERNET,
		.hlen = BOOTP_HLEN_ETHERNET,

		.flags = BOOTP_FLAGS_BROADCAST
	},

	.magic_cookie = DHCP_MAGIC_COOKIE,

	.options = dhcp_discover_options
};

static uint16_t
_dhcp_packet_serialize (DHCP_Packet *packet, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	memcpy (buf_ptr, packet, sizeof (BOOTP_Packet) + 4); // bootp + magic_cookie
	buf_ptr += sizeof (BOOTP_Packet) + 4;

	BOOTP_Option *opt;
	for (opt=packet->options; opt->code!=255; opt++)
	{
		*buf_ptr++ = opt->code;
		*buf_ptr++ = opt->len;
		memcpy (buf_ptr, opt->dat, opt->len);
		buf_ptr += opt->len;
	}
	*buf_ptr++ = 255; // option end

	return buf_ptr-buf;
}

uint16_t
dhcpc_discover (uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl (++xid);
	dhcp_packet.bootp.secs = hton (secs);

	memcpy (dhcp_packet.bootp.chaddr, config.comm.mac, 6);
	dat_53[0] = DHCPDISCOVER;
	memcpy (&dat_61[1], config.comm.mac, 6);

	return _dhcp_packet_serialize (&dhcp_packet, buf);
}

void
dhcpc_dispatch (uint8_t *buf, uint16_t size)
{
	DHCP_Packet *recv = (DHCP_Packet *)buf;
	recv->bootp.xid = htonl (recv->bootp.xid);
	recv->bootp.secs = hton (recv->bootp.secs);

	uint8_t *buf_ptr = (uint8_t *)recv->options;
	BOOTP_Option *opt;
	while (buf_ptr-buf < size)
		switch ((opt = (BOOTP_Option *)buf_ptr)->code)
		{
			case 53:
				switch (opt->dat[0])
				{
					case DHCPDECLINE:
						break;
					case DHCPOFFER:
						break;
					case DHCPACK:
						break;
					case DHCPNAK:
						break;
				}
				buf_ptr += 2 + opt->len;
				break;
			case 61:
				buf_ptr += 2 + opt->len;
				break;
			case 12:
				buf_ptr += 2 + opt->len;
				break;
			case 55:
				buf_ptr += 2 + opt->len;
				break;
			case 255:
				buf_ptr += 1;
				break;
		}
}
