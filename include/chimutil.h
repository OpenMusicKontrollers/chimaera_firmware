/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
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
