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

#include "oscmidi_private.h"

nOSC_Item oscmidi_bndl [1];
const char *oscmidi_fmt = "M";

OSCMidi_Msg msg;

const char *midi_str = "/midi";
char midi_fmt [OSCMIDI_MAX+1];

uint8_t oscmidi_keys [BLOB_MAX]; // FIXME we should use something bigger or a hash instead

nOSC_Timestamp oscmidi_timestamp;

uint8_t oscmidi_tok;

void
oscmidi_init ()
{
	nosc_item_message_set (oscmidi_bndl, 0, msg, midi_str, midi_fmt);
	midi_fmt[0] = nOSC_END;
}

void
oscmidi_engine_frame_cb (uint32_t fid, nOSC_Timestamp timestamp, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	uint8_t ch;
	oscmidi_timestamp = timestamp + config.output.offset;

	oscmidi_tok = 0;

	if (nblob_old + nblob_new == 0) // idling
		for (ch=0; ch<0x10; ch++) //TODO check if 0x10 < OSCMIDI_MAX
		{
			nosc_message_set_midi(msg, oscmidi_tok, ch, CONTROL_CHANGE, ALL_NOTES_OFF, 0x0); // send all notes off on all channels
			midi_fmt[oscmidi_tok++] = nOSC_MIDI;
		}

	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = floor (X);
	oscmidi_keys[pos] = key;

	nosc_message_set_midi (msg, oscmidi_tok, ch, NOTE_ON, key, 0x7f);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	uint8_t key = oscmidi_keys[pos];

	nosc_message_set_midi (msg, oscmidi_tok, ch, NOTE_OFF, key, 0x7f);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

void
oscmidi_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint8_t ch = gid % 0xf;
	uint8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = oscmidi_keys[pos];
	uint16_t bend = (X - key) * 170.65 + 0x2000; // := (X - key) / 48.0 * 0x1fff + 0x2000;

	uint16_t eff = y * 0x3fff;

	nosc_message_set_midi (msg, oscmidi_tok, ch, PITCH_BEND, bend & 0x7f, bend >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;

	nosc_message_set_midi (msg, oscmidi_tok, ch, CONTROL_CHANGE, config.oscmidi.effect | LSV, eff & 0x7f);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;

	nosc_message_set_midi (msg, oscmidi_tok, ch, CONTROL_CHANGE, config.oscmidi.effect | MSV, eff >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

CMC_Engine oscmidi_engine = {
	oscmidi_engine_frame_cb,
	oscmidi_engine_on_cb,
	oscmidi_engine_off_cb,
	oscmidi_engine_set_cb
};
