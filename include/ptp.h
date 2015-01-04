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

#ifndef _PTP_H_
#define _PTP_H_

#include <stdint.h>

#include <netdef.h>
#include <oscquery.h>

extern const OSC_Query_Item ptp_tree [6];

void ptp_reset(void);
int64_t ptp_uptime(void);
void ptp_timestamp_refresh(int64_t tick, OSC_Timetag *now, OSC_Timetag *offset);
void ptp_request(void);
void ptp_dispatch(uint8_t *buf, int64_t tick);

#endif // _PTP_H_
