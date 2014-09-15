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

#ifndef _CHIMUTIL_H_
#define _CHIMUTIL_H_

#include <stdint.h>

#include <libmaple/adc.h>

uint_fast8_t ip_part_of_subnet(uint8_t *ip);
void cidr_to_subnet(uint8_t *subnet, uint8_t mask);
uint8_t subnet_to_cidr(uint8_t *subnet);
void broadcast_address(uint8_t *brd, uint8_t *ip, uint8_t *subnet);

void adc_timer_reconfigure(void);
void sync_timer_reconfigure(void);
void dhcpc_timer_reconfigure(void);
void mdns_timer_reconfigure(void);
void ptp_timer_reconfigure(float sec);

void output_enable(uint8_t b);
void config_enable(uint8_t b);
void sntp_enable(uint8_t b);
void ptp_enable(uint8_t b);
void debug_enable(uint8_t b);
void mdns_enable(uint8_t b);
void dhcpc_enable(uint8_t b);

typedef struct _Stop_Watch Stop_Watch;

struct _Stop_Watch {
	const char *id;
	uint16_t thresh;
	uint32_t t0;
	uint32_t ticks;
	uint16_t counter;
};

void stop_watch_start(Stop_Watch *sw);
void stop_watch_stop(Stop_Watch *sw);

uint32_t uid_seed(void);
void uid_str(char *str);

/*
 * str to/from ip, maC, cidr conversions
 */
uint_fast8_t str2mac(const char *str, uint8_t *mac);
void mac2str(uint8_t *mac, char *str);
uint_fast8_t str2ip(const char *str, uint8_t *ip);
uint_fast8_t str2ipCIDR(const char *str, uint8_t *ip, uint8_t *mask);
void ip2str(uint8_t *ip, char *str);
void ip2strCIDR(uint8_t *ip, uint8_t mask, char *str);
uint_fast8_t str2addr(const char *str, uint8_t *ip, uint16_t *port);
void addr2str(uint8_t *ip, uint16_t port, char *str);

/*
 * SLIP encode/decode
 */
size_t slip_encode(uint8_t *buf, size_t len);
size_t slip_decode(uint8_t *buf, size_t len, size_t *size);

#endif // _CHIMUTIL_H_
