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
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "scsynth_private.h"

SCSynth_Msg msgs [1];
nOSC_Item scsynth_bndl [1+1];
static uint8_t tok = 0;

void
scsynth_init ()
{
	//TODO
}

void
scsynth_engine_frame_cb (uint32_t fid, uint64_t timestamp, uint8_t nblob_old, uint8_t nblob_new)
{
	//nosc_item_term_set (scsynth_bndl, nblob_new + (nblob_new-nblob_old));
	nosc_item_term_set (scsynth_bndl, nblob_new); // FIXME 

	tok = 0; // reset token pointer
}

void
scsynth_engine_on_cb (uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = msgs[tok];

	nosc_message_set_string (msg, 0, "chiminst");
	nosc_message_set_int32 (msg, 1, id);
	nosc_message_set_int32 (msg, 2, 0); // add to HEAD
	nosc_message_set_int32 (msg, 3, 1); // group

	nosc_message_set_string (msg, 4, "gate");
	nosc_message_set_int32 (msg, 5, 1);

	nosc_message_set_end (msg, 6);

	nosc_item_message_set (scsynth_bndl, tok, msgs[tok], "/s_new");

	//tok++; // increase token pointer FIXME
}

void
scsynth_engine_off_cb (uint32_t sid, uint16_t uid, uint16_t tid)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = msgs[tok];

	/*
	nosc_message_set_int32 (msg, 0, id);

	nosc_message_set_string (msg, 1, "gate");
	nosc_message_set_int32 (msg, 2, 0);

	nosc_message_set_end (msg, 3);

	nosc_item_message_set (scsynth_bndl, tok, msgs[tok], "/n_set");
	*/

	nosc_message_set_int32 (msg, 0, id);
	nosc_message_set_end (msg, 1);
	nosc_item_message_set (scsynth_bndl, tok, msgs[tok], "/n_free");

	//tok++; // increase token pointer FIXME
}

void
scsynth_engine_set_cb (uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	uint32_t id = 1000 + sid%1000;

	nOSC_Message msg = msgs[tok];

	nosc_message_set_int32 (msg, 0, id);

	nosc_message_set_int32 (msg, 1, 0);
	nosc_message_set_float (msg, 2, x);

	nosc_message_set_int32 (msg, 3, 1);
	nosc_message_set_float (msg, 4, y);

	nosc_message_set_end (msg, 5);

	nosc_item_message_set (scsynth_bndl, tok, msgs[tok], "/n_set");

	//tok++; // increase token pointer FIXME
}

CMC_Engine scsynth_engine = {
	&config.scsynth.enabled,
	scsynth_engine_frame_cb,
	scsynth_engine_on_cb,
	scsynth_engine_off_cb,
	scsynth_engine_set_cb
};
