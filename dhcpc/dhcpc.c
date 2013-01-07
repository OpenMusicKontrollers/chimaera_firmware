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

DHCPC dhcpc = {
	.state = DISCOVER
};

static uint8_t dhcp_message_type_discover [1] = {DHCPDISCOVER};
static uint8_t client_identifier [7] = {1, 0, 0, 0, 0, 0, 0};
static uint8_t host_name [8] = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a'};
static uint8_t parameter_request_list [4] = {OPTION_SUBNET_MASK, OPTION_ROUTER, OPTION_DOMAIN_NAME, OPTION_DOMAIN_NAME_SERVER};

static BOOTP_Option dhcp_discover_options [] = {
	BOOTP_OPTION (OPTION_DHCP_MESSAGE_TYPE, dhcp_message_type_discover),
	BOOTP_OPTION (OPTION_CLIENT_IDENTIFIER, client_identifier),
	BOOTP_OPTION (OPTION_HOST_NAME, host_name),
	BOOTP_OPTION (OPTION_PARAMETER_REQUEST_LIST, parameter_request_list),
	BOOTP_OPTION_END
};

static uint8_t dhcp_message_type_request [1] = {DHCPREQUEST};
static uint8_t dhcp_request_ip [4] = {0, 0, 0, 0};
static uint8_t dhcp_server_identifier [4] = {0, 0, 0, 0};

static BOOTP_Option dhcp_request_options [] = {
	BOOTP_OPTION (OPTION_DHCP_MESSAGE_TYPE, dhcp_message_type_request),
	BOOTP_OPTION (OPTION_DHCP_REQUEST_IP, dhcp_request_ip),
	BOOTP_OPTION (OPTION_DHCP_SERVER_IDENTIFIER, dhcp_server_identifier),
	BOOTP_OPTION_END
};

static DHCP_Packet dhcp_packet = {
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

	dhcp_packet.options = dhcp_discover_options;

	memcpy (&client_identifier[1], config.comm.mac, 6);

	dhcpc.state = OFFER;

	return _dhcp_packet_serialize (&dhcp_packet, buf);
}

uint16_t
dhcpc_request (uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl (xid);
	dhcp_packet.bootp.secs = hton (secs);

	memcpy (dhcp_packet.bootp.siaddr, dhcpc.server_ip, 4);
	memcpy (dhcp_packet.bootp.chaddr, config.comm.mac, 6);

	dhcp_packet.options = dhcp_request_options;

	memcpy (dhcp_request_ip, dhcpc.ip, 4);
	memcpy (dhcp_server_identifier, dhcpc.server_id, 4);

	dhcpc.state = ACK;

	return _dhcp_packet_serialize (&dhcp_packet, buf);
}

void
dhcpc_dispatch (uint8_t *buf, uint16_t size)
{
	DHCP_Packet *recv = (DHCP_Packet *)buf;

	// check for magic_cokie
	if (strncmp (recv->magic_cookie, dhcp_packet.magic_cookie, 4))
		return;

	recv->bootp.xid = htonl (recv->bootp.xid);
	recv->bootp.secs = hton (recv->bootp.secs);

	memcpy (dhcpc.ip, recv->bootp.yiaddr, 4);
	memcpy (dhcpc.server_ip, recv->bootp.siaddr, 4);
	memcpy (dhcpc.gateway_ip, recv->bootp.giaddr, 4);

	uint8_t *buf_ptr = buf + sizeof (BOOTP_Packet) + 4; // offset of options
	uint8_t code;
	while ( (code = buf_ptr[0]) != OPTION_END)
	{
		uint8_t len = buf_ptr[1];
		uint8_t *dat = &buf_ptr[2];
		switch (code)
		{
			case OPTION_SUBNET_MASK:
				memcpy (dhcpc.subnet_mask, dat, 4);
				break;

			case OPTION_ROUTER:
				memcpy (dhcpc.router_ip, dat, 4);
				break;

			case OPTION_DOMAIN_NAME_SERVER:
				//TODO
				break;

			case OPTION_HOST_NAME:
				//TODO
				break;

			case OPTION_DOMAIN_NAME:
				//TODO
				break;

			case OPTION_IP_ADDRESS_LEASE_TIME:
				//TODO
				break;

			case OPTION_DHCP_MESSAGE_TYPE:
				switch (dat[0])
				{
					case DHCPOFFER:
						dhcpc.state = REQUEST;
						break;
					case DHCPACK:
						dhcpc.state = LEASE;
						break;
				}
				break;

			case OPTION_DHCP_SERVER_IDENTIFIER:
				memcpy (dhcpc.server_id, dat, 4);
				break;
		}
		buf_ptr += 2 + len;
	}
}
