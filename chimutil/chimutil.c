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

#include <chimutil.h>

uint8_t buf_o[768]; // general purpose output buffer //TODO how big?
uint8_t buf_o2[768]; // general purpose output buffer //TODO how big?
uint8_t buf_i[256]; // general purpose input buffer //TODO how big?
CMC *cmc = NULL;

uint32_t debug_counter = 0;

void
debug_str (const char *str)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "is", debug_counter++, str);
	udp_send (config.debug.socket.sock, buf_o, size);
}

void
debug_int32 (int32_t i)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "ii", debug_counter++, i);
	udp_send (config.debug.socket.sock, buf_o, size);
}

void
debug_float (float f)
{
	if (!config.debug.enabled)
		return;
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "if", debug_counter++, f);
	udp_send (config.debug.socket.sock, buf_o, size);
}

void (*adc12_irq_handler) (void) = NULL;

static void
__irq_adc ()
{
	if (adc12_irq_handler)
		adc12_irq_handler ();
}

void
adc12_attach_interrupt (void (*handler) (void))
{
	adc12_irq_handler = handler;
	nvic_irq_enable (NVIC_ADC_1_2);
}

void
adc12_detach_interrupt ()
{
	nvic_irq_disable (NVIC_ADC_1_2);
	adc12_irq_handler = NULL;
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
tuio_enable (uint8_t b)
{
	config.tuio.enabled = b;
	if (config.tuio.enabled)
	{
		udp_begin (config.tuio.socket.sock, config.tuio.socket.port);
		udp_set_remote (config.tuio.socket.sock, config.tuio.socket.ip, config.tuio.socket.port);
	}
}

void 
config_enable (uint8_t b)
{
	config.config.enabled = b;
	if (config.config.enabled)
	{
		udp_begin (config.config.socket.sock, config.config.socket.port);
		udp_set_remote (config.config.socket.sock, config.config.socket.ip, config.config.socket.port);
	}
}

void 
sntp_enable (uint8_t b)
{
	config.sntp.enabled = b;
	if (config.sntp.enabled)
	{
		udp_begin (config.sntp.socket.sock, config.sntp.socket.port);
		udp_set_remote (config.sntp.socket.sock, config.sntp.socket.ip, config.sntp.socket.port);
	}
}

void 
dump_enable (uint8_t b)
{
	config.dump.enabled = b;
	if (config.dump.enabled)
	{
		udp_begin (config.dump.socket.sock, config.dump.socket.port);
		udp_set_remote (config.dump.socket.sock, config.dump.socket.ip, config.dump.socket.port);
	}
}

void 
debug_enable (uint8_t b)
{
	config.debug.enabled = b;
	if (config.debug.enabled)
	{
		udp_begin (config.debug.socket.sock, config.debug.socket.port);
		udp_set_remote (config.debug.socket.sock, config.debug.socket.ip, config.debug.socket.port);
	}
}

void 
rtpmidi_enable (uint8_t b)
{
	config.rtpmidi.enabled = b;
	if (config.rtpmidi.enabled)
	{
		udp_begin (config.rtpmidi.payload.socket.sock, config.rtpmidi.payload.socket.port);
		udp_set_remote (config.rtpmidi.payload.socket.sock, config.rtpmidi.payload.socket.ip, config.rtpmidi.payload.socket.port);

		udp_begin (config.rtpmidi.session.socket.sock, config.rtpmidi.session.socket.port);
		udp_set_remote (config.rtpmidi.session.socket.sock, config.rtpmidi.session.socket.ip, config.rtpmidi.session.socket.port);
	}
}

void 
ping_enable (uint8_t b)
{
	config.ping.enabled = b;
	if (config.ping.enabled)
	{
		udp_begin (config.ping.socket.sock, config.ping.socket.port);
		udp_set_remote (config.ping.socket.sock, config.ping.socket.ip, config.ping.socket.port);
	}
}

void
stop_watch_start (Stop_Watch *sw)
{
	sw->micros -= _micros ();
}

void
stop_watch_stop (Stop_Watch *sw)
{
	sw->micros += _micros ();
	sw->counter++;

	if (sw->counter > 1000)
	{
		uint16_t size;
		size = nosc_message_vararg_serialize (buf_o, "/stop_watch", "si", sw->id, sw->micros/1000);
		udp_send (config.debug.socket.sock, buf_o, size);

		sw->micros = 0;
		sw->counter = 0;
	}
}
