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

#include <string.h>

#include "dhcpc_private.h"

#include <libmaple/systick.h>

#include <wiz.h>
#include <config.h>

uint32_t xid = 0x3903F326;

DHCPC dhcpc = {
	.state = DISCOVER
};

const uint8_t dhcp_message_type_discover [1] = {DHCPDISCOVER};
uint8_t client_identifier [7] = {1, 0, 0, 0, 0, 0, 0};
const uint8_t host_name [8] = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a'};
const uint8_t parameter_request_list [4] = {OPTION_SUBNET_MASK, OPTION_ROUTER, OPTION_DOMAIN_NAME, OPTION_DOMAIN_NAME_SERVER};

BOOTP_Option dhcp_discover_options [] = {
	BOOTP_OPTION (OPTION_DHCP_MESSAGE_TYPE, (uint8_t *)dhcp_message_type_discover),
	BOOTP_OPTION (OPTION_CLIENT_IDENTIFIER, client_identifier),
	BOOTP_OPTION (OPTION_HOST_NAME, (uint8_t *)host_name),
	BOOTP_OPTION (OPTION_PARAMETER_REQUEST_LIST, (uint8_t *)parameter_request_list),
	BOOTP_OPTION_END
};

const uint8_t dhcp_message_type_request [1] = {DHCPREQUEST};
uint8_t dhcp_request_ip [4] = {0, 0, 0, 0};
uint8_t dhcp_server_identifier [4] = {0, 0, 0, 0};

BOOTP_Option dhcp_request_options [] = {
	BOOTP_OPTION (OPTION_DHCP_MESSAGE_TYPE, (uint8_t *)dhcp_message_type_request),
	BOOTP_OPTION (OPTION_DHCP_REQUEST_IP, dhcp_request_ip),
	BOOTP_OPTION (OPTION_DHCP_SERVER_IDENTIFIER, dhcp_server_identifier),
	BOOTP_OPTION_END
};

const uint8_t dhcp_message_type_decline [1] = {DHCPDECLINE};

BOOTP_Option dhcp_decline_options [] = {
	BOOTP_OPTION (OPTION_DHCP_MESSAGE_TYPE, (uint8_t *)dhcp_message_type_decline),
	BOOTP_OPTION (OPTION_DHCP_REQUEST_IP, (uint8_t *)dhcp_request_ip),
	BOOTP_OPTION (OPTION_DHCP_SERVER_IDENTIFIER, dhcp_server_identifier),
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

uint16_t
dhcpc_decline (uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl (xid);
	dhcp_packet.bootp.secs = hton (secs);

	memcpy (dhcp_packet.bootp.siaddr, dhcpc.server_ip, 4);
	memcpy (dhcp_packet.bootp.chaddr, config.comm.mac, 6);

	dhcp_packet.options = dhcp_decline_options;

	memcpy (dhcp_request_ip, dhcpc.ip, 4);
	memcpy (dhcp_server_identifier, dhcpc.server_id, 4);

	dhcpc.state = DISCOVER;

	return _dhcp_packet_serialize (&dhcp_packet, buf);
}

void
dhcpc_dispatch (uint8_t *buf, uint16_t size)
{
	DHCP_Packet *recv = (DHCP_Packet *)buf;

	// check for magic_cookie
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

static void
dhcpc_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	dhcpc_dispatch (buf, len);
}

uint8_t
dhcpc_claim (uint8_t *ip, uint8_t *gateway, uint8_t *subnet)
{
	uint8_t nil_ip [4] = {0, 0, 0, 0};
	wiz_ip_set (nil_ip);

	dhcpc.state = DISCOVER;
	dhcpc.delay = 4;
	dhcpc.timeout = systick_uptime() + dhcpc.delay*10000;

	uint16_t secs;
	uint16_t len;
	while ( (dhcpc.state != CLAIMED) && (dhcpc.state != TIMEOUT))
		switch (dhcpc.state)
		{
			case DISCOVER:
				secs = systick_uptime() / 10000 + 1;
				len = dhcpc_discover (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], secs);
				udp_send (config.dhcpc.socket.sock, buf_o_ptr, len);
				break;
			case OFFER:
				if (systick_uptime() > dhcpc.timeout) // timeout has occured, prepare to resend
				{
					dhcpc.state = DISCOVER;
					dhcpc.delay *= 2;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*10000;

					if (dhcpc.delay > 64) // maximal number of retries reached
						dhcpc.state = TIMEOUT;

					break;
				}

				udp_dispatch (config.dhcpc.socket.sock, buf_o_ptr, dhcpc_cb);

				if (dhcpc.state == REQUEST) // reset timeout for REQUEST
				{
					dhcpc.delay = 4;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*10000;
				}
				break;
			case REQUEST:
				secs = systick_uptime() / 10000 + 1;
				len = dhcpc_request (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], secs);
				udp_send (config.dhcpc.socket.sock, buf_o_ptr, len);
				break;
			case ACK:
				if (systick_uptime() > dhcpc.timeout) // timeout has occured, prepare to resend
				{
					dhcpc.state = REQUEST;
					dhcpc.delay *= 2;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*10000;

					if (dhcpc.delay > 64) // maximal number of retries reached
						dhcpc.state = TIMEOUT;

					break;
				}

				udp_dispatch (config.dhcpc.socket.sock, buf_o_ptr, dhcpc_cb);
				break;
			case DECLINE:
				//TODO properly test this
				secs = systick_uptime() / 10000 + 1;
				len = dhcpc_decline (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], secs);
				udp_send (config.dhcpc.socket.sock, buf_o_ptr, len);

				dhcpc.delay = 4;
				dhcpc.timeout = systick_uptime() + dhcpc.delay*10000;
				break;
			case LEASE:
				if (arp_probe (0, dhcpc.ip)) // collision
				{
					// decline IP because of ARP collision
					dhcpc.state = DECLINE;
				}
				else
				{
					arp_announce (0, dhcpc.ip);
					dhcpc.state = CLAIMED;
					memcpy (ip, dhcpc.ip, 4);
					memcpy (gateway, dhcpc.gateway_ip, 4);
					memcpy (subnet, dhcpc.subnet_mask, 4);
				}
				break;
		}

	return dhcpc.state == CLAIMED;
	//FIXME lease renewal
	//FIXME lease time timeout
}
