/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include "oscmidi_private.h"

static nOSC_Item oscmidi_bndl [1];
static char oscmidi_fmt [2] = {nOSC_MESSAGE, nOSC_TERM};

nOSC_Bundle_Item oscmidi_osc = {
	.bndl = oscmidi_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = oscmidi_fmt
};

static OSCMidi_Msg msg;

static const char *midi_str = "/midi";
static char midi_fmt [OSCMIDI_MAX+1];

static MIDI_Hash oscmidi_hash [BLOB_MAX];

static uint_fast8_t oscmidi_tok;

void
oscmidi_init()
{
	nosc_item_message_set(oscmidi_bndl, 0, msg, midi_str, midi_fmt);
	midi_fmt[0] = nOSC_END;

	config.oscmidi.mul =(float)0x2000 / config.oscmidi.range;
}

static void
oscmidi_engine_frame_cb(uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	uint8_t ch;
	oscmidi_osc.tt = offset;

	oscmidi_tok = 0;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

static void
oscmidi_engine_on_cb(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint8_t ch = gid % 0xf;
	float X = config.oscmidi.offset + x*config.oscmidi.range;
	uint8_t key = floor(X);
	midi_add_key(oscmidi_hash, sid, key);

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_NOTE_ON, key, 0x7f);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;

	uint16_t bend =(X - key)*config.oscmidi.mul + 0x1fff;
	uint16_t eff = y * 0x3fff;

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;

	if(config.oscmidi.effect <= 0xd)
	{
		nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_CONTROL_CHANGE, config.oscmidi.effect | MIDI_LSV, eff & 0x7f);
		midi_fmt[oscmidi_tok++] = nOSC_MIDI;
		midi_fmt[oscmidi_tok] = nOSC_END;
	}

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_CONTROL_CHANGE, config.oscmidi.effect | MIDI_MSV, eff >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

static void 
oscmidi_engine_off_cb(uint32_t sid, uint16_t gid, uint16_t pid)
{
	uint8_t ch = gid % 0xf;
	uint8_t key = midi_rem_key(oscmidi_hash, sid);

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_NOTE_OFF, key, 0x7f);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

static void
oscmidi_engine_set_cb(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint8_t ch = gid % 0xf;
	float X = config.oscmidi.offset + x*config.oscmidi.range;
	uint8_t key = midi_get_key(oscmidi_hash, sid);
	uint16_t bend =(X - key)*config.oscmidi.mul + 0x1fff;
	uint16_t eff = y * 0x3fff;

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;

	if(config.oscmidi.effect <= 0xd)
	{
		nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_CONTROL_CHANGE, config.oscmidi.effect | MIDI_LSV, eff & 0x7f);
		midi_fmt[oscmidi_tok++] = nOSC_MIDI;
		midi_fmt[oscmidi_tok] = nOSC_END;
	}

	nosc_message_set_midi(msg, oscmidi_tok, ch, MIDI_STATUS_CONTROL_CHANGE, config.oscmidi.effect | MIDI_MSV, eff >> 7);
	midi_fmt[oscmidi_tok++] = nOSC_MIDI;
	midi_fmt[oscmidi_tok] = nOSC_END;
}

CMC_Engine oscmidi_engine = {
	oscmidi_engine_frame_cb,
	oscmidi_engine_on_cb,
	oscmidi_engine_off_cb,
	oscmidi_engine_set_cb
};

/*
 * Config
 */

static uint_fast8_t
_oscmidi_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, args, &config.oscmidi.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_oscmidi_offset(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_check_float(path, fmt, argc, args, &config.oscmidi.offset);
}

static uint_fast8_t
_oscmidi_range(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res;
	res = config_check_float(path, fmt, argc, args, &config.oscmidi.range);
	if(res)
		config.oscmidi.mul = (float)0x1fff / config.oscmidi.range;
	return res;
}

static uint_fast8_t
_oscmidi_effect(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_check_uint8(path, fmt, argc, args, &config.oscmidi.effect);
}

/*
 * Query
 */

static const nOSC_Query_Argument oscmidi_offset_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("Notes", nOSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
};

static const nOSC_Query_Argument oscmidi_range_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("Notes", nOSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
};

static const nOSC_Query_Argument oscmidi_effect_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Controller number", nOSC_QUERY_MODE_RW, 0, 0x7f, 1),
};

const nOSC_Query_Item oscmidi_tree [] = {
	// read-write
	nOSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _oscmidi_enabled, config_boolean_args),
	nOSC_QUERY_ITEM_METHOD("offset", "MIDI key offset", _oscmidi_offset, oscmidi_offset_args),
	nOSC_QUERY_ITEM_METHOD("range", "MIDI pitch bend range", _oscmidi_range, oscmidi_range_args),
	nOSC_QUERY_ITEM_METHOD("effect", "MIDI vicinity mapping", _oscmidi_effect, oscmidi_effect_args),
};
