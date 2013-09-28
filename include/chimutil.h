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

#ifndef _CHIMUTIL_H_
#define _CHIMUTIL_H_

#include <stdint.h>

#include <libmaple/adc.h>

#include <nosc.h>
#include <wiz.h>

void dma_memcpy (uint8_t *dst, uint8_t *src, uint16_t len);

uint_fast8_t ip_part_of_subnet (uint8_t *ip);
void cidr_to_subnet(uint8_t *subnet, uint8_t mask);
uint8_t subnet_to_cidr(uint8_t *subnet);
void broadcast_address(uint8_t *brd, uint8_t *ip, uint8_t *subnet);

#define DEBUG(...) \
({ \
	if (config.debug.socket.enabled) { \
		uint16_t size; \
		size = nosc_message_vararg_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], "/debug", __VA_ARGS__); \
		udp_send (config.debug.socket.sock, buf_o_ptr, size); \
	} \
})

#define debug_str(s) DEBUG("s", s)
#define debug_int32(i) DEBUG("i", i)
#define debug_float(f) DEBUG("f", f)
#define debug_double(d) DEBUG("d", d)
#define debug_timestamp(t) DEBUG("t", t)

void adc_timer_reconfigure ();
void sntp_timer_reconfigure ();
void dhcpc_timer_reconfigure ();

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

uint32_t uid_seed ();
void uid_str (char *str);

#endif
