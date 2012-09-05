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

Dump dump;

void
dump_init ()
{
	uint8_t i;

	dump.time = nosc_message_add_timestamp (NULL, nOSC_IMMEDIATE);
	dump.adc = nosc_message_add_int32 (dump.time, 0);

	nOSC_Message *ptr = dump.adc;
	for (i=0; i<MUX_MAX; i++)
	{
		dump.sensor[i] = nosc_message_add_int32 (ptr, 0);
		ptr = dump.sensor[i];
	}
}

uint16_t
dump_serialize (uint8_t *buf)
{
	return nosc_message_serialize (dump.time, "/dump", buf);
}

void 
dump_frm_set (timestamp64u_t timestamp, uint8_t adc)
{
	dump.time->arg.t = timestamp;
	dump.adc->arg.i = adc;
}

void 
dump_tok_set (uint8_t sensor, int16_t value)
{
	dump.sensor[sensor]->arg.i = value;
}

