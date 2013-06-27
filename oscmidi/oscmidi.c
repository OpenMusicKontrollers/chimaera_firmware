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
#include <math.h> // floor

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "oscmidi_private.h"

nOSC_Item oscmidi_bndl [OSCMIDI_MAX];
char oscmidi_fmt [OSCMIDI_MAX+1];

OSCMidi_Msg msgs [OSCMIDI_MAX];

const char *midi_note_on_str = "/midi/note_on";
const char *midi_note_off_str ="/midi/note_off";
const char *midi_pitch_bend_str = "/midi/pitch_bend";
const char *midi_control_change_str = "/midi/control_change";

const char *midi_cmd_2_fmt = "ii";
const char *midi_cmd_3_fmt = "iii";

uint8_t oscmidi_keys [BLOB_MAX]; // FIXME we should use something bigger or a hash instead

nOSC_Timestamp oscmidi_timestamp;

uint8_t oscmidi_tok;

void
oscmidi_init ()
{
	// do nothing
}

void
oscmidi_engine_frame_cb (uint32_t fid, nOSC_Timestamp timestamp, uint8_t nblob_old, uint8_t nblob_new)
{
	uint8_t ch;
	oscmidi_timestamp = timestamp + config.output.offset;

	oscmidi_tok = 0;

	if (nblob_old + nblob_new == 0)
		for (ch=0; ch<16; ch++) {
			nOSC_Message msg = msgs[oscmidi_tok];
			nosc_message_set_int32 (msg, 0, ch);
			nosc_message_set_int32 (msg, 1, 0x7b);
			nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_control_change_str, midi_cmd_2_fmt);
			oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;
		}

	oscmidi_fmt[oscmidi_tok] = nOSC_TERM;
}

void
oscmidi_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = floor (X);
	oscmidi_keys[pos] = key;

	msg = msgs[oscmidi_tok];
	nosc_message_set_int32 (msg, 0, ch);
	nosc_message_set_int32 (msg, 1, key);
	nosc_message_set_int32 (msg, 2, 0x7f);
	nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_note_on_str, midi_cmd_3_fmt);
	oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;

	oscmidi_fmt[oscmidi_tok] = nOSC_TERM;
}

void
oscmidi_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	nOSC_Message msg;
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	uint8_t key = oscmidi_keys[pos];

	msg = msgs[oscmidi_tok];
	nosc_message_set_int32 (msg, 0, ch);
	nosc_message_set_int32 (msg, 1, key);
	nosc_message_set_int32 (msg, 2, 0x00);
	nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_note_off_str, midi_cmd_3_fmt);
	oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;

	oscmidi_fmt[oscmidi_tok] = nOSC_TERM;
}

void
oscmidi_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = oscmidi_keys[pos];
	uint16_t bend = (X - key) * 170.65 + 0x2000; // := (X - key) / 48.0 * 0x1fff + 0x2000;

	uint16_t eff = y * 0x3fff;

	msg = msgs[oscmidi_tok];
	nosc_message_set_int32 (msg, 0, ch);
	nosc_message_set_int32 (msg, 1, bend);
	nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_pitch_bend_str, midi_cmd_2_fmt);
	oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;

	msg = msgs[oscmidi_tok];
	nosc_message_set_int32 (msg, 0, ch);
	nosc_message_set_int32 (msg, 1, config.oscmidi.effect | LSV);
	nosc_message_set_int32 (msg, 2, eff & 0x7f);
	nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_control_change_str, midi_cmd_3_fmt);
	oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;

	msg = msgs[oscmidi_tok];
	nosc_message_set_int32 (msg, 0, ch);
	nosc_message_set_int32 (msg, 1, config.oscmidi.effect | MSV);
	nosc_message_set_int32 (msg, 2, eff >> 7);
	nosc_item_message_set (oscmidi_bndl, oscmidi_tok, msg, midi_control_change_str, midi_cmd_3_fmt);
	oscmidi_fmt[oscmidi_tok++] = nOSC_MESSAGE;

	oscmidi_fmt[oscmidi_tok] = nOSC_TERM;
}

CMC_Engine oscmidi_engine = {
	oscmidi_engine_frame_cb,
	oscmidi_engine_on_cb,
	oscmidi_engine_off_cb,
	oscmidi_engine_set_cb
};
