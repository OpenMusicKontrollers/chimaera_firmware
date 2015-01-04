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

#ifndef _SNTP_H_
#define _SNTP_H_

#include <stdint.h>

#include <oscquery.h>

/*
#define SNTP_SYSTICK_RELOAD_VAL 719 // 10us
#define SNTP_SYSTICK_DURATION 0.00001ULLK // 10us
#define SNTP_SYSTICK_RATE 100000
#define SNTP_SYSTICK_US 10
*/

#define SNTP_SYSTICK_RELOAD_VAL (72000 - 1) // 1ms
#define SNTP_SYSTICK_DURATION 0.001ULLK // 1ms
#define SNTP_SYSTICK_RATE 1000 // 1000 Hz
#define SNTP_SYSTICK_US 1000

extern fix_s31_32_t clock_offset;
extern fix_32_32_t roundtrip_delay;
extern const OSC_Query_Item sntp_tree [5];

void sntp_reset(void);
uint32_t sntp_uptime(void);
void sntp_timestamp_refresh(int64_t tick, OSC_Timetag *now, OSC_Timetag *offset);
uint16_t sntp_request(uint8_t *buf, OSC_Timetag t3);
void sntp_dispatch(uint8_t *buf, OSC_Timetag t4);

#endif // _SNTP_H_
