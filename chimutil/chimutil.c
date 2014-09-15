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

#include <libmaple/nvic.h>
#include <libmaple/dma.h>
#include <libmaple/systick.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <chimutil.h>
#include <tube.h>
#include <armfix.h>
#include <dhcpc.h>
#include <config.h>
#include <wiz.h>
#include <sntp.h>
#include <ptp.h>
#include <debug.h>

uint_fast8_t
ip_part_of_subnet(uint8_t *ip)
{
	uint32_t *ip32 = (uint32_t *)ip;
	uint32_t *subnet32 = (uint32_t *)config.comm.subnet;
	uint32_t *comm32 = (uint32_t *)config.comm.ip;

	return (*ip32 & *subnet32) == (*comm32 & *subnet32);
}

void
cidr_to_subnet(uint8_t *subnet, uint8_t mask)
{
	uint32_t *subnet32 = (uint32_t *)subnet;
	uint32_t subn = 0xffffffff;
	if(mask == 0)
		subn = 0x0;
	else
		subn &= ~((1UL << (32-mask)) - 1);
	*subnet32 = htonl(subn);
}

uint8_t
subnet_to_cidr(uint8_t *subnet)
{
	uint32_t *subnet32 = (uint32_t *)subnet;
	uint32_t subn = htonl(*subnet32);
	uint8_t mask;
	if(subn == 0)
		return 0;
	for(mask=0; !(subn & 1); mask++)
		subn = subn >> 1;
	return 32 - mask;
}

void
broadcast_address(uint8_t *brd, uint8_t *ip, uint8_t *subnet)
{
	uint32_t *brd32 = (uint32_t *)brd;
	uint32_t *ip32 = (uint32_t *)ip;
	uint32_t *subnet32 = (uint32_t *)subnet;
	*brd32 = (*ip32 & *subnet32) | (~(*subnet32));
}

void 
output_enable(uint8_t b)
{
	Socket_Config *socket = &config.output.osc.socket;

	socket->enabled = b;

	if(!config.output.osc.mode)
	{
		udp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			udp_begin(socket->sock, socket->port[SRC_PORT],
				wiz_is_multicast(socket->ip));
		}
	}
	else // tcp
	{
		tcp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			tcp_begin(socket->sock, socket->port[SRC_PORT], config.output.osc.server); // TCP client or server?
		}
	}
}

void 
config_enable(uint8_t b)
{
	Socket_Config *socket = &config.config.osc.socket;

	socket->enabled = b;

	if(!config.config.osc.mode)
	{
		udp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			udp_begin(socket->sock, socket->port[SRC_PORT],
				wiz_is_multicast(socket->ip));
		}
	}
	else // tcp
	{
		tcp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			tcp_begin(socket->sock, socket->port[SRC_PORT], config.config.osc.server); // TCP client or server?
		}
	}
}

void 
ptp_enable(uint8_t b)
{
	Socket_Config *event = &config.ptp.event;
	Socket_Config *general = &config.ptp.general;
		
	timer_pause(ptp_timer);

	event->enabled = b;
	general->enabled = b;
	udp_end(event->sock);
	udp_end(general->sock);

	if(event->enabled)
	{
		ptp_reset();

		udp_set_remote(event->sock, event->ip, event->port[DST_PORT]);
		udp_set_remote(general->sock, general->ip, general->port[DST_PORT]);

		udp_begin(event->sock, event->port[SRC_PORT],
			wiz_is_multicast(event->ip));
		udp_begin(general->sock, general->port[SRC_PORT],
			wiz_is_multicast(general->ip));
	}
}

void 
sntp_enable(uint8_t b)
{
	Socket_Config *socket = &config.sntp.socket;

	timer_pause(sync_timer);
	sync_timer_reconfigure();

	socket->enabled = b;
	udp_end(socket->sock);

	if(socket->enabled)
	{
		sntp_reset();

		udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin(socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));

		timer_resume(sync_timer);
	}
}

void 
debug_enable(uint8_t b)
{
	Socket_Config *socket = &config.debug.osc.socket;

	socket->enabled = b;

	if(!config.debug.osc.mode)
	{
		udp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			udp_begin(socket->sock, socket->port[SRC_PORT],
				wiz_is_multicast(socket->ip));
		}
	} else // tcp
	{
		tcp_end(socket->sock);
		if(socket->enabled)
		{
			udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
			tcp_begin(socket->sock, socket->port[SRC_PORT], config.debug.osc.server); // TCP client or server?
		}
	}
}

void 
mdns_enable(uint8_t b)
{
	Socket_Config *socket = &config.mdns.socket;

	socket->enabled = b;
	udp_end(socket->sock);

	if(socket->enabled)
	{
		udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin(socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void 
dhcpc_enable(uint8_t b)
{
	Socket_Config *socket = &config.dhcpc.socket;

	socket->enabled = b;
	udp_end(socket->sock);

	if(socket->enabled)
	{
		udp_set_remote(socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin(socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void
stop_watch_start(Stop_Watch *sw)
{
	sw->t0 = systick_uptime();
}

void
stop_watch_stop(Stop_Watch *sw)
{
	sw->ticks += systick_uptime() - sw->t0;
	sw->counter++;

	if(sw->counter > sw->thresh)
	{
		DEBUG("ssi", "stop_watch", sw->id, sw->ticks * SNTP_SYSTICK_US / sw->thresh); // 1 tick = 100 us

		sw->ticks = 0;
		sw->counter = 0;
	}
}

uint32_t
uid_seed()
{
	union _seed {
		uint32_t i;
		uint8_t b [4];
	} seed;

	seed.b[0] =	UID_BASE[11] ^ UID_BASE[7] ^ UID_BASE[3];
	seed.b[1] =	UID_BASE[10] ^ UID_BASE[6] ^ UID_BASE[2];
	seed.b[2] =	UID_BASE[9]  ^ UID_BASE[5] ^ UID_BASE[1];
	seed.b[3] =	UID_BASE[8]  ^ UID_BASE[4] ^ UID_BASE[0];

	return seed.i;
}

void
uid_str(char *str)
{
	sprintf(str, "%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x",
		UID_BASE[11],
		UID_BASE[10],
		UID_BASE[9],
		UID_BASE[8],
		UID_BASE[7],
		UID_BASE[6],
		UID_BASE[5],
		UID_BASE[4],
		UID_BASE[3],
		UID_BASE[2],
		UID_BASE[1],
		UID_BASE[0]);

}

uint_fast8_t
str2mac(const char *str, uint8_t *mac)
{
	uint16_t smac [6];
	uint_fast8_t res;
	res = sscanf(str, "%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",
		smac, smac+1, smac+2, smac+3, smac+4, smac+5) == 6;
	if(res
		&& (smac[0] < 0x100) && (smac[1] < 0x100) && (smac[2] < 0x100)
		&& (smac[3] < 0x100) && (smac[4] < 0x100) && (smac[5] < 0x100) )
	{
		mac[0] = smac[0];
		mac[1] = smac[1];
		mac[2] = smac[2];
		mac[3] = smac[3];
		mac[4] = smac[4];
		mac[5] = smac[5];
	}
	return res;
}

void
mac2str(uint8_t *mac, char *str)
{
	sprintf(str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint_fast8_t
str2ip(const char *str, uint8_t *ip)
{
	uint16_t sip [4];
	uint_fast8_t res;
	res = sscanf(str, "%hu.%hu.%hu.%hu", // hhu only available in c99
		sip, sip+1, sip+2, sip+3) == 4;
	if(res
		&& (sip[0] < 0x100) && (sip[1] < 0x100) && (sip[2] < 0x100) && (sip[3] < 0x100) )
	{
		ip[0] = sip[0];
		ip[1] = sip[1];
		ip[2] = sip[2];
		ip[3] = sip[3];
	}
	return res;
}

uint_fast8_t
str2ipCIDR(const char *str, uint8_t *ip, uint8_t *mask)
{
	uint16_t sip [4];
	uint16_t smask;
	uint_fast8_t res;
	res = sscanf(str, "%hu.%hu.%hu.%hu/%hu",
		sip, sip+1, sip+2, sip+3, &smask) == 5;
	if(res
		&& (sip[0] < 0x100) && (sip[1] < 0x100) && (sip[2] < 0x100) && (sip[3] < 0x100) && (smask <= 0x20) )
	{
		ip[0] = sip[0];
		ip[1] = sip[1];
		ip[2] = sip[2];
		ip[3] = sip[3];
		*mask = smask;
	}
	return res;
}

void
ip2str(uint8_t *ip, char *str)
{
	sprintf(str, "%hhu.%hhu.%hhu.%hhu",
		ip[0], ip[1], ip[2], ip[3]);
}

void
ip2strCIDR(uint8_t *ip, uint8_t mask, char *str)
{
	sprintf(str, "%hhu.%hhu.%hhu.%hhu/%hhu",
		ip[0], ip[1], ip[2], ip[3], mask);
}

uint_fast8_t
str2addr(const char *str, uint8_t *ip, uint16_t *port)
{
	uint16_t sip [4];
	uint16_t sport;
	uint_fast8_t res;
	res = sscanf(str, "%hu.%hu.%hu.%hu:%hu",
		sip, sip+1, sip+2, sip+3, &sport) == 5;
	if(res
		&& (sip[0] < 0x100) && (sip[1] < 0x100) && (sip[2] < 0x100) && (sip[3] < 0x100) )
	{
		ip[0] = sip[0];
		ip[1] = sip[1];
		ip[2] = sip[2];
		ip[3] = sip[3];
		*port = sport;
	}
	return res;
}

void
addr2str(uint8_t *ip, uint16_t port, char *str)
{
	sprintf(str, "%hhu.%hhu.%hhu.%hhu:%hu",
		ip[0], ip[1], ip[2], ip[3], port);
}

#define SLIP_END					0300	// indicates end of packet
#define SLIP_ESC					0333	// indicates byte stuffing
#define SLIP_END_REPLACE	0334	// ESC ESC_END means END data byte
#define SLIP_ESC_REPLACE	0335	// ESC ESC_ESC means ESC data byte

// inline SLIP encoding
size_t
slip_encode(uint8_t *buf, size_t len)
{
	uint8_t *src;
	uint8_t *end = buf + len;
	uint8_t *dst;

	size_t count = 0;
	for(src=buf; src<end; src++)
		if( (*src == SLIP_END) || (*src == SLIP_ESC) )
			count++;

	src = end - 1;
	dst = end + count;
	*dst-- = SLIP_END;

	while( (src >= buf) && (src != dst) )
	{
		if(*src == SLIP_END)
		{
			*dst-- = SLIP_END_REPLACE;
			*dst-- = SLIP_ESC;
			src--;
		}
		else if(*src == SLIP_ESC)
		{
			*dst-- = SLIP_ESC_REPLACE;
			*dst-- = SLIP_ESC;
			src--;
		}
		else
			*dst-- = *src--;
	}

	return len + count + 1;
}

// inline SLIP decoding
size_t
slip_decode(uint8_t *buf, size_t len, size_t *size)
{
	uint8_t *src = buf;
	uint8_t *end = buf + len;
	uint8_t *dst = buf;

	while(src < end)
	{
		if(*src == SLIP_ESC)
		{
			src++;
			if(*src == SLIP_END_REPLACE)
				*dst++ = SLIP_END;
			else if(*src == SLIP_ESC_REPLACE)
				*dst++ = SLIP_ESC;
			else
				goto error; // neither END_REPLACE nor ESC_REPLACE, this is invalid slip
			src++;
		}
		else if(*src == SLIP_END)
		{
			src++;

			//break;
			*size = dst - buf;
			return src - buf; // return 1 for double SLIP
		}
		else
		{
			if(src != dst)
				*dst = *src;
			src++;
			dst++;
		}
	}

error:

	*size = 0;
	return 0;
}
