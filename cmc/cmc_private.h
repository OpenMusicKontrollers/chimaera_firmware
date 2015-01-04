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

#ifndef _CMC_PRIVATE_H_
#define _CMC_PRIVATE_H_

#include <cmc.h>

#define POLE_NORTH 1
#define POLE_SOUTH 0

#define ENGINE_MAX 6 // tuio1, tuio2, scsynth, oscmidi, dummy, custom

typedef enum {
	CMC_BLOB_INVALID,
	CMC_BLOB_EXISTED_STILL,
	CMC_BLOB_EXISTED_DIRTY,
	CMC_BLOB_IGNORED,
	CMC_BLOB_APPEARED,
	CMC_BLOB_DISAPPEARED
} CMC_Blob_State;

typedef struct _CMC_Filt CMC_Filt;
typedef struct _CMC_Blob CMC_Blob;

struct _CMC_Filt {
	float f1;
	float f11;
};

struct _CMC_Blob {
	uint32_t sid;
	CMC_Group *group;
	float x, y;
	CMC_Filt vx, vy;
	float v, m;
	uint16_t pid;
	uint8_t above_thresh;
	CMC_Blob_State state;
};

#endif // _CMC_PRIVATE_H_ 
