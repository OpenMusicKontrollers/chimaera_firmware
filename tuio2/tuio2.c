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

#include "tuio2_private.h"
#include "config.h"

Tuio2 tuio;
char *frm_str = "/frm";
char *tok_str = "/_STxz";
char *alv_str = "/alv";

void
tuio2_init ()
{
	uint8_t i;

	// initialize bundle
	tuio.bndl[0].path =  frm_str;
	tuio.bndl[0].msg = tuio.frm;

	for (i=0; i<BLOB_MAX; i++)
	{
		tuio.bndl[i + 1].path = tok_str;
		tuio.bndl[i + 1].msg = tuio.tok[i];
	}

	tuio.bndl[BLOB_MAX + 1].path = alv_str;
	tuio.bndl[BLOB_MAX + 1].msg = tuio.alv;

	tuio.bndl[BLOB_MAX + 2].path = NULL;
	tuio.bndl[BLOB_MAX + 2].msg = NULL;

	// initialize frame
	nosc_message_set_int32 (tuio.frm, 0, 0);
	nosc_message_set_timestamp (tuio.frm, 1, nOSC_IMMEDIATE);
	nosc_message_set_end (tuio.frm, 2);

	// initialize tok
	for (i=0; i<BLOB_MAX; i++)
	{
		nOSC_Message msg = tuio.tok[i];

		nosc_message_set_int32 (msg, 0, 0);
		nosc_message_set_int32 (msg, 1, 0);
		nosc_message_set_float (msg, 2, 0.0);
		nosc_message_set_float (msg, 3, 0.0);
		nosc_message_set_end (msg, 4);
	}

	// initialize alv
	for (i=0; i<BLOB_MAX; i++)
		nosc_message_set_int32 (tuio.alv, i, 0);
	nosc_message_set_end (tuio.alv, BLOB_MAX);
}

uint16_t
tuio2_serialize (uint8_t *buf, uint8_t end, uint64_t offset)
{
	// unlink at end pos
	if (end < BLOB_MAX)
	{
		// unlink alv message
		if (end > 0)
			nosc_message_set_end (tuio.alv, end);
		else
		{
			nosc_message_set_nil (tuio.alv, 0);
			nosc_message_set_end (tuio.alv, 1);
		}

		// unlink bundle
		tuio.bndl[end+1].path = tuio.bndl[BLOB_MAX+1].path; // alv
		tuio.bndl[end+1].msg = tuio.bndl[BLOB_MAX+1].msg; // alv

		tuio.bndl[end+2].path = NULL;
		tuio.bndl[end+2].msg = NULL;
	}

	// serialize
	uint16_t size = nosc_bundle_serialize (tuio.bndl, offset, buf);

	// relink at end pos
	if (end < BLOB_MAX)
	{
		// relink alv message
		if (end > 0)
			nosc_message_set_int32 (tuio.alv, end, 0);
		else
		{
			nosc_message_set_int32 (tuio.alv, 0, 0);
			nosc_message_set_int32 (tuio.alv, 1, 0);
		}

		tuio.bndl[end+1].path = tok_str;
		tuio.bndl[end+1].msg = tuio.tok[end];

		if (end < BLOB_MAX-1)
		{
			tuio.bndl[end+2].path = tok_str;
			tuio.bndl[end+2].msg = tuio.tok[end+1];
		}
	}

	return size;
}

void 
tuio2_frm_set (uint32_t id, uint64_t timestamp)
{
	tuio.frm[0].val.i = id;
	tuio.frm[1].val.t = timestamp;
}

void 
tuio2_tok_set (uint8_t pos, uint32_t S, uint32_t T, float x, float z)
{
	nOSC_Message msg = tuio.tok[pos];

	msg[0].val.i = S;
	msg[1].val.i = T;
	msg[2].val.f = x;
	msg[3].val.f = z;

	nosc_message_set_int32 (tuio.alv, pos, S);
}
