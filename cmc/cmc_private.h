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

typedef struct _CMC_Sensor CMC_Sensor;
typedef struct _CMC_Blob CMC_Blob;

struct _CMC_Sensor {
	float x;
	uint16_t v;
};

struct _CMC_Blob {
	uint32_t sid;
	float x, p;
	uint8_t above_thresh;
	uint8_t ignore;
};

struct _CMC {
	uint8_t I, J;
	uint32_t fid, sid;
	uint8_t n_sensors, max_blobs;
	float d;
	uint16_t bitdepth;
	float _bitdepth; // 1/bitdepth

	uint16_t diff;
	uint16_t thresh0;
	uint16_t thresh1;
	float thresh0_f;
	float _thresh0_f; // 1/(1-thresh0_f)

	CMC_Sensor *sensors;
	CMC_Blob *old_blobs;
	CMC_Blob *new_blobs;

	Tuio2 *tuio;
};

#endif /* CMC_PRIVATE_H */
