/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

#ifndef CMC_PRIVATE_H
#define CMC_PRIVATE_H

#include <cmc.h>
#include <tuio2.h>
#include <armfix.h>

extern ufix32_t dist [];

typedef struct _CMC_Sensor CMC_Sensor;
typedef struct _CMC_Blob CMC_Blob;
typedef struct _CMC_Group CMC_Group;

struct _CMC_Sensor {
	ufix32_t x;
	uint16_t v;
	uint8_t n; // negative?
};

struct _CMC_Blob {
	uint32_t sid;
	uint16_t uid;
	CMC_Group *group;
	ufix32_t x, p;
	uint8_t above_thresh;
	uint8_t ignore;
};

struct _CMC_Group {
	uint16_t tid;
	uint16_t uid;
	ufix32_t x0, x1;
	ufix32_t m;
	CMC_Group *next;
};

struct _CMC {
	uint8_t I, J;
	uint32_t fid, sid;
	uint8_t n_sensors, max_blobs;
	ufix32_t d;
	uint16_t bitdepth;
	ufix32_t _bitdepth; // 1/bitdepth

	uint16_t diff;
	uint16_t thresh0;
	uint16_t thresh1;
	ufix32_t thresh0_f;
	ufix32_t _thresh0_f;

	CMC_Sensor *sensors;
	CMC_Blob *old_blobs;
	CMC_Blob *new_blobs;
	CMC_Group *groups;
	uint8_t n_groups;

	fix15_t **matrix;

	Tuio2 *tuio;
};

CMC_Group *_cmc_group_new ();
void _cmc_group_free (CMC_Group *group);
CMC_Group *_cmc_group_push (CMC_Group *group, uint16_t tid, uint16_t uid, float x0, float x1);
CMC_Group *_cmc_group_pop (CMC_Group *group);

#endif /* CMC_PRIVATE_H */
