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

#include <string.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "scsynth_private.h"

SCSynth_On_Msg on_msg[1];
SCSynth_Off_Msg off_msg[1];
SCSynth_Free_Msg free_msg[1];
SCSynth_Set_Msg set_msgs [BLOB_MAX];
nOSC_Item free_bndl [1];

nOSC_Item scsynth_bndl [SCSYNTH_MAX]; // because of free_bndl
char scsynth_fmt [SCSYNTH_MAX+1];
static uint8_t set_tok = 0;
static uint8_t tok = 0;

char *inst_str = "chiminst";
char *gate_str = "gate";

char *on_str = "/s_new";
char *free_str = "/n_free";
char *set_str = "/n_set";

char *on_fmt = "siiisi";
char *off_fmt = "isi";
char *free_fmt = "i";
char *set_fmt = "iifif";

char *free_bndl_fmt = "M";

uint64_t tt;

void
scsynth_init ()
{
	// nothing to do
	memset (scsynth_fmt, nOSC_MESSAGE, SCSYNTH_MAX);
	scsynth_fmt[SCSYNTH_MAX] = nOSC_TERM;
}

void
scsynth_engine_frame_cb (uint32_t fid, uint64_t timestamp, uint8_t nblob_old, uint8_t nblob_new)
{
	uint8_t end;

	end = nblob_old > nblob_new ? nblob_new + 2*(nblob_old-nblob_new) : nblob_new; // +1 because of free

	memset (scsynth_fmt, nOSC_MESSAGE, end);
	scsynth_fmt[end] = nOSC_TERM;

	tt = timestamp;
	tok = 0;
	set_tok = 0;
}

void
scsynth_engine_on_cb (uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = on_msg[0];

	nosc_message_set_string (msg, 0, inst_str); // TODO make this configurable
	nosc_message_set_int32 (msg, 1, id);
	nosc_message_set_int32 (msg, 2, 0); // add to HEAD
	nosc_message_set_int32 (msg, 3, 1); // group

	nosc_message_set_string (msg, 4, gate_str);
	nosc_message_set_int32 (msg, 5, 1);

	nosc_item_message_set (scsynth_bndl, tok, msg, on_str, on_fmt);

	tok++;
}

void
scsynth_engine_off_cb (uint32_t sid, uint16_t uid, uint16_t tid)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = off_msg[0];

	nosc_message_set_int32 (msg, 0, id);

	nosc_message_set_string (msg, 1, gate_str);
	nosc_message_set_int32 (msg, 2, 0);

	nosc_item_message_set (scsynth_bndl, tok, msg, set_str, off_fmt);

	tok++;

	msg = free_msg[0];
	nosc_message_set_int32 (msg, 0, id);
	nosc_item_message_set (free_bndl, 0, msg, free_str, free_fmt);

	uint64_t offset = tt + (2ULL << 32); // + 2 seconds
	nosc_item_bundle_set (scsynth_bndl, tok, free_bndl, offset, free_bndl_fmt);
	scsynth_fmt[tok] = nOSC_BUNDLE;

	tok++;
}

void
scsynth_engine_set_cb (uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = set_msgs[set_tok];

	nosc_message_set_int32 (msg, 0, id);

	nosc_message_set_int32 (msg, 1, 0);
	nosc_message_set_float (msg, 2, x);

	nosc_message_set_int32 (msg, 3, 1);
	nosc_message_set_float (msg, 4, y);

	nosc_item_message_set (scsynth_bndl, tok, msg, set_str, set_fmt);

	set_tok++;
	tok++;
}

CMC_Engine scsynth_engine = {
	&config.scsynth.enabled,
	scsynth_engine_frame_cb,
	scsynth_engine_on_cb,
	scsynth_engine_off_cb,
	scsynth_engine_set_cb
};
