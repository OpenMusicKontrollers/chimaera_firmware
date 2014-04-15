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

#ifndef _SNTP_H_
#define _SNTP_H_

#include <stdint.h>

#include <netdef.h>
#include <nosc.h>

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
extern const nOSC_Query_Item sntp_tree [5];

void sntp_reset();
uint32_t sntp_uptime();
void sntp_timestamp_refresh(uint32_t tick, nOSC_Timestamp *now, nOSC_Timestamp *offset);
uint16_t sntp_request(uint8_t *buf, nOSC_Timestamp t3);
void sntp_dispatch(uint8_t *buf, nOSC_Timestamp t4);

#endif // _SNTP_H_
