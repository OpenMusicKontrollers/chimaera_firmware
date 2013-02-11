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

void
scsynth_init ()
{
	//TODO
}

inline void
scsynth_engine_frame_cb (uint32_t fid, uint64_t timestamp, uint8_t end)
{
	nosc_item_term_set (scsynth_bndl, end);
}

inline void
scsynth_engine_token_cb (uint8_t tok, uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	//TODO
	nOSC_Message msg = msgs[tok];

	//nosc_message_set_int32 (msg, 0, sid);
	nosc_message_set_int32 (msg, 0, 1000); //TODO FIXME
	nosc_message_set_int32 (msg, 1, 0);
	nosc_message_set_float (msg, 2, x);
	nosc_message_set_int32 (msg, 3, 1);
	nosc_message_set_float (msg, 4, y);
	nosc_message_set_end (msg, 5); //TODO only do once

	nosc_item_message_set (scsynth_bndl, tok, msgs[0], "/n_set");
}

inline void
scsynth_engine_update_cb (uint8_t tok, CMC_Engine_Update_Type type, uint32_t sid, uint16_t uid, uint16_t tid, float x, float y)
{
	//TODO
	scsynth_engine_token_cb (tok, sid, uid, tid, x, y);

	switch (type)
	{
		case CMC_ENGINE_UPDATE_ON:
			break;
		case CMC_ENGINE_UPDATE_OFF:
			break;
		case CMC_ENGINE_UPDATE_SET:
			break;
	}
}

CMC_Engine scsynth_engine = {
	&config.scsynth.enabled,
	scsynth_engine_frame_cb,
	scsynth_engine_update_cb
};
