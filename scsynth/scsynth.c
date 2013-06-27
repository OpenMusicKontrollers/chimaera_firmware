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

nOSC_Item scsynth_early_bndl [BLOB_MAX];
nOSC_Item scsynth_late_bndl [BLOB_MAX];
nOSC_Item scsynth_bndl [2];

char scsynth_early_fmt [BLOB_MAX+1];
char scsynth_late_fmt [BLOB_MAX+1];
char scsynth_fmt [3];

SCSynth_Msg msgs[BLOB_MAX*2];

const char *gate_str = "gate";
const char *out_str = "out";

const char *on_str = "/s_new";
const char *set_str = "/n_set";

const char *on_fmt = "siiisisi";
const char *on_set_fmt = "iififiisi";
const char *off_fmt = "isi";
const char *set_fmt = "iififii";

nOSC_Timestamp scsynth_timestamp;
nOSC_Timestamp tt;

uint8_t early_i = 0;
uint8_t late_i = 0;

void
scsynth_init ()
{
	memset (scsynth_early_fmt, nOSC_MESSAGE, BLOB_MAX);
	scsynth_early_fmt[BLOB_MAX] = nOSC_TERM;

	memset (scsynth_late_fmt, nOSC_MESSAGE, BLOB_MAX);
	scsynth_late_fmt[BLOB_MAX] = nOSC_TERM;

	memset (scsynth_fmt, nOSC_BUNDLE, 2);
	scsynth_fmt[2] = nOSC_TERM;

	scsynth_timestamp = nOSC_IMMEDIATE;
}

void
scsynth_engine_frame_cb (uint32_t fid, nOSC_Timestamp timestamp, uint8_t nblob_old, uint8_t nblob_new)
{
	scsynth_early_fmt[0] = nOSC_TERM;
	scsynth_late_fmt[0] = nOSC_TERM;

	tt = timestamp + config.output.offset;
	early_i = 0;
	late_i = 0;

	nosc_item_bundle_set (scsynth_bndl, 0, scsynth_late_bndl, tt, scsynth_late_fmt);
	scsynth_fmt[0] = nOSC_BUNDLE;
	scsynth_fmt[1] = nOSC_TERM;
}

void
scsynth_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint32_t id;
	nOSC_Message msg;

	id = config.scsynth.offset + sid%config.scsynth.modulo;

	// message to create synth (sent early, e.g immediately)
	msg = msgs[early_i + late_i];
	nosc_message_set_string (msg, 0, cmc_group_name_get (gid)); // synthdef name 
	nosc_message_set_int32 (msg, 1, id);
	nosc_message_set_int32 (msg, 2, config.scsynth.addaction);
	nosc_message_set_int32 (msg, 3, gid); // group id
	nosc_message_set_string (msg, 4, (char *)gate_str);
	nosc_message_set_int32 (msg, 5, 0); // do not start synth yet
	nosc_message_set_string (msg, 6, (char *)out_str);
	nosc_message_set_int32 (msg, 7, gid);

	nosc_item_message_set (scsynth_early_bndl, early_i, msg, (char *)on_str, (char *)on_fmt);
	scsynth_early_fmt[early_i++] = nOSC_MESSAGE;
	scsynth_early_fmt[early_i] = nOSC_TERM;

	// message to start synth (sent late, e.g. with lag)
	msg = msgs[early_i + late_i];
	nosc_message_set_int32 (msg, 0, id);
	nosc_message_set_int32 (msg, 1, 0);
	nosc_message_set_float (msg, 2, x);
	nosc_message_set_int32 (msg, 3, 1);
	nosc_message_set_float (msg, 4, y);
	nosc_message_set_int32 (msg, 5, 2);
	nosc_message_set_int32 (msg, 6, pid);
	nosc_message_set_string (msg, 7, (char *)gate_str);
	nosc_message_set_int32 (msg, 8, 1);

	nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)on_set_fmt);
	scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
	scsynth_late_fmt[late_i] = nOSC_TERM;

	nosc_item_bundle_set (scsynth_bndl, 0, scsynth_early_bndl, nOSC_IMMEDIATE, scsynth_early_fmt);
	nosc_item_bundle_set (scsynth_bndl, 1, scsynth_late_bndl, tt, scsynth_late_fmt);
	scsynth_fmt[0] = nOSC_BUNDLE;
	scsynth_fmt[1] = nOSC_BUNDLE;
	scsynth_fmt[2] = nOSC_TERM;
}

void
scsynth_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	uint32_t id;
	nOSC_Message msg;

	id = config.scsynth.offset + sid%config.scsynth.modulo;

	msg = msgs[early_i + late_i];
	nosc_message_set_int32 (msg, 0, id);
	nosc_message_set_string (msg, 1, (char *)gate_str);
	nosc_message_set_int32 (msg, 2, 0);

	nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)off_fmt);
	scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
	scsynth_late_fmt[late_i] = nOSC_TERM;
}

void
scsynth_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint32_t id;
	nOSC_Message msg;

	id = config.scsynth.offset + sid%config.scsynth.modulo;
	
	msg = msgs[early_i + late_i];
	nosc_message_set_int32 (msg, 0, id);
	nosc_message_set_int32 (msg, 1, 0);
	nosc_message_set_float (msg, 2, x);
	nosc_message_set_int32 (msg, 3, 1);
	nosc_message_set_float (msg, 4, y);
	nosc_message_set_int32 (msg, 5, 2);
	nosc_message_set_int32 (msg, 6, pid);

	nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)set_fmt);
	scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
	scsynth_late_fmt[late_i] = nOSC_TERM;
}

CMC_Engine scsynth_engine = {
	scsynth_engine_frame_cb,
	scsynth_engine_on_cb,
	scsynth_engine_off_cb,
	scsynth_engine_set_cb
};
