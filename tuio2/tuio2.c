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

#include "tuio2_private.h"
#include "../nosc/nosc_private.h"

Tuio2 *
tuio2_new (uint8_t len)
{
	Tuio2 *tuio = calloc (1, sizeof (Tuio2));

	uint8_t i;

	tuio->len = len;

	// bndl
	tuio->bndl = calloc (len+2, sizeof (nOSC_Bundle *)); // frm + len*tok + alv

	// frm
	tuio->frm_id = nosc_message_add_int32 (NULL, 0);
	tuio->frm_timestamp = nosc_message_add_timestamp (tuio->frm_id, nOSC_IMMEDIATE);
	tuio->bndl[0] = nosc_bundle_add_message (NULL, tuio->frm_timestamp, "/tuio2/frm");

	// tok
	tuio->tok = calloc (len, sizeof (Tuio2_Tok));
	for (i=0; i<len; i++)
	{
		Tuio2_Tok *tok = &tuio->tok[i];
		tok->S = nosc_message_add_int32 (NULL, 0);
		tok->x = nosc_message_add_float (tok->S, 0.0);
		tok->p = nosc_message_add_float (tok->x, 0.0);
		tuio->bndl[i+1] = nosc_bundle_add_message (tuio->bndl[i], tok->p, "/tuio2/_Sxp");
	}
	
	// alv
	tuio->alv = calloc (len, sizeof (nOSC_Message *));
	for (i=0; i<len; i++)
	{
		nOSC_Message *prev = i>0 ? tuio->alv[i-1] : NULL;
		tuio->alv[i] = nosc_message_add_int32 (prev, 0);
	}
	tuio->bndl[len+1] = nosc_bundle_add_message (tuio->bndl[len], tuio->alv[len-1], "/tuio2/alv");

	return tuio;
}

void 
tuio2_free (Tuio2 *tuio)
{
	nosc_bundle_free (tuio->bndl[tuio->len+2]);
	free (tuio->bndl);
	free (tuio->tok);
	free (tuio->alv);
	free (tuio);
}

uint16_t
tuio2_serialize (Tuio2 *tuio, uint8_t *buf, uint8_t end)
{
	// unlink at end pos
	if (end < tuio->len)
	{
		// unlink alv message
		if (end > 0)
			tuio->alv[end-1]->next = NULL;
		else
		{
			tuio->alv[0]->type = nOSC_NIL;
			tuio->alv[0]->next = NULL;
		}

		// unlink bundle
		tuio->bndl[end]->next = tuio->bndl[tuio->len+1];
		tuio->bndl[tuio->len+1]->prev = tuio->bndl[end];
	}

	// serialize
	uint16_t size = nosc_bundle_serialize (tuio->bndl[tuio->len+1], nOSC_IMMEDIATE, buf);

	// relink at end pos
	if (end < tuio->len)
	{
		// relink alv message
		if (end > 0)
			tuio->alv[end-1]->next = tuio->alv[end];
		else
		{
			tuio->alv[0]->type = nOSC_INT32;
			tuio->alv[0]->next = tuio->alv[1];
		}

		// relink bundle
		tuio->bndl[end]->next = tuio->bndl[end+1];
		tuio->bndl[tuio->len+1]->prev = tuio->bndl[tuio->len];
	}

	return size;
}

void 
tuio2_frm_set (Tuio2 *tuio, uint32_t id, timestamp64u_t timestamp)
{
	tuio->frm_id->arg.i = id;
	tuio->frm_timestamp->arg.t = timestamp;
}

void 
tuio2_tok_set (Tuio2 *tuio, uint8_t pos, uint32_t S, float x, float p)
{
	Tuio2_Tok *tok = &tuio->tok[pos];
	tok->S->arg.i = S;
	tok->x->arg.f = x;
	tok->p->arg.f = p;

	tuio->alv[pos]->arg.i = S;
}
