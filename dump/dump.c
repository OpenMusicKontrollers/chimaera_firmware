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

int16_t dump_array [SENSOR_N]; //TODO can we use some other array teomporarily to save memory? those are 288bytes
uint32_t frame = 0;
 
nOSC_Arg dump_msg [] = {
	nosc_int32 (0), // frame number
	nosc_timestamp (nOSC_IMMEDIATE), // timestamp of sensor array sweep
	nosc_blob (sizeof (dump_array), (uint8_t *)dump_array), // 16-bit sensor data (network endianess)
	nosc_end
};

nOSC_Item dump_bndl [] = {
	{"/dump", dump_msg},
	{NULL, NULL}
};

inline uint16_t
dump_serialize (uint8_t *buf, uint64_t offset)
{
	return nosc_bundle_serialize (dump_bndl, offset, buf);
}

inline void 
dump_timestamp_set (uint64_t now)
{
	dump_msg[DUMP_FRAME].val.i = ++frame;
	dump_msg[DUMP_TIME].val.t = now;
}

inline void 
dump_tok_set (uint8_t sensor, int16_t value)
{
	dump_array[sensor] = hton (value); // convert to network byte order
}
