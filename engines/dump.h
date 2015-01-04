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

#ifndef _DUMP_H_
#define _DUMP_H_

#include <cmc.h>
#include <oscquery.h>

extern const OSC_Query_Item dump_tree [1];

osc_data_t * dump_update(osc_data_t *buf, osc_data_t *end, OSC_Timetag now, OSC_Timetag offset, int32_t len, int16_t *swap);

#endif // _DUMP_H_
