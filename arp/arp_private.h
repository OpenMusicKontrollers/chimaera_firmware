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

#ifndef _ARP_PRIVATE_H_
#define _ARP_PRIVATE_H_

#include <arp.h>

#define MACRAW_TYPE_ARP 0x0806

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4 0x0800
#define ARP_HLEN_ETHERNET 0x06
#define ARP_PLEN_IPV4 0x04

#define ARP_OPER_REQUEST 0x0001
#define ARP_OPER_REPLY 0x0002

#define ARP_PROBE_WAIT           1 // second  (initial random delay)
#define ARP_PROBE_NUM            3 //         (number of probe packets)
#define ARP_PROBE_MIN            1 // second  (minimum delay until repeated probe)
#define ARP_PROBE_MAX            2 // seconds (maximum delay until repeated probe)
#define ARP_ANNOUNCE_WAIT        2 // seconds (delay before announcing)
#define ARP_ANNOUNCE_NUM         2 //         (number of announcement packets)
#define ARP_ANNOUNCE_INTERVAL    2 // seconds (time between announcement packets)
#define ARP_MAX_CONFLICTS       10 //         (max conflicts before rate limiting)
#define ARP_RATE_LIMIT_INTERVAL 60 // seconds (delay between successive attempts)
#define ARP_DEFEND_INTERVAL     10 // seconds (minimum interval between defensive

typedef struct _ARP_Payload ARP_Payload;

struct _ARP_Payload {
	uint16_t htype;		// hardware type(ethernet MAC)
	uint16_t ptype;		// protocol type(IPv4|IPv6)
	uint8_t hlen;			// hardware length(6bytes)
	uint8_t plen;			// protocol length(4bytes for IPv4)
	uint16_t oper;		// operation mode(request|reply)
	uint8_t sha [6];	// source hardware address
	uint8_t spa [4];	// source IP address
	uint8_t tha [6];	// target hardware address
	uint8_t tpa [4];	// target IP address
} __attribute((packed,aligned(2)));

#define ARP_PAYLOAD_SIZE sizeof(ARP_Payload)

#endif // _ARP_PRIVATE_H
