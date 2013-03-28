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

#ifndef _CHIMUTIL_H_
#define _CHIMUTIL_H_

#include <stdint.h>

#include <libmaple/adc.h>

#include <nosc.h>

void dma_memcpy (uint8_t *dst, uint8_t *src, uint16_t len);

void debug_str (const char *str);
void debug_int32 (int32_t i);
void debug_float (float f);
void debug_double (double d);
void debug_timestamp (nOSC_Timestamp t);

void set_adc_sequence (const adc_dev *dev, uint8_t *seq, uint8_t len);

void adc_timer_reconfigure ();
void sntp_timer_reconfigure ();
void config_timer_reconfigure ();

void output_enable (uint8_t b);
void config_enable (uint8_t b);
void sntp_enable (uint8_t b);
void debug_enable (uint8_t b);
void mdns_enable (uint8_t b);
void dhcpc_enable (uint8_t b);

typedef struct _Stop_Watch Stop_Watch;

struct _Stop_Watch {
	const char *id;
	uint16_t thresh;
	uint32_t t0;
	uint32_t ticks;
	uint16_t counter;
};

void stop_watch_start (Stop_Watch *sw);
void stop_watch_stop (Stop_Watch *sw);

extern uint8_t EUI_32 [4];
extern uint8_t EUI_48 [6];
extern uint8_t EUI_64 [8];
extern char EUI_96_STR [96/8*2+1];
void eui_init ();

#endif
