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

#include <chimaera.h>

#include "dump_private.h"
#include "../nosc/nosc_private.h"
#include "../sntp/sntp_private.h"
#include "../config/config_private.h"

static uint32_t frame = 0;
 
static nOSC_Arg dump_msg [] = {
	nosc_int32 (0), // frame number
	nosc_timestamp (nOSC_IMMEDIATE), // timestamp of sensor array sweep
	nosc_blob (0, NULL), // 16-bit sensor data (network endianess)
	nosc_end
};

static nOSC_Item dump_bndl [] = {
	nosc_message(dump_msg, "/dump"),
	nosc_term
};

inline uint16_t
dump_serialize (uint8_t *buf, uint64_t offset)
{
	return nosc_bundle_serialize (dump_bndl, offset, buf);
}

inline void
dump_update (uint64_t now, int32_t size, int16_t *swap)
{
	dump_msg[DUMP_FRAME].val.i = ++frame;
	dump_msg[DUMP_TIME].val.t = now;

	nosc_message_set_blob (dump_msg, DUMP_BLOB, size, (uint8_t*)swap); //FIXME do this only once!!!
}
