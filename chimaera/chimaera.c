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

#include <chimaera.h>
#include <config.h>

uint8_t buf_o[1024]; // general purpose output buffer
uint8_t buf_i[1024]; // general purpose input buffer
CMC *cmc = NULL;

uint32_t debug_counter = 0;

void
debug_str (const char *str)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "is", debug_counter++, str);
	udp_send (config.config.sock, buf_o, size);
}

void
debug_int32 (int32_t i)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "ii", debug_counter++, i);
	udp_send (config.config.sock, buf_o, size);
}

void
debug_float (float f)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf_o, "/debug", "if", debug_counter++, f);
	udp_send (config.config.sock, buf_o, size);
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
