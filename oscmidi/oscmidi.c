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

#include <midi.h>
#include <oscmidi.h>

static const char *oscmidi_str = "/midi";
static const char *oscmidi_fmt_4 = "mmmm";
static const char *oscmidi_fmt_3 = "mmm";
static const char *oscmidi_fmt_2 = "mm";
static const char *oscmidi_fmt_1 = "m";

static MIDI_Hash oscmidi_hash [BLOB_MAX];

static osc_data_t *pack;

void
oscmidi_init()
{
	config.oscmidi.mul =(float)0x2000 / config.oscmidi.range;
}

static osc_data_t *
oscmidi_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_item_variable(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_item_variable(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_on_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	buf_ptr = osc_start_item_variable(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, oscmidi_str);
		if(config.oscmidi.effect <= 0xd)
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_4);
		else
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_3);

		uint8_t ch = gid % 0xf;
		float X = config.oscmidi.offset + x*config.oscmidi.range;
		uint8_t key = floor(X);
		midi_add_key(oscmidi_hash, sid, key);

		uint8_t *M;
		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_NOTE_ON;
		M[2] = key;
		M[3] = 0x7f;

		uint16_t bend =(X - key)*config.oscmidi.mul + 0x1fff;
		uint16_t eff = y * 0x3fff;

		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_PITCH_BEND;
		M[2] = bend & 0x7f;
		M[3] = bend >> 7;

		if(config.oscmidi.effect <= 0xd)
		{
			buf_ptr = osc_set_midi_inline(buf_ptr, &M);
			M[0] = ch;
			M[1] = MIDI_STATUS_CONTROL_CHANGE;
			M[2] = config.oscmidi.effect | MIDI_LSV;
			M[3] = eff & 0x7f;
		}

		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_CONTROL_CHANGE;
		M[2] = config.oscmidi.effect | MIDI_MSV;
		M[3] = eff >> 7;
	}
	buf_ptr = osc_end_item_variable(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t * 
oscmidi_engine_off_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	buf_ptr = osc_start_item_variable(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, oscmidi_str);
		buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_1);

		uint8_t ch = gid % 0xf;
		uint8_t key = midi_rem_key(oscmidi_hash, sid);

		uint8_t *M;
		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_NOTE_OFF;
		M[2] = key;
		M[3] = 0x7f;
	}
	buf_ptr = osc_end_item_variable(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_set_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	buf_ptr = osc_start_item_variable(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, oscmidi_str);
		if(config.oscmidi.effect <= 0xd)
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_3);
		else
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_2);

		uint8_t ch = gid % 0xf;
		float X = config.oscmidi.offset + x*config.oscmidi.range;
		uint8_t key = midi_get_key(oscmidi_hash, sid);
		uint16_t bend =(X - key)*config.oscmidi.mul + 0x1fff;
		uint16_t eff = y * 0x3fff;

		uint8_t *M;
		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_PITCH_BEND;
		M[2] = bend & 0x7f;
		M[3] = bend >> 7;

		if(config.oscmidi.effect <= 0xd)
		{
			buf_ptr = osc_set_midi_inline(buf_ptr, &M);
			M[0] = ch;
			M[1] = MIDI_STATUS_CONTROL_CHANGE;
			M[2] = config.oscmidi.effect | MIDI_LSV;
			M[3] = eff & 0x7f;
		}

		buf_ptr = osc_set_midi_inline(buf_ptr, &M);
		M[0] = ch;
		M[1] = MIDI_STATUS_CONTROL_CHANGE;
		M[2] = config.oscmidi.effect | MIDI_MSV;
		M[3] = eff >> 7;
	}
	buf_ptr = osc_end_item_variable(buf_ptr, itm);

	return buf_ptr;
}

CMC_Engine oscmidi_engine = {
	oscmidi_engine_frame_cb,
	oscmidi_engine_on_cb,
	oscmidi_engine_off_cb,
	oscmidi_engine_set_cb,
	oscmidi_engine_end_cb
};

/*
 * Config
 */

static uint_fast8_t
_oscmidi_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.oscmidi.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_oscmidi_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_float(path, fmt, argc, buf, &config.oscmidi.offset);
}

static uint_fast8_t
_oscmidi_range(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res;
	res = config_check_float(path, fmt, argc, buf, &config.oscmidi.range);
	if(res)
		config.oscmidi.mul = (float)0x1fff / config.oscmidi.range;
	return res;
}

static uint_fast8_t
_oscmidi_effect(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_uint8(path, fmt, argc, buf, &config.oscmidi.effect);
}

/*
 * Query
 */

static const OSC_Query_Argument oscmidi_offset_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Notes", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
};

static const OSC_Query_Argument oscmidi_range_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Notes", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
};

static const OSC_Query_Argument oscmidi_effect_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Controller number", OSC_QUERY_MODE_RW, 0, 0x7f, 1),
};

const OSC_Query_Item oscmidi_tree [] = {
	// read-write
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _oscmidi_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("offset", "MIDI key offset", _oscmidi_offset, oscmidi_offset_args),
	OSC_QUERY_ITEM_METHOD("range", "MIDI pitch bend range", _oscmidi_range, oscmidi_range_args),
	OSC_QUERY_ITEM_METHOD("effect", "MIDI vicinity mapping", _oscmidi_effect, oscmidi_effect_args),
};
