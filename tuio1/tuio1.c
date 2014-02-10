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
#include <math.h>

#include <chimaera.h>
#include <chimutil.h>

#include "tuio1_private.h"

static const char *profile_str [2] = { "/tuio/2Dobj", "/tuio/_sixya" };
static const char *fseq_str = "fseq";
static const char *set_str = "set";
static const char *alive_str = "alive";

static const char *frm_fmt = "si";
static const char *tok_fmt [2] = {
	"siifffggggg", // "set" s i x y a X Y A m r
	"siifff" // "set" s i x y a
};
static char alv_fmt [BLOB_MAX+2]; // this has a variable string len

static nOSC_Arg frm [2];
static Tuio1_Tok tok [BLOB_MAX];
static nOSC_Arg alv [BLOB_MAX+1];

static nOSC_Item tuio1_bndl [TUIO1_MAX]; // BLOB_MAX + frame + alv
static char tuio1_fmt [TUIO1_MAX+1];

nOSC_Bundle_Item tuio1_osc = {
	.bndl = tuio1_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = tuio1_fmt
};

static uint_fast8_t old_end = BLOB_MAX;
static uint_fast8_t counter = 0;

void
tuio1_init ()
{
	uint_fast8_t i;

	// initialize bundle format
	memset (tuio1_fmt, nOSC_MESSAGE, TUIO1_MAX);
	tuio1_fmt[TUIO1_MAX] = nOSC_TERM;

	// initialize bundle
	char *profile = (char *)profile_str[config.tuio1.custom_profile];
	char *tokfmt = (char *)tok_fmt[config.tuio1.custom_profile];

	nosc_item_message_set (tuio1_bndl, 0, alv, profile, alv_fmt);

	for (i=0; i<BLOB_MAX; i++)
		nosc_item_message_set (tuio1_bndl, i+1, tok[i], profile, tokfmt);

	nosc_item_message_set (tuio1_bndl, BLOB_MAX + 1, frm, profile, (char *)frm_fmt);

	// initialize frame
	nosc_message_set_string (frm, 0, (char *)fseq_str);
	nosc_message_set_int32 (frm, 1, 0);

	// initialize tok
	for (i=0; i<BLOB_MAX; i++)
	{
		nOSC_Message msg = tok[i];

		nosc_message_set_string (msg, 0, (char *)set_str);
		nosc_message_set_int32 (msg, 1, 0);			// sid
		nosc_message_set_int32 (msg, 2, 0);			// gid
		nosc_message_set_float (msg, 3, 0.f);		// x
		nosc_message_set_float (msg, 4, 0.f);		// y
		nosc_message_set_float (msg, 5, 0.f);		// angle, aka pid
	}

	// initialize alv
	nosc_message_set_string (alv, 0, (char *)alive_str);
	alv_fmt[0] = nOSC_STRING;
	for (i=0; i<BLOB_MAX; i++)
	{
		nosc_message_set_int32 (alv, 1+i, 0);
		alv_fmt[1+i] = nOSC_INT32;
	}
	alv_fmt[1+BLOB_MAX] = nOSC_END;
}

static void
tuio1_engine_frame_cb (uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t end)
{
	char *profile = (char *)profile_str[config.tuio1.custom_profile];
	char *tokfmt = (char *)tok_fmt[config.tuio1.custom_profile];

	tuio1_bndl[0].message.path = profile;

	frm[1].i = fid;

	tuio1_osc.tt = offset;

	// first undo previous unlinking at position old_end
	if (old_end < BLOB_MAX)
	{
		// relink alv message
		alv_fmt[1+old_end] = nOSC_INT32;

		nosc_item_message_set (tuio1_bndl, old_end + 1, tok[old_end], profile, tokfmt);

		if (old_end < BLOB_MAX-1)
			nosc_item_message_set (tuio1_bndl, old_end + 2, tok[old_end+1], profile, tokfmt);

		tuio1_fmt[old_end+2] = nOSC_MESSAGE;
	}

	// then unlink at position end
	if (end < BLOB_MAX)
	{
		// unlink alv message
		alv_fmt[1+end] = nOSC_END;

		// prepend frm message
		nosc_item_message_set (tuio1_bndl, end + 1, frm, profile, (char *)frm_fmt);

		// unlink bundle
		tuio1_fmt[end+2] = nOSC_TERM;
	}

	old_end = end;

	counter = 0; // reset token pointer
}

static void
tuio1_engine_token_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg = tok[counter];

	//msg[0].s = (char *)set_str;
	msg[1].i = sid;
	msg[2].i = gid;
	msg[3].f = x;
	msg[4].f = y;
	msg[5].f = pid == 0x80 ? 0.f : M_PI;

	nosc_message_set_int32 (alv, 1+counter, sid);

	counter++; // increase token pointer
}

CMC_Engine tuio1_engine = {
	tuio1_engine_frame_cb,
	tuio1_engine_token_cb,
	NULL,
	tuio1_engine_token_cb
};

/*
 * Config
 */

static uint_fast8_t
_tuio1_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = config_check_bool (path, fmt, argc, args, &config.tuio1.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_tuio1_custom_profile (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_check_bool(path, fmt, argc, args, &config.tuio1.custom_profile);
}

/*
 * Query
 */

static const nOSC_Query_Argument tuio1_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", 1)
};

static const nOSC_Query_Argument tuio1_custom_profile_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", 1)
};

const nOSC_Query_Item tuio1_tree [] = {
	// read-write
	nOSC_QUERY_ITEM_METHOD_RW("enabled", "enable/disable", _tuio1_enabled, tuio1_enabled_args),
	nOSC_QUERY_ITEM_METHOD_RW("custom_profile", "toggle custom profile", _tuio1_custom_profile, tuio1_custom_profile_args),
};
