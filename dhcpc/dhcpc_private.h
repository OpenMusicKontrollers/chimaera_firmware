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

#ifndef DHCPC_PRIVATE_H
#define DHCPC_PRIVATE_H

#include <dhcpc.h>

#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7

#define BOOTP_OP_REQUEST 1
#define BOOTP_HTYPE_ETHERNET 1
#define BOOTP_HLEN_ETHERNET 6
#define BOOTP_FLAGS_BROADCAST {0x80, 0x00}
#define BOOTP_FLAGS_UNICAST {0x00, 0x00}
#define DHCP_MAGIC_COOKIE {0x63, 0x82, 0x53, 0x63}

#define OPTION_SUBNET_MASK 1
#define OPTION_ROUTER 3
#define OPTION_DOMAIN_NAME_SERVER 6
#define OPTION_HOST_NAME 12
#define OPTION_DOMAIN_NAME 15
#define OPTION_DHCP_REQUEST_IP 50
#define OPTION_IP_ADDRESS_LEASE_TIME 51
#define OPTION_DHCP_MESSAGE_TYPE 53
#define OPTION_DHCP_SERVER_IDENTIFIER 54
#define OPTION_PARAMETER_REQUEST_LIST 55
#define OPTION_CLIENT_IDENTIFIER 61
#define OPTION_END 255

typedef struct _BOOTP_Packet BOOTP_Packet;
typedef struct _BOOTP_Option BOOTP_Option;
typedef struct _DHCP_Packet DHCP_Packet;

struct _BOOTP_Packet {
	uint8_t op;					// 1: request, 0: reply
	uint8_t htype;			// 1: ethernet
	uint8_t hlen;				// 6: MAC/ethernet
	uint8_t hops;				// optional

	uint32_t xid;				// session ID
	uint16_t secs;
	uint8_t flags [2];

	uint8_t ciaddr [4];	// client IP
	uint8_t yiaddr [4]; // own IP
	uint8_t siaddr [4]; // server IP
	uint8_t giaddr [4]; // relay agent IP
	uint8_t chaddr [16];// client MAC

	char sname [64];		// server name [optional] FIXME this is unused, we don't want to waste memory on it!
	char file [128];		// file name [optional]
};

struct _BOOTP_Option {
	uint8_t code;
	uint8_t len;
	uint8_t *dat;
};

#define BOOTP_OPTION(CODE,LEN,DAT) {.code=CODE,.len=LEN,.dat=DAT}
#define BOOTP_OPTION_END {.code=OPTION_END}

struct _DHCP_Packet {
	BOOTP_Packet bootp;
	uint8_t magic_cookie [4];
	BOOTP_Option *options;
};

#endif // DHCPC_PRIVATE_H
