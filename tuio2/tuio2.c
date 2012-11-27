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
#include "../nosc/nosc_private.h"
#include "config.h"

Tuio2 tuio;

void
tuio2_init ()
{
	uint8_t i;

	tuio.bndl[0] = tuio.frm;

	// frm
	tuio.frm_id = nosc_message_add_int32 (NULL, 0);
	tuio.frm_timestamp = nosc_message_add_timestamp (tuio.frm_id, nOSC_IMMEDIATE);
	tuio.frm_app = nosc_message_add_string (tuio.frm_timestamp, "chimaera");
	tuio.frm_addr = nosc_message_add_int32 (tuio.frm_app, 0);
	tuio.frm_inst = nosc_message_add_int32 (tuio.frm_addr, 0);
	tuio.frm_dim = nosc_message_add_int32 (tuio.frm_inst, 0);
	tuio.bndl[0] = nosc_bundle_add_message (NULL, tuio.frm_timestamp, "/tuio2/frm");

	if (!config.tuio.long_header)
		tuio.frm_timestamp->next = NULL;

	// tok
	//tuio.tok = calloc (tuio.len, sizeof (Tuio2_Tok));
	tuio.tok = _tok;
	for (i=0; i<tuio.len; i++)
	{
		Tuio2_Tok *tok = &tuio.tok[i];
		tok->S = nosc_message_add_int32 (NULL, 0);
		tok->T = nosc_message_add_int32 (tok->S, 0);
		tok->x = nosc_message_add_float (tok->T, 0.0);
		tok->z = nosc_message_add_float (tok->x, 0.0);
		tuio.bndl[i+1] = nosc_bundle_add_message (tuio.bndl[i], tok->z, "/tuio2/_STxz");
	}
	
	// alv
	//tuio.alv = calloc (tuio.len, sizeof (nOSC_Message *));
	tuio.alv = _alv;
	for (i=0; i<tuio.len; i++)
	{
		nOSC_Message *prev = i>0 ? tuio.alv[i-1] : NULL;
		tuio.alv[i] = nosc_message_add_int32 (prev, 0);
	}
	tuio.bndl[tuio.len+1] = nosc_bundle_add_message (tuio.bndl[tuio.len], tuio.alv[tuio.len-1], "/tuio2/alv");
}

uint16_t
tuio2_serialize (uint8_t *buf, uint8_t end, uint64_t offset)
{
	// unlink at end pos
	if (end < tuio.len)
	{
		// unlink alv message
		if (end > 0)
			tuio.alv[end-1]->next = NULL;
		else
		{
			tuio.alv[0]->type = nOSC_NIL;
			tuio.alv[0]->next = NULL;
		}

		// unlink bundle
		tuio.bndl[end]->next = tuio.bndl[tuio.len+1];
		tuio.bndl[tuio.len+1]->prev = tuio.bndl[end];
	}

	// serialize
	uint16_t size = nosc_bundle_serialize (tuio.bndl[tuio.len+1], offset, buf);

	// relink at end pos
	if (end < tuio.len)
	{
		// relink alv message
		if (end > 0)
			tuio.alv[end-1]->next = tuio.alv[end];
		else
		{
			tuio.alv[0]->type = nOSC_INT32;
			tuio.alv[0]->next = tuio.alv[1];
		}

		// relink bundle
		tuio.bndl[end]->next = tuio.bndl[end+1];
		tuio.bndl[tuio.len+1]->prev = tuio.bndl[tuio.len];
	}

	return size;
}

void 
tuio2_frm_set (uint32_t id, uint64_t timestamp)
{
	tuio.frm_id->arg.i = id;
	tuio.frm_timestamp->arg.t = timestamp;
}

void 
tuio2_frm_long_set (const char *app, uint8_t *addr, uint16_t inst, uint16_t w, uint16_t h)
{
	uint32_t _addr = (addr[0]<<24) + (addr[1]<<16) + (addr[2]<<8) + addr[3];
	uint32_t _dim = (w<<16) + h;

	tuio.frm_app->arg.s = (char *) app;
	tuio.frm_addr->arg.i = _addr;
	tuio.frm_inst->arg.i = inst;
	tuio.frm_dim->arg.i = _dim;

	tuio.frm_timestamp->next = tuio.frm_app;
	config.tuio.long_header = 1;
}

void
tuio2_frm_long_unset ()
{
	tuio.frm_timestamp->next = NULL;
	config.tuio.long_header = 0;
}

void 
tuio2_tok_set (uint8_t pos, uint32_t S, uint32_t T, float x, float z)
{
	Tuio2_Tok *tok = &tuio.tok[pos];
	tok->S->arg.i = S;
	tok->T->arg.i = T;
	tok->x->arg.f = x;
	tok->z->arg.f = z;

	tuio.alv[pos]->arg.i = S;
}
