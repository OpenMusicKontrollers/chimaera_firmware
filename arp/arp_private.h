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

#ifndef _ARP_PRIVATE_H
#define _ARP_PRIVATE_H

#include <arp.h>

#define MACRAW_TYPE_ARP 0x0806

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4 0x0800
#define ARP_HLEN_ETHERNET 0x06
#define ARP_PLEN_IPV4 0x04

#define ARP_OPER_REQUEST 0x0001
#define ARP_OPER_REPLY 0x0002

#define ARP_PROBE_WAIT           1 // second   (initial random delay)
#define ARP_PROBE_NUM            3 //          (number of probe packets)
#define ARP_PROBE_MIN            1 // second   (minimum delay until repeated probe)
#define ARP_PROBE_MAX            2 // seconds  (maximum delay until repeated probe)
#define ARP_ANNOUNCE_WAIT        2 // seconds  (delay before announcing)
#define ARP_ANNOUNCE_NUM         2 //          (number of announcement packets)
#define ARP_ANNOUNCE_INTERVAL    2 // seconds  (time between announcement packets)
#define ARP_MAX_CONFLICTS       10 //          (max conflicts before rate limiting)
#define ARP_RATE_LIMIT_INTERVAL 60 // seconds  (delay between successive attempts)
#define ARP_DEFEND_INTERVAL     10 // seconds  (minimum interval between defensive

typedef struct _ARP_Payload ARP_Payload;

struct _ARP_Payload {
	uint16_t htype;		// hardware type (ethernet MAC)
	uint16_t ptype;		// protocol type (IPv4|IPv6)
	uint8_t hlen;			// hardware length (6bytes)
	uint8_t plen;			// protocol length (4bytes for IPv4)
	uint16_t oper;		// operation mode (request|reply)
	uint8_t sha [6];	// source hardware address
	uint8_t spa [4];	// source IP address
	uint8_t tha [6];	// target hardware address
	uint8_t tpa [4];	// target IP address
} __attribute((packed,aligned(2)));

#define ARP_PAYLOAD_SIZE sizeof(ARP_Payload)

#endif
