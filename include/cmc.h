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

#ifndef _CMC_H_
#define _CMC_H_

#include <stdint.h>
#include <stdlib.h>

#include <nosc.h>
#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CMC_Engine_Frame_Cb) (uint32_t fid, uint64_t timestamp, uint8_t end);
typedef void (*CMC_Engine_Token_Cb) (uint8_t tok, uint32_t sid, uint16_t uid, uint16_t tid, float x, float y);

void cmc_init ();
uint8_t cmc_process (int16_t *rela);

void cmc_engine_update (uint64_t now, CMC_Engine_Frame_Cb frame_cb, CMC_Engine_Token_Cb token_cb);
//TODO cmc_engine(s)_update when more than one engine is used

void cmc_group_clear ();
uint8_t cmc_group_add (uint16_t tid, uint16_t uid, float x0, float x1);
uint8_t cmc_group_set (uint16_t tid, uint16_t uid, float x0, float x1);
uint8_t cmc_group_del (uint16_t tid);

uint8_t *cmc_group_buf_get (uint8_t *size);
uint8_t *cmc_group_buf_set (uint8_t size);

#ifdef __cplusplus
}
#endif

#endif
