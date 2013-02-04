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

#ifndef SCSYNTH_H_
#define SCSYNTH_H_

#include <stdint.h>

#include <netdef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern nOSC_Item scsynth_bndl [];

void scsynth_init ();

void scsynth_engine_frame_cb (uint32_t fid, uint64_t timestamp, uint8_t end);
void scsynth_engine_token_cb (uint8_t tok, uint32_t sid, uint16_t uid, uint16_t tid, float x, float y);

#ifdef __cplusplus
}
#endif

#endif
