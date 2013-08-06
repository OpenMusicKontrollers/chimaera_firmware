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

#include <string.h>
#include <math.h> // floor

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "dummy_private.h"

nOSC_Item dummy_bndl [BLOB_MAX];
char dummy_fmt [BLOB_MAX+1];

Dummy_Msg msgs [BLOB_MAX];

const char *dummy_idle_str = "/idle";
const char *dummy_on_str = "/on";
const char *dummy_off_str ="/off";
const char *dummy_set_str ="/set";

const char *dummy_idle_fmt = "";
const char *dummy_on_fmt = "iiiff";
const char *dummy_off_fmt = "iii";
const char *dummy_set_fmt = "iiiff";

nOSC_Timestamp dummy_timestamp;

uint_fast8_t dummy_tok;

void
dummy_init ()
{
	// do nothing
}

void
dummy_engine_frame_cb (uint32_t fid, nOSC_Timestamp timestamp, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	dummy_timestamp = timestamp + config.output.offset;

	dummy_tok = 0;

	if (!(nblob_old + nblob_new))
	{
		nOSC_Message msg;

		msg = msgs[dummy_tok];
		nosc_item_message_set (dummy_bndl, dummy_tok, msg, dummy_idle_str, dummy_idle_fmt);
		dummy_fmt[dummy_tok++] = nOSC_MESSAGE;
	}

	dummy_fmt[dummy_tok] = nOSC_TERM;
}

void
dummy_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;

	msg = msgs[dummy_tok];
	nosc_message_set_int32 (msg, 0, sid);
	nosc_message_set_int32 (msg, 1, gid);
	nosc_message_set_int32 (msg, 2, pid);
	nosc_message_set_float (msg, 3, x);
	nosc_message_set_float (msg, 4, y);
	nosc_item_message_set (dummy_bndl, dummy_tok, msg, dummy_on_str, dummy_on_fmt);
	dummy_fmt[dummy_tok++] = nOSC_MESSAGE;

	dummy_fmt[dummy_tok] = nOSC_TERM;
}

void
dummy_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	nOSC_Message msg;

	msg = msgs[dummy_tok];
	nosc_message_set_int32 (msg, 0, sid);
	nosc_message_set_int32 (msg, 1, gid);
	nosc_message_set_int32 (msg, 2, pid);
	nosc_item_message_set (dummy_bndl, dummy_tok, msg, dummy_off_str, dummy_off_fmt);
	dummy_fmt[dummy_tok++] = nOSC_MESSAGE;

	dummy_fmt[dummy_tok] = nOSC_TERM;
}

void
dummy_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;

	msg = msgs[dummy_tok];
	nosc_message_set_int32 (msg, 0, sid);
	nosc_message_set_int32 (msg, 1, gid);
	nosc_message_set_int32 (msg, 2, pid);
	nosc_message_set_float (msg, 3, x);
	nosc_message_set_float (msg, 4, y);
	nosc_item_message_set (dummy_bndl, dummy_tok, msg, dummy_set_str, dummy_set_fmt);
	dummy_fmt[dummy_tok++] = nOSC_MESSAGE;

	dummy_fmt[dummy_tok] = nOSC_TERM;
}

CMC_Engine dummy_engine = {
	dummy_engine_frame_cb,
	dummy_engine_on_cb,
	dummy_engine_off_cb,
	dummy_engine_set_cb
};
