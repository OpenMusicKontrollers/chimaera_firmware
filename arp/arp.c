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

#include "arp_private.h"

#include <string.h>
#include <stdio.h>

#include <wiz.h>
#include <config.h>
#include <chimaera.h>
#include <sntp.h>

#include <libmaple/systick.h>

static MACRAW_Header header;
static ARP_Payload payload;
static volatile uint_fast8_t arp_collision = 0;

/*
 * fill macraw packet with ARP payload
 */
static void
_arp_fill(uint16_t oper, uint8_t *src_mac, uint8_t *src_ip, uint8_t *tar_mac, uint8_t *tar_ip)
{
	// fill MACRAW header
	memcpy(header.dst_mac, tar_mac, 6);
	memcpy(header.src_mac, src_mac, 6);
	header.type = hton(MACRAW_TYPE_ARP);

	// fill ARP payload
	payload.htype = hton(ARP_HTYPE_ETHERNET);
	payload.ptype = hton(ARP_PTYPE_IPV4);
	payload.hlen = ARP_HLEN_ETHERNET;
	payload.plen = ARP_PLEN_IPV4;
	payload.oper = hton(oper);

	memcpy(payload.sha, src_mac, 6);
	memcpy(payload.spa, src_ip, 4);
	memcpy(payload.tha, tar_mac, 6);
	memcpy(payload.tpa, tar_ip, 4);
}

/*
 * send and ARP probe with mac and ip
 */
static void
_arp_fill_probe(uint8_t *mac, uint8_t *ip)
{
	_arp_fill(ARP_OPER_REQUEST, mac,(uint8_t *)wiz_nil_ip,(uint8_t *)wiz_broadcast_mac, ip);
}

/*
 * send and ARP announce with mac and ip
 */
static void
_arp_fill_announce(uint8_t *mac, uint8_t *ip)
{
	_arp_fill(ARP_OPER_REQUEST, mac, ip,(uint8_t *)wiz_broadcast_mac, ip);
}

/*
 * callback to handle replies to the ARP probes and announces
 */
static void
arp_reply_cb(uint8_t *buf, uint16_t len, void *data)
{
	(void)len;
	uint8_t *ip = data;

	MACRAW_Header *mac_header =(MACRAW_Header *)buf;

	// check whether this is an ethernet ARP packet
	if(hton(mac_header->type) != MACRAW_TYPE_ARP)
		return;

	ARP_Payload *arp_reply =(ARP_Payload *)(buf + MACRAW_HEADER_SIZE);

	// check whether this is an IPv4 ARP reply
	if( (hton(arp_reply->htype) != ARP_HTYPE_ETHERNET) ||
			(hton(arp_reply->ptype) != ARP_PTYPE_IPV4) ||
			(arp_reply->hlen != ARP_HLEN_ETHERNET) ||
			(arp_reply->plen != ARP_PLEN_IPV4) ||
			(hton(arp_reply->oper) != ARP_OPER_REPLY) )
		return;

	// check whether the ARP is for us
	if(memcmp(arp_reply->tha, config.comm.mac, 6))
		return;

	// check whether the ARP has our requested IP as source -> then there is an ARP probe collision
	if(!memcmp(arp_reply->spa, ip, 4))
		arp_collision = 1;
}

/*
 * return number of systicks representing a random delay in seconds in the range [minsecs:maxsecs]
 */
static uint32_t
_random_ticks(uint32_t minsecs, uint32_t maxsecs)
{
	uint32_t span = maxsecs - minsecs;
	//return SNTP_SYSTICK_RATE*(minsecs +(float)rand() /(RAND_MAX / span));
	return SNTP_SYSTICK_RATE*(minsecs +(float)rand() / RAND_MAX * span);
}

uint_fast8_t
arp_probe(uint8_t sock, uint8_t *ip)
{
	uint32_t tick;
	uint32_t arp_timeout;

	// open socket in MACRAW mode
	macraw_begin(sock, 1);

	_arp_fill_probe(config.comm.mac, ip);

	// initial delay
	tick = systick_uptime();
	arp_timeout = _random_ticks(0, ARP_PROBE_WAIT);
	while(systick_uptime() - tick < arp_timeout)
		; // wait for random delay

	arp_collision = 0;
	uint_fast8_t i;
	for(i=ARP_PROBE_NUM; i>0; i--)
	{
		// serialize MACRAW packet with ARP payload
		memcpy(&BUF_O_BASE(buf_o_ptr)[WIZ_SEND_OFFSET], &header, MACRAW_HEADER_SIZE);
		memcpy(&BUF_O_BASE(buf_o_ptr)[WIZ_SEND_OFFSET + MACRAW_HEADER_SIZE], &payload, ARP_PAYLOAD_SIZE);
		// send packet
		macraw_send(sock, BUF_O_BASE(buf_o_ptr), MACRAW_HEADER_SIZE + ARP_PAYLOAD_SIZE);
		
		// delay between probes
		if(i>1)
		{
			tick = systick_uptime();
			arp_timeout = _random_ticks(ARP_PROBE_MIN, ARP_PROBE_MAX);
			while(!arp_collision && (systick_uptime() - tick < arp_timeout) )
				macraw_dispatch(sock, BUF_O_BASE(buf_o_ptr), arp_reply_cb, ip);
		}
	}

	// delay before first announce
	tick = systick_uptime();
	arp_timeout = SNTP_SYSTICK_RATE*ARP_ANNOUNCE_WAIT;
	while(!arp_collision && (systick_uptime() - tick < arp_timeout) )
		macraw_dispatch(sock, BUF_O_BASE(buf_o_ptr), arp_reply_cb, ip);

	// close MACRAW socket
	macraw_end(sock);

	return arp_collision;
}

void
arp_announce(uint8_t sock, uint8_t *ip)
{
	uint32_t tick;
	uint32_t arp_timeout;

	// open socket in MACRAW mode
	macraw_begin(sock, 1);

	_arp_fill_announce(config.comm.mac, ip);

	uint_fast8_t i;
	for(i=ARP_ANNOUNCE_NUM; i>0; i--)
	{
		// serialize MACRAW packet with ARP payload
		memcpy(&BUF_O_BASE(buf_o_ptr)[WIZ_SEND_OFFSET], &header, MACRAW_HEADER_SIZE);
		memcpy(&BUF_O_BASE(buf_o_ptr)[WIZ_SEND_OFFSET + MACRAW_HEADER_SIZE], &payload, ARP_PAYLOAD_SIZE);
		// send packet
		macraw_send(sock, BUF_O_BASE(buf_o_ptr), MACRAW_HEADER_SIZE + ARP_PAYLOAD_SIZE);

		// delay between announces
		if(i>1)
		{
			tick = systick_uptime();
			arp_timeout = SNTP_SYSTICK_RATE*ARP_ANNOUNCE_INTERVAL;
			while(systick_uptime() - tick < arp_timeout)
				; // wait for random delay
		}
	}
	
	// close MACRAW socket
	macraw_end(sock);
}
