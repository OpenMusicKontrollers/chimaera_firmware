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

#ifndef _TUIO2_H_
#define _TUIO2_H_

#include <stdint.h>

#include <netdef.h>

#ifdef __cplusplus
extern "C" {
#endif

void tuio2_init ();

uint16_t tuio2_serialize (uint8_t *buf, uint8_t end, timestamp64u_t offset);

void tuio2_frm_set (uint32_t id, timestamp64u_t timestamp);
void tuio2_frm_long_set (const char *app, uint8_t *addr, uint16_t inst, uint16_t w, uint16_t h);
void tuio2_frm_long_unset ();
void tuio2_tok_set (uint8_t pos, uint32_t S, uint32_t T, float x, float z);

#ifdef __cplusplus
}
#endif

#endif
