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

volatile uint_fast8_t mem2mem_dma_done = 0;

void
_mem2mem_dma_irq (void)
{
	mem2mem_dma_done = 1;
}

void 
dma_memcpy (uint8_t *dst, uint8_t *src, uint16_t len)
{
	mem2mem_tube.tube_src = src;
	mem2mem_tube.tube_dst = dst;
	mem2mem_tube.tube_nr_xfers = len;

	int status = dma_tube_cfg (DMA1, DMA_CH2, &mem2mem_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH2, DMA_PRIORITY_HIGH);
	dma_attach_interrupt (DMA1, DMA_CH2, _mem2mem_dma_irq);

	mem2mem_dma_done = 0;
	dma_enable (DMA1, DMA_CH2);
	while (!mem2mem_dma_done)
		;
	dma_disable (DMA1, DMA_CH2);
}

uint_fast8_t
ip_part_of_subnet (uint8_t *ip)
{
	uint32_t *ip32 = (uint32_t *)ip;
	uint32_t *subnet32 = (uint32_t *)config.comm.subnet;
	uint32_t *comm32 = (uint32_t *)config.comm.ip;

	return (*ip32 & *subnet32) == (*comm32 & *subnet32);
}

void
cidr_to_subnet(uint8_t *subnet, uint8_t mask)
{
	uint32_t *subnet_ptr = (uint32_t *)subnet;
	uint32_t subn = 0xffffffff;
	if(mask == 0)
		subn = 0x0;
	else
		subn &= ~((1UL << (32-mask)) - 1);
	*subnet_ptr = htonl(subn);
}

uint8_t
subnet_to_cidr(uint8_t *subnet)
{
	uint32_t *subnet_ptr = (uint32_t *)subnet;
	uint32_t subn = htonl(*subnet_ptr);
	uint8_t mask;
	if(subn == 0)
		return 0;
	for(mask=0; !(subn & 1); mask++)
		subn = subn >> 1;
	return 32-mask;
}

void
broadcast_address(uint8_t *brd, uint8_t *ip, uint8_t *subnet)
{
	uint32_t *brd_ptr = (uint32_t *)brd;
	uint32_t *ip_ptr = (uint32_t *)ip;
	uint32_t *subnet_ptr = (uint32_t *)subnet;
	*brd_ptr = (*ip_ptr & *subnet_ptr) | (~(*subnet_ptr));
}

void 
output_enable (uint8_t b)
{
	Socket_Config *socket = &config.output.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void 
config_enable (uint8_t b)
{
	Socket_Config *socket = &config.config.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void 
sntp_enable (uint8_t b)
{
	timer_pause (sntp_timer);
	sntp_timer_reconfigure ();

	Socket_Config *socket = &config.sntp.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));

		timer_resume (sntp_timer);
	}
}

void 
debug_enable (uint8_t b)
{
	Socket_Config *socket = &config.debug.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void 
mdns_enable (uint8_t b)
{
	Socket_Config *socket = &config.mdns.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));
	}
}

void 
dhcpc_enable (uint8_t b)
{
	//timer_pause (dhcpc_timer); //todo we don't need this
	//dhcpc_timer_reconfigure (); //TODO we don't need this

	Socket_Config *socket = &config.dhcpc.socket;
	socket->enabled = b;
	udp_end (socket->sock);
	if (socket->enabled)
	{
		udp_set_remote (socket->sock, socket->ip, socket->port[DST_PORT]);
		udp_begin (socket->sock, socket->port[SRC_PORT],
			wiz_is_multicast(socket->ip));

		//timer_resume (dhcpc_timer); //TODO we don't need this
	}
}

void
stop_watch_start (Stop_Watch *sw)
{
	sw->t0 = systick_uptime ();
}

void
stop_watch_stop (Stop_Watch *sw)
{
	sw->ticks += systick_uptime () - sw->t0;
	sw->counter++;

	if (sw->counter > sw->thresh)
	{
		uint16_t size;
		size = nosc_message_vararg_serialize (BUF_O_OFFSET(buf_o_ptr), "/stop_watch", "si", sw->id, sw->ticks * 100 / sw->thresh); // 1 tick = 100 us
		udp_send (config.debug.socket.sock, BUF_O_BASE(buf_o_ptr), size);

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
	sprintf (str, "%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x",
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
