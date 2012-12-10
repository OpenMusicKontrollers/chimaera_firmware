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

Dump dump;
const char *dump_path = "/dump";

void
dump_init ()
{
	uint8_t i;

	nosc_message_set_timestamp (dump, DUMP_TIME, nOSC_IMMEDIATE);
	nosc_message_set_int32 (dump, DUMP_ADC, 0);

	for (i=0; i<MUX_MAX; i++)
		nosc_message_set_int32 (dump, DUMP_SENSOR + i, 0);

	nosc_message_set_end (dump, DUMP_SENSOR + MUX_MAX);
}

uint16_t
dump_serialize (uint8_t *buf)
{
	return nosc_message_serialize (dump, dump_path, buf);
}

void 
dump_frm_set (uint8_t adc, uint64_t now)
{
	dump[DUMP_TIME].val.t = now;
	dump[DUMP_ADC].val.i = adc;
}

void 
dump_tok_set (uint8_t sensor, int16_t value)
{
	dump[DUMP_SENSOR + sensor].val.i = value;
}
