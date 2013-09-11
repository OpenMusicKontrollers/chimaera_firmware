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

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

#include "tuio2_private.h"

const char *frm_str = "/tuio2/frm";
const char *tok_str = "/tuio2/tok";
const char *alv_str = "/tuio2/alv";

const char *frm_fmt_short = "it";
const char *frm_fmt_long = "itsiii";
const char *tok_fmt = "iiifff";
char alv_fmt [BLOB_MAX+1]; // this has a variable string len

nOSC_Arg frm [6];
Tuio2_Tok tok [BLOB_MAX];
nOSC_Arg alv [BLOB_MAX];

nOSC_Item tuio2_bndl [TUIO2_MAX]; // BLOB_MAX + frame + alv
char tuio2_fmt [TUIO2_MAX+1];

nOSC_Bundle_Item tuio2_osc = {
	.bndl = tuio2_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = tuio2_fmt
};

uint_fast8_t old_end = BLOB_MAX;
uint_fast8_t counter = 0;

void
tuio2_init ()
{
	uint_fast8_t i;

	// initialize bundle format
	memset (tuio2_fmt, nOSC_MESSAGE, TUIO2_MAX);
	tuio2_fmt[TUIO2_MAX] = nOSC_TERM;

	// initialize bundle
	nosc_item_message_set (tuio2_bndl, 0, frm, (char *)frm_str, (char *)frm_fmt_short);

	for (i=0; i<BLOB_MAX; i++)
		nosc_item_message_set (tuio2_bndl, i+1, tok[i], (char *)tok_str, (char *)tok_fmt);

	nosc_item_message_set (tuio2_bndl, BLOB_MAX + 1, alv, (char *)alv_str, alv_fmt);
	for (i=0; i<BLOB_MAX; i++)

	// initialize frame
	nosc_message_set_int32 (frm, 0, 0);
	nosc_message_set_timestamp (frm, 1, nOSC_IMMEDIATE);

	// long header
	tuio2_long_header_enable (config.tuio.long_header);

	// initialize tok
	for (i=0; i<BLOB_MAX; i++)
	{
		nOSC_Message msg = tok[i];

		nosc_message_set_int32 (msg, 0, 0);			// sid
		nosc_message_set_int32 (msg, 1, 0);			// tuid
		nosc_message_set_int32 (msg, 2, 0);			// gid
		nosc_message_set_float (msg, 3, 0.0);		// x
		nosc_message_set_float (msg, 4, 0.0);		// y
		nosc_message_set_float (msg, 5, 0.0);		// angle
	}

	// initialize alv
	for (i=0; i<BLOB_MAX; i++)
	{
		nosc_message_set_int32 (alv, i, 0);
		alv_fmt[i] = nOSC_INT32;
	}
	alv_fmt[BLOB_MAX] = nOSC_END;
}

void
tuio2_long_header_enable (uint_fast8_t on)
{
	config.tuio.long_header = on;

	if (on)
	{
		// use EUI_64
		//int32_t addr = (EUI_64[0] << 24) | (EUI_64[1] << 16) | (EUI_64[2] << 8) | EUI_64[3];
		//int32_t inst = (EUI_64[4] << 24) | (EUI_64[5] << 16) | (EUI_64[6] << 8) | EUI_64[7];

		// or use EUI_48 + port
		uint16_t port = config.output.socket.port[SRC_PORT];
		int32_t addr = (EUI_48[0] << 24) | (EUI_48[1] << 16) | (EUI_48[2] << 8) | EUI_48[3];
		int32_t inst = (EUI_48[4] << 24) | (EUI_48[5] << 16) | (port & 0xff00) | (port & 0xff);

		nosc_message_set_string (frm, 2, config.name);
		nosc_message_set_int32 (frm, 3, addr);
		nosc_message_set_int32 (frm, 4, inst);
		nosc_message_set_int32 (frm, 5, (SENSOR_N << 16) | 1);

		nosc_item_message_set (tuio2_bndl, 0, frm, (char *)frm_str, (char *)frm_fmt_long);
	}
	else // off
		nosc_item_message_set (tuio2_bndl, 0, frm, (char *)frm_str, (char *)frm_fmt_short);
}

void
tuio2_engine_frame_cb (uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t end)
{
	frm[0].i = fid;
	frm[1].t = now;

	tuio2_osc.tt = offset;

	// first undo previous unlinking at position old_end
	if (old_end < BLOB_MAX)
	{
		// relink alv message
		alv_fmt[old_end] = nOSC_INT32;

		nosc_item_message_set (tuio2_bndl, old_end + 1, tok[old_end], (char *)tok_str, (char *)tok_fmt);

		if (old_end < BLOB_MAX-1)
			nosc_item_message_set (tuio2_bndl, old_end + 2, tok[old_end+1], (char *)tok_str, (char *)tok_fmt);

		tuio2_fmt[old_end+2] = nOSC_MESSAGE;
	}

	// then unlink at position end
	if (end < BLOB_MAX)
	{
		// unlink alv message
		alv_fmt[end] = nOSC_END;

		// prepend alv message
		//nosc_item_message_set (tuio2_bndl, end + 1, tuio2_bndl[BLOB_MAX+1].message.msg, tuio2_bndl[BLOB_MAX+1].message.path, tuio2_bndl[BLOB_MAX+1].message.fmt); TODO remove me
		nosc_item_message_set (tuio2_bndl, end + 1, alv, (char *)alv_str, alv_fmt);

		// unlink bundle
		tuio2_fmt[end+2] = nOSC_TERM;
	}

	old_end = end;

	counter = 0; // reset token pointer
}

void
tuio2_engine_token_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg = tok[counter];

	msg[0].i = sid;
	msg[1].i = pid;
	msg[2].i = gid;
	msg[3].f = x;
	msg[4].f = y;
	//msg[5].f = 0; // not used

	nosc_message_set_int32 (alv, counter, sid);

	counter++; // increase token pointer
}

CMC_Engine tuio2_engine = {
	tuio2_engine_frame_cb,
	tuio2_engine_token_cb,
	NULL,
	tuio2_engine_token_cb
};
