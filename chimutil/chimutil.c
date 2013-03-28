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

volatile uint8_t mem2mem_dma_done = 0;

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

uint32_t debug_counter = 0;

void
debug_str (const char *str)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", "is", debug_counter++, str);
	udp_send (config.debug.socket.sock, buf_o_ptr, size);
}

void
debug_int32 (int32_t i)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", "ii", debug_counter++, i);
	udp_send (config.debug.socket.sock, buf_o_ptr, size);
}

void
debug_float (float f)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", "if", debug_counter++, f);
	udp_send (config.debug.socket.sock, buf_o_ptr, size);
}

void
debug_double (double d)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", "id", debug_counter++, d);
	udp_send (config.debug.socket.sock, buf_o_ptr, size);
}

void
debug_timestamp (nOSC_Timestamp t)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", "it", debug_counter++, t);
	udp_send (config.debug.socket.sock, buf_o_ptr, size);
}

/*
 * This function converts the array into one number by multiplying each 5-bits
 * channel numbers by multiplications of 2^5
 */
static uint32_t
_calc_adc_sequence (uint8_t *adc_sequence_array, uint8_t n)
{
	uint8_t i;
  uint32_t adc_sequence=0;

  for (i=0; i<n; i++)
		adc_sequence += adc_sequence_array[i] << (i*5);

  return adc_sequence;
}

void
set_adc_sequence (const adc_dev *dev, uint8_t *seq, uint8_t len)
{
	if (len > 12)
	{
		dev->regs->SQR1 = _calc_adc_sequence (&(seq[12]), len % 12); 
		len -= len % 12;
	}
	if (len > 6)
	{
		dev->regs->SQR2 = _calc_adc_sequence (&(seq[6]), len % 6);
		len -= len % 6;
	}
  dev->regs->SQR3 = _calc_adc_sequence (&(seq[0]), len);
}

void 
output_enable (uint8_t b)
{
	config.output.enabled = b;
	if (config.output.enabled)
	{
		udp_begin (config.output.socket.sock, config.output.socket.port[SRC_PORT], 0);
		udp_set_remote (config.output.socket.sock, config.output.socket.ip, config.output.socket.port[DST_PORT]);
	}
	else
		udp_end (config.output.socket.sock);
}

void 
config_enable (uint8_t b)
{
	timer_pause (config_timer);
	config_timer_reconfigure ();

	config.config.enabled = b;
	if (config.config.enabled)
	{
		udp_begin (config.config.socket.sock, config.config.socket.port[SRC_PORT], 0);
		udp_set_remote (config.config.socket.sock, config.config.socket.ip, config.config.socket.port[DST_PORT]);

		timer_resume (config_timer);
	}
	else
		udp_end (config.config.socket.sock);
}

void 
sntp_enable (uint8_t b)
{
	timer_pause (sntp_timer);
	sntp_timer_reconfigure ();

	config.sntp.enabled = b;
	if (config.sntp.enabled)
	{
		udp_begin (config.sntp.socket.sock, config.sntp.socket.port[SRC_PORT], 0);
		udp_set_remote (config.sntp.socket.sock, config.sntp.socket.ip, config.sntp.socket.port[DST_PORT]);

		timer_resume (sntp_timer);
	}
	else
		udp_end (config.sntp.socket.sock);
}

void 
debug_enable (uint8_t b)
{
	config.debug.enabled = b;
	if (config.debug.enabled)
	{
		udp_begin (config.debug.socket.sock, config.debug.socket.port[SRC_PORT], 0);
		udp_set_remote (config.debug.socket.sock, config.debug.socket.ip, config.debug.socket.port[DST_PORT]);
	}
	else
		udp_end (config.debug.socket.sock);
}

void 
mdns_enable (uint8_t b)
{
	//FIXME handle timer
	config.mdns.enabled = b;
	if (config.mdns.enabled)
	{
		udp_set_remote_har (config.mdns.socket.sock, config.mdns.har);
		udp_set_remote (config.mdns.socket.sock, config.mdns.socket.ip, config.mdns.socket.port[DST_PORT]);
		udp_begin (config.mdns.socket.sock, config.mdns.socket.port[SRC_PORT], 1);
	}
	else
		udp_end (config.mdns.socket.sock);
}

void 
dhcpc_enable (uint8_t b)
{
	//FIXME handle timer
	config.dhcpc.enabled = b;
	if (config.dhcpc.enabled)
	{
		udp_set_remote (config.dhcpc.socket.sock, config.dhcpc.socket.ip, config.dhcpc.socket.port[DST_PORT]);
		udp_begin (config.dhcpc.socket.sock, config.dhcpc.socket.port[SRC_PORT], 0);
	}
	else
		udp_end (config.dhcpc.socket.sock);
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
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/stop_watch", "si", sw->id, sw->ticks * 100 / sw->thresh); // 1 tick = 100 us
		udp_send (config.debug.socket.sock, buf_o_ptr, size);

		sw->ticks = 0;
		sw->counter = 0;
	}
}

uint8_t EUI_32 [4];
uint8_t EUI_48 [6];
uint8_t EUI_64 [8];
char EUI_96_STR [96/8*2+1]; // (96bit/8bit)byte + '\0'

void
eui_init ()
{
	EUI_32[0] =	UID_BASE[11] ^ UID_BASE[7] ^ UID_BASE[3];
	EUI_32[1] =	UID_BASE[10] ^ UID_BASE[6] ^ UID_BASE[2];
	EUI_32[2] =	UID_BASE[9]  ^ UID_BASE[5] ^ UID_BASE[1];
	EUI_32[3] =	UID_BASE[8]  ^ UID_BASE[4] ^ UID_BASE[0];

	EUI_48[0] =	UID_BASE[11] ^ UID_BASE[5];
	EUI_48[1] =	UID_BASE[10] ^ UID_BASE[4];
	EUI_48[2] =	UID_BASE[9]  ^ UID_BASE[3];
	EUI_48[3] =	UID_BASE[8]  ^ UID_BASE[2];
	EUI_48[4] =	UID_BASE[7]  ^ UID_BASE[1];
	EUI_48[5] =	UID_BASE[6]  ^ UID_BASE[0];

	EUI_64[0] = EUI_48[0];
	EUI_64[1] = EUI_48[1];
	EUI_64[2] = EUI_48[2];
	EUI_64[3] = 0xfe;
	EUI_64[4] = 0xff;
	EUI_64[5] = EUI_48[3];
	EUI_64[6] = EUI_48[4];
	EUI_64[7] = EUI_48[5];

	sprintf (EUI_96_STR, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
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
