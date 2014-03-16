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

#ifndef _CMC_H_
#define _CMC_H_

#include <stdint.h>
#include <stdlib.h>

#include <nosc.h>
#include <config.h>

#define CMC_NOSCALE 0.0f

typedef void(*CMC_Engine_Frame_Cb)(uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t nbob_new);
typedef void(*CMC_Engine_Blob_On_Cb)(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y);
typedef void(*CMC_Engine_Blob_Off_Cb)(uint32_t sid, uint16_t gid, uint16_t pid);
typedef void(*CMC_Engine_Blob_Set_Cb)(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y);

typedef struct _CMC_Engine CMC_Engine;
typedef struct _CMC_Group CMC_Group;

struct _CMC_Engine {
	CMC_Engine_Frame_Cb frame_cb;
	CMC_Engine_Blob_On_Cb on_cb;
	CMC_Engine_Blob_Off_Cb off_cb;
	CMC_Engine_Blob_Set_Cb set_cb;
};

struct _CMC_Group {
	float x0, x1;
	float m;
	uint16_t gid;
	uint16_t pid;
};

extern CMC_Group cmc_groups[];

extern CMC_Engine *engines [];
extern uint_fast8_t cmc_engines_active;

void cmc_init();
uint_fast8_t cmc_process(nOSC_Timestamp now, nOSC_Timestamp offset, int16_t *rela, CMC_Engine **engines);

void cmc_group_clear();
void cmc_engines_update();

#endif // _CMC_H_
