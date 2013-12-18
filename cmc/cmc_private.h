/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <chimaera.h>
#include <cmc.h>
#include <tuio2.h>

#define CMC_NORTH 0x80
#define CMC_SOUTH 0x100
#define CMC_BOTH (CMC_NORTH | CMC_SOUTH)

#define POLE_NORTH 1
#define POLE_SOUTH 0

typedef enum {
	CMC_BLOB_INVALID,
	CMC_BLOB_EXISTED,
	CMC_BLOB_IGNORED,
	CMC_BLOB_APPEARED,
	CMC_BLOB_DISAPPEARED
} CMC_Blob_State;

typedef struct _CMC_Blob CMC_Blob;

struct _CMC_Blob {
	uint32_t sid;
	CMC_Group *group;
	float x, p;
	uint16_t pid;
	uint8_t above_thresh;
	CMC_Blob_State state;
};

#endif /* CMC_PRIVATE_H */
