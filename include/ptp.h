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

#ifndef _PTP_H_
#define _PTP_H_

#include <stdint.h>

#include <netdef.h>
#include <nosc.h>

#define PTP_SYSTICK_DURATION 0.00001LLK // 10us

extern const nOSC_Query_Item ptp_tree [6];

void ptp_reset();
void ptp_timestamp_refresh(uint32_t tick, nOSC_Timestamp *now, nOSC_Timestamp *offset);
uint16_t ptp_request_serialize(uint8_t *buf);
void ptp_request_timestamp(uint32_t tick);
void ptp_dispatch(uint8_t *buf, uint32_t tick);

#endif // _PTP_H_
