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

#include <chimaera.h>

#include "dump_private.h"

static uint32_t frame = 0;
static nOSC_Arg dump_msg [2];

static nOSC_Item dump_bndl [] = {
	nosc_message(dump_msg, "/dump", "ib")
};

static char dump_fmt [] = {
	nOSC_MESSAGE,
	nOSC_TERM
};

nOSC_Bundle_Item dump_osc = {
	.bndl = dump_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = dump_fmt
};

void
dump_init (int32_t size, int16_t *swap)
{
	nosc_message_set_blob (dump_msg, DUMP_BLOB, size, (uint8_t *)swap);
}

__always_inline void
dump_update (nOSC_Timestamp now, nOSC_Timestamp offset)
{
	dump_osc.tt = offset;
	nosc_message_set_int32 (dump_msg, DUMP_FRAME, ++frame);
}
