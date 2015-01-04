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

#ifndef _CMC_H_
#define _CMC_H_

#include <stdint.h>
#include <stdlib.h>

#include <oscquery.h>

#define CMC_NOSCALE 0.0f

#define CMC_NORTH 0x80
#define CMC_SOUTH 0x100
#define CMC_BOTH (CMC_NORTH | CMC_SOUTH)

typedef struct _CMC_Engine CMC_Engine;
typedef struct _CMC_Group CMC_Group;
typedef struct _CMC_Frame_Event CMC_Frame_Event;
typedef struct _CMC_Blob_Event CMC_Blob_Event;

typedef void (*CMC_Engine_Init_Cb)(void);
typedef osc_data_t *(*CMC_Engine_Frame_Cb)(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev);
typedef osc_data_t *(*CMC_Engine_Blob_Cb)(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev);

#ifdef BENCHMARK
#	include <chimutil.h>
extern Stop_Watch sw_engine_process;
#endif

struct _CMC_Engine {
	CMC_Engine_Init_Cb init_cb;
	CMC_Engine_Frame_Cb frame_cb;
	CMC_Engine_Blob_Cb on_cb;
	CMC_Engine_Blob_Cb off_cb;
	CMC_Engine_Blob_Cb set_cb;
	CMC_Engine_Frame_Cb end_cb;
};

struct _CMC_Group {
	float x0, x1;
	float m;
	uint16_t gid;
	uint16_t pid;
};

struct _CMC_Frame_Event {
	uint32_t fid;
	OSC_Timetag now;
	OSC_Timetag offset;
	uint_fast8_t nblob_old;
	uint_fast8_t nblob_new;
};

struct _CMC_Blob_Event {
	uint32_t sid;
	uint16_t gid;
	uint16_t pid;
	float x, y;
	float vx, vy;
	float m;
};

extern CMC_Group *cmc_groups;
extern uint_fast8_t cmc_engines_active;

void cmc_velocity_stiffness_update(uint8_t stiffness);
void cmc_init(void);
osc_data_t *cmc_process(OSC_Timetag now, OSC_Timetag offset, int16_t *rela, osc_data_t *buf, osc_data_t *end);

void cmc_group_clear(void);
void cmc_engines_update(void);

#endif // _CMC_H_
