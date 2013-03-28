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

#include "oscmidi_private.h"

nOSC_Item oscmidi_bndl [1];
char *oscmidi_fmt = "M"; // single message

nOSC_Arg midi_msg [OSCMIDI_MAX];
char midi_fmt [OSCMIDI_MAX+1]; // plus terminating '\0'
char *midi_str = "/midi";

uint8_t oscmidi_keys [BLOB_MAX]; // FIXME we should use something bigger or a hash instead

uint8_t oscmidi_tok;

void
oscmidi_init ()
{
	memset (midi_fmt, nOSC_MIDI, OSCMIDI_MAX);
	midi_fmt[0] = nOSC_TIMESTAMP;
	midi_fmt[OSCMIDI_MAX] = nOSC_END;

	nosc_item_message_set (oscmidi_bndl, 0, midi_msg, midi_str, midi_fmt);
}

void
oscmidi_engine_frame_cb (uint32_t fid, nOSC_Timestamp timestamp, uint8_t nblob_old, uint8_t nblob_new)
{
	nosc_message_set_timestamp (midi_msg, 0, timestamp);
	oscmidi_tok = 1;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, fix_0_32_t x, fix_0_32_t y)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	fix_s31_32_t X = config.oscmidi.offset - 0.5LLK + x * 48.0LLK;
	uint8_t key = X;
	oscmidi_keys[pos] = key;

	nosc_message_set_midi (midi_msg, oscmidi_tok, NOTE_ON | ch, key, 0x7f, 0x00);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;

	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	uint8_t key = oscmidi_keys[pos];

	nosc_message_set_midi (midi_msg, oscmidi_tok, NOTE_OFF | ch, key, 0x00, 0x00);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;

	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, fix_0_32_t x, fix_0_32_t y)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	fix_s31_32_t X = config.oscmidi.offset - 0.5LLK + x * 48.0LLK;
	uint8_t key = oscmidi_keys[pos];
	uint16_t bend = (X - key) * 170.65LLK + 0x2000; // := (X - key) / 48LLK * 0x1fff + 0x2000;

	uint16_t eff = (fix_16_16_t)y * 0x3fff;

	nosc_message_set_midi (midi_msg, oscmidi_tok, PITCH_BEND | ch, bend & 0x7f, bend >> 7, 0x00);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;

	nosc_message_set_midi (midi_msg, oscmidi_tok, CONTROL_CHANGE | ch, config.oscmidi.effect | LSV, eff & 0x7f, 0x00);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;

	nosc_message_set_midi (midi_msg, oscmidi_tok, CONTROL_CHANGE | ch, config.oscmidi.effect | MSV, eff >> 7, 0x00);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;

	midi_fmt[oscmidi_tok] = nOSC_END;
}

CMC_Engine oscmidi_engine = {
	oscmidi_engine_frame_cb,
	oscmidi_engine_on_cb,
	oscmidi_engine_off_cb,
	oscmidi_engine_set_cb
};
