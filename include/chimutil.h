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

#ifndef _CHIMAERA_H_
#define _CHIMAERA_H_

#include <stdint.h>
#include <libmaple/adc.h>

#include <nosc.h>
#include <cmc.h>
#include <udp.h>
#include <config.h>
#include <sntp.h>
#include <tube.h>
#include <eeprom.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t buf_o[]; // general purpose output buffer
extern uint8_t buf_o2[]; // general purpose output buffer
extern uint8_t buf_i[]; // general purpose input buffer
extern CMC *cmc;

void debug_str (const char *str);
void debug_int32 (int32_t i);
void debug_float (float f);

void adc12_attach_interrupt (void (*handler) (void));
void adc12_detach_interrupt ();
void set_adc_sequence (const adc_dev *dev, uint8_t *seq, uint8_t len);

void adc_timer_pause ();
void adc_timer_reconfigure ();
void adc_timer_resume ();

void sntp_timer_pause ();
void sntp_timer_reconfigure ();
void sntp_timer_resume ();

void config_timer_pause ();
void config_timer_reconfigure ();
void config_timer_resume ();

void tuio_enable (uint8_t b);
void config_enable (uint8_t b);
void sntp_enable (uint8_t b);
void dump_enable (uint8_t b);
void debug_enable (uint8_t b);
void ping_enable (uint8_t b);

typedef struct _Stop_Watch Stop_Watch;

struct _Stop_Watch {
	const char *id;
	int32_t micros;
	uint16_t counter;
};

uint32_t _micros ();
void stop_watch_start (Stop_Watch *sw);
void stop_watch_stop (Stop_Watch *sw);

#ifdef __cplusplus
}
#endif

#endif
