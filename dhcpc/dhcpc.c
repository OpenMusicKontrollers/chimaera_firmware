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

#include <string.h>

#include "dhcpc_private.h"

#include <libmaple/systick.h>

#include <wiz.h>
#include <config.h>
#include <sntp.h>
#include <chimutil.h>

// global
DHCPC dhcpc = {
	.state = DISCOVER
};

static uint32_t xid = 0x3903F326; // TODO make this random or based on uid?
static const uint8_t dhcp_message_type_discover [1] = {DHCPDISCOVER};
static uint8_t client_identifier [7] = {1, 0, 0, 0, 0, 0, 0};
static const uint8_t host_name [8] = {'c', 'h', 'i', 'm', 'a', 'e', 'r', 'a'};
static const uint8_t parameter_request_list [2] = {OPTION_SUBNET_MASK, OPTION_ROUTER};

static BOOTP_Option dhcp_discover_options [] = {
	BOOTP_OPTION(OPTION_DHCP_MESSAGE_TYPE, 1,(uint8_t *)dhcp_message_type_discover),
	BOOTP_OPTION(OPTION_CLIENT_IDENTIFIER, 7, client_identifier),
	BOOTP_OPTION(OPTION_HOST_NAME, 8,(uint8_t *)host_name),
	BOOTP_OPTION(OPTION_PARAMETER_REQUEST_LIST, 2,(uint8_t *)parameter_request_list),
	BOOTP_OPTION_END
};

static const uint8_t dhcp_message_type_request [1] = {DHCPREQUEST};
static uint8_t dhcp_request_ip [4] = {0, 0, 0, 0};
static uint8_t dhcp_server_identifier [4] = {0, 0, 0, 0};

static BOOTP_Option dhcp_request_options [] = {
	BOOTP_OPTION(OPTION_DHCP_MESSAGE_TYPE, 1,(uint8_t *)dhcp_message_type_request),
	BOOTP_OPTION(OPTION_DHCP_REQUEST_IP, 4, dhcp_request_ip),
	BOOTP_OPTION(OPTION_DHCP_SERVER_IDENTIFIER, 4, dhcp_server_identifier),
	BOOTP_OPTION_END
};

static const uint8_t dhcp_message_type_decline [1] = {DHCPDECLINE};

static BOOTP_Option dhcp_decline_options [] = {
	BOOTP_OPTION(OPTION_DHCP_MESSAGE_TYPE, 1,(uint8_t *)dhcp_message_type_decline),
	BOOTP_OPTION(OPTION_DHCP_REQUEST_IP, 4,(uint8_t *)dhcp_request_ip),
	BOOTP_OPTION(OPTION_DHCP_SERVER_IDENTIFIER, 4, dhcp_server_identifier),
	BOOTP_OPTION_END
};

static DHCP_Packet_Packed dhcp_packet = {
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
_dhcp_packet_serialize(DHCP_Packet_Packed *packet, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	memcpy(buf_ptr, &packet->bootp, sizeof(BOOTP_Packet));
	buf_ptr += sizeof(BOOTP_Packet);

	memset(buf_ptr, 0, sizeof(BOOTP_Packet_Optionals));
	buf_ptr += sizeof(BOOTP_Packet_Optionals);

	memcpy(buf_ptr, &packet->magic_cookie, 4);
	buf_ptr += 4;

	BOOTP_Option *opt;
	for(opt=packet->options; opt->code!=255; opt++)
	{
		*buf_ptr++ = opt->code;
		*buf_ptr++ = opt->len;
		memcpy(buf_ptr, opt->dat, opt->len);
		buf_ptr += opt->len;
	}
	*buf_ptr++ = 255; // option end

	return buf_ptr-buf;
}

static uint16_t
dhcpc_discover(uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl(++xid);
	dhcp_packet.bootp.secs = hton(secs);

	memcpy(dhcp_packet.bootp.chaddr, config.comm.mac, 6);

	dhcp_packet.options = dhcp_discover_options;

	memcpy(&client_identifier[1], config.comm.mac, 6);

	dhcpc.state = OFFER;

	return _dhcp_packet_serialize(&dhcp_packet, buf);
}

static uint16_t
dhcpc_request(uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl(xid);
	dhcp_packet.bootp.secs = hton(secs);

	memcpy(dhcp_packet.bootp.siaddr, dhcpc.server_ip, 4);
	memcpy(dhcp_packet.bootp.chaddr, config.comm.mac, 6);

	dhcp_packet.options = dhcp_request_options;

	memcpy(dhcp_request_ip, dhcpc.ip, 4);
	memcpy(dhcp_server_identifier, dhcpc.server_ip, 4);

	dhcpc.state = ACK;

	return _dhcp_packet_serialize(&dhcp_packet, buf);
}

static uint16_t
dhcpc_decline(uint8_t *buf, uint16_t secs)
{
	dhcp_packet.bootp.xid = htonl(xid);
	dhcp_packet.bootp.secs = hton(secs);

	memcpy(dhcp_packet.bootp.siaddr, dhcpc.server_ip, 4);
	memcpy(dhcp_packet.bootp.chaddr, config.comm.mac, 6);

	dhcp_packet.options = dhcp_decline_options;

	memcpy(dhcp_request_ip, dhcpc.ip, 4);
	memcpy(dhcp_server_identifier, dhcpc.server_ip, 4);

	dhcpc.state = DISCOVER;

	return _dhcp_packet_serialize(&dhcp_packet, buf);
}

static void
dhcpc_dispatch(uint8_t *buf, uint16_t size)
{
	DHCP_Packet_Unpacked *recv =(DHCP_Packet_Unpacked *)buf;

	// check for magic_cookie
	if(strncmp(recv->magic_cookie, dhcp_packet.magic_cookie, 4))
		return;

	recv->bootp.xid = htonl(recv->bootp.xid);
	recv->bootp.secs = hton(recv->bootp.secs);

	uint32_t *_ip =(uint32_t *)recv->bootp.yiaddr;
	if(*_ip != 0x00000000UL)
		memcpy(dhcpc.ip, recv->bootp.yiaddr, 4);

	uint8_t *buf_ptr = buf + sizeof(DHCP_Packet_Unpacked);
	uint8_t code;
	while( (code = buf_ptr[0]) != OPTION_END)
	{
		uint8_t len = buf_ptr[1];
		uint8_t *dat = &buf_ptr[2];
		switch(code)
		{
			case OPTION_SUBNET_MASK:
				memcpy(dhcpc.subnet_mask, dat, 4);
				break;

			case OPTION_ROUTER:
				memcpy(dhcpc.router_ip, dat, 4);
				break;

			case OPTION_DOMAIN_NAME_SERVER:
				// not used
				break;

			case OPTION_HOST_NAME:
				// not used
				break;

			case OPTION_DOMAIN_NAME:
				// not used
				break;

			case OPTION_DHCP_REQUEST_IP:
				memcpy(dhcpc.ip, dat, 4);
				break;

			case OPTION_IP_ADDRESS_LEASE_TIME:
				memcpy(&dhcpc.leastime, dat, 4);
				dhcpc.leastime = htonl(dhcpc.leastime) / 2; // refresh after half of it
				break;

			case OPTION_DHCP_MESSAGE_TYPE:
				switch(dat[0])
				{
					case DHCPOFFER:
						dhcpc.state = REQUEST;
						break;
					case DHCPACK:
						dhcpc.state = LEASE;
						break;
					case DHCPNAK:
						dhcpc.state = LEASE;
						break;
					default:
						// should never get here
						break;
				}
				break;

			case OPTION_DHCP_SERVER_IDENTIFIER:
				memcpy(dhcpc.server_ip, dat, 4);
				break;

			default:
				//ignore
				break;
		}
		buf_ptr += 2 + len;
	}
}

static void
dhcpc_cb(uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	dhcpc_dispatch(buf, len);
}

uint_fast8_t
dhcpc_claim(uint8_t *ip, uint8_t *gateway, uint8_t *subnet) //TODO migrate to ASIO
{
	uint8_t nil_ip [4] = {0, 0, 0, 0};
	uint8_t broadcast_ip [4] = {255, 255, 255, 255};

	// a DHCP claim is done in broadcast with IP: 0.0.0.0
	wiz_ip_set(nil_ip);
	udp_set_remote(config.dhcpc.socket.sock, broadcast_ip, config.dhcpc.socket.port[DST_PORT]);

	dhcpc.state = DISCOVER;
	dhcpc.delay = 4;
	dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

	uint16_t secs;
	uint16_t len;
	while( (dhcpc.state != CLAIMED) && (dhcpc.state != TIMEOUT))
		switch(dhcpc.state)
		{
			case DISCOVER:
				secs = systick_uptime() / SNTP_SYSTICK_RATE + 1;
				len = dhcpc_discover(BUF_O_OFFSET(buf_o_ptr), secs);
				udp_send(config.dhcpc.socket.sock, BUF_O_BASE(buf_o_ptr), len);
				break;
			case OFFER:
				if(systick_uptime() > dhcpc.timeout) // timeout has occured, prepare to resend
				{
					dhcpc.state = DISCOVER;
					dhcpc.delay *= 2;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

					if(dhcpc.delay > 64) // maximal number of retries reached
						dhcpc.state = TIMEOUT;

					break;
				}

				udp_dispatch(config.dhcpc.socket.sock, BUF_I_BASE(buf_i_ptr), dhcpc_cb);

				if(dhcpc.state == REQUEST) // reset timeout for REQUEST
				{
					dhcpc.delay = 4;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;
				}
				break;
			case REQUEST:
				secs = systick_uptime() / SNTP_SYSTICK_RATE + 1;
				len = dhcpc_request(BUF_O_OFFSET(buf_o_ptr), secs);
				udp_send(config.dhcpc.socket.sock, BUF_O_BASE(buf_o_ptr), len);
				break;
			case ACK:
				if(systick_uptime() > dhcpc.timeout) // timeout has occured, prepare to resend
				{
					dhcpc.state = REQUEST;
					dhcpc.delay *= 2;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

					if(dhcpc.delay > 64) // maximal number of retries reached
						dhcpc.state = TIMEOUT;

					break;
				}

				udp_dispatch(config.dhcpc.socket.sock, BUF_I_BASE(buf_i_ptr), dhcpc_cb);
				break;
			case DECLINE:
				//TODO needs to be tested
				secs = systick_uptime() / SNTP_SYSTICK_RATE + 1;
				len = dhcpc_decline(BUF_O_OFFSET(buf_o_ptr), secs);
				udp_send(config.dhcpc.socket.sock, BUF_O_BASE(buf_o_ptr), len);

				dhcpc.delay = 4;
				dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;
				break;
			case LEASE:
				if(arp_probe(0, dhcpc.ip)) // collision
				{
					// decline IP because of ARP collision
					dhcpc.state = DECLINE;
				}
				else
				{
					arp_announce(0, dhcpc.ip);
					dhcpc.state = CLAIMED;
					memcpy(ip, dhcpc.ip, 4);
					memcpy(gateway, dhcpc.router_ip, 4);
					memcpy(subnet, dhcpc.subnet_mask, 4);

					// reconfigure lease timer and start it
					timer_pause(dhcpc_timer);
					dhcpc_timer_reconfigure();
					timer_resume(dhcpc_timer);

					//FIXME actually, the user should do this before enabling dhcpc, not?
					uint8_t brd [4];
					broadcast_address(brd, ip, subnet);
					memcpy(config.output.osc.socket.ip, brd, 4);
					memcpy(config.config.osc.socket.ip, brd, 4);
					memcpy(config.sntp.socket.ip, brd, 4);
					memcpy(config.debug.osc.socket.ip, brd, 4);
				}
				break;
		}

	if(dhcpc.state == TIMEOUT)
		wiz_ip_set(config.comm.ip); // reset to current IP

	return dhcpc.state == CLAIMED;
}

uint_fast8_t //TODO get rid of duplicated code from dhcpc_claim
dhcpc_refresh()
{
	uint8_t *ip = dhcpc.ip;
	uint8_t *gateway = dhcpc.router_ip;
	uint8_t *subnet = dhcpc.subnet_mask;

	// a DHCP REFRESH is a DHCP REQUEST done in unicast
	udp_set_remote(config.dhcpc.socket.sock, dhcpc.server_ip, config.dhcpc.socket.port[DST_PORT]);

	dhcpc.state = REQUEST;
	dhcpc.delay = 4;
	dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

	uint16_t secs;
	uint16_t len;
	while( (dhcpc.state != CLAIMED) && (dhcpc.state != TIMEOUT))
		switch(dhcpc.state)
		{
			case REQUEST:
				secs = systick_uptime() / SNTP_SYSTICK_RATE + 1;
				len = dhcpc_request(BUF_O_OFFSET(buf_o_ptr), secs);
				udp_send(config.dhcpc.socket.sock, BUF_O_BASE(buf_o_ptr), len);
				break;
			case ACK:
				if(systick_uptime() > dhcpc.timeout) // timeout has occured, prepare to resend
				{
					dhcpc.state = REQUEST;
					dhcpc.delay *= 2;
					dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

					if(dhcpc.delay > 64) // maximal number of retries reached
						dhcpc.state = TIMEOUT;

					break;
				}

				udp_dispatch(config.dhcpc.socket.sock, BUF_I_BASE(buf_i_ptr), dhcpc_cb);
				break;
			case DECLINE:
				//TODO needs to be tested
				secs = systick_uptime() / SNTP_SYSTICK_RATE + 1;
				len = dhcpc_decline(BUF_O_OFFSET(buf_o_ptr), secs);
				udp_send(config.dhcpc.socket.sock, BUF_O_BASE(buf_o_ptr), len);

				dhcpc.delay = 4;
				dhcpc.timeout = systick_uptime() + dhcpc.delay*SNTP_SYSTICK_RATE;

				//TODO if refresh is declined or timed-out, claim a new IP after 7/8 of lease time
				break;
			case LEASE:
				if(arp_probe(0, dhcpc.ip)) // collision
				{
					// decline IP because of ARP collision
					dhcpc.state = DECLINE;
				}
				else
				{
					arp_announce(0, dhcpc.ip);
					dhcpc.state = CLAIMED;
					memcpy(ip, dhcpc.ip, 4);
					memcpy(gateway, dhcpc.router_ip, 4);
					memcpy(subnet, dhcpc.subnet_mask, 4);

					// reconfigure lease timer and start it
					timer_pause(dhcpc_timer);
					dhcpc_timer_reconfigure();
					timer_resume(dhcpc_timer);
				}
				break;
		}

	return dhcpc.state == CLAIMED;
}

/*
 * Config
 */

static uint_fast8_t
_dhcpc_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	// needs a config save and reboot to take action
	return config_check_bool(path, fmt, argc, args, &config.dhcpc.socket.enabled);
}

/*
 * Query
 */

const nOSC_Query_Item dhcpc_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _dhcpc_enabled, config_boolean_args),
};
