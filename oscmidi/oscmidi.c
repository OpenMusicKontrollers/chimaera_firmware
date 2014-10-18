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
#include <stdio.h>
#include <math.h> // floor

#include <config.h>
#include <midi.h>
#include <oscmidi.h>

OSC_MIDI_Group *oscmidi_groups = config.oscmidi_groups;

static float mul [GROUP_MAX];

static const char *oscmidi_fmt_4 [] = {
	[OSC_MIDI_FORMAT_MIDI] = "mmmm",
	[OSC_MIDI_FORMAT_INT32] = "iiii"
};
static const char *oscmidi_fmt_3 [] = {
	[OSC_MIDI_FORMAT_MIDI] = "mmm",
	[OSC_MIDI_FORMAT_INT32] = "iii"
};
static const char *oscmidi_fmt_2 [] = {
	[OSC_MIDI_FORMAT_MIDI] = "mm",
	[OSC_MIDI_FORMAT_INT32] = "ii"
};
static const char *oscmidi_fmt_1 [] = {
	[OSC_MIDI_FORMAT_MIDI] = "m",
	[OSC_MIDI_FORMAT_INT32] = "i"
};

static MIDI_Hash oscmidi_hash [BLOB_MAX];

static osc_data_t *pack;
static osc_data_t *bndl;

static void
oscmidi_init(void)
{
	uint_fast8_t i;
	OSC_MIDI_Group *group = config.oscmidi_groups;
	for(i=0; i<GROUP_MAX; i++, group++)
		mul[i] = (float)0x1fff / group->range;
}

static osc_data_t *
oscmidi_serialize(osc_data_t *buf, OSC_MIDI_Format format, uint8_t channel, uint8_t status, uint8_t dat1, uint8_t dat2)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	uint_fast8_t multi = config.oscmidi.multi;

	if(!multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		buf_ptr = osc_set_path(buf_ptr, config.oscmidi.path);
		buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_1[format]);
	}

	switch(format)
	{
		case OSC_MIDI_FORMAT_MIDI:
		{
			uint8_t *M;
			buf_ptr = osc_set_midi_inline(buf_ptr, &M);
			M[0] = 0;
			M[1] = channel | status;
			M[2] = dat1;
			M[3] = dat2;
			break;
		}
		case OSC_MIDI_FORMAT_INT32:
		{
			int32_t i = (dat2 << 16) | (dat1 << 8) | ( (channel | status) << 0);
			buf_ptr = osc_set_int32(buf_ptr, i);
		}
	}

	if(!multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	(void)nblob_old;
	(void)nblob_new;
	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	(void)offset;
	(void)nblob_old;
	(void)nblob_new;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_on_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	(void)pid;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;
	OSC_MIDI_Group *group = &oscmidi_groups[gid];
	OSC_MIDI_Mapping mapping = group->mapping;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		buf_ptr = osc_set_path(buf_ptr, config.oscmidi.path);
		if( (mapping == OSC_MIDI_MAPPING_CONTROL_CHANGE) && (group->control <= 0xd) )
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_4[format]);
		else
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_3[format]);
	}

	uint8_t ch = gid % 0xf;
	float X = group->offset + x*group->range;
	uint8_t key = floor(X);
	midi_add_key(oscmidi_hash, sid, key);

	uint16_t bend =(X - key)*mul[gid] + 0x1fff;
	uint16_t eff = y * 0x3fff;

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_NOTE_ON, key, 0x7f);
	buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	switch(mapping)
	{
		case OSC_MIDI_MAPPING_NOTE_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_NOTE_PRESSURE, key, eff >> 7);
			break;
		case OSC_MIDI_MAPPING_CHANNEL_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CHANNEL_PRESSURE, eff >> 7, 0x0);
			break;
		case OSC_MIDI_MAPPING_CONTROL_CHANGE:
			if(group->control <= 0xd)
				buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_LSV, eff & 0x7f);
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_MSV, eff >> 7);
			break;
	}
	
	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t * 
oscmidi_engine_off_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid)
{
	(void)pid;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		buf_ptr = osc_set_path(buf_ptr, config.oscmidi.path);
		buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_1[format]);
	}

	uint8_t ch = gid % 0xf;
	uint8_t key = midi_rem_key(oscmidi_hash, sid);

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_NOTE_OFF, key, 0x7f);

	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_set_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	(void)pid;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;
	OSC_MIDI_Group *group = &oscmidi_groups[gid];
	OSC_MIDI_Mapping mapping = group->mapping;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		buf_ptr = osc_set_path(buf_ptr, config.oscmidi.path);
		if( (mapping == OSC_MIDI_MAPPING_CONTROL_CHANGE) && (group->control <= 0xd) )
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_3[format]);
		else
			buf_ptr = osc_set_fmt(buf_ptr, oscmidi_fmt_2[format]);
	}

	uint8_t ch = gid % 0xf;
	float X = group->offset + x*group->range;
	uint8_t key = midi_get_key(oscmidi_hash, sid);
	uint16_t bend =(X - key)*mul[gid] + 0x1fff;
	uint16_t eff = y * 0x3fff;

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	switch(mapping)
	{
		case OSC_MIDI_MAPPING_NOTE_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_NOTE_PRESSURE, key, eff >> 7);
			break;
		case OSC_MIDI_MAPPING_CHANNEL_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CHANNEL_PRESSURE, eff >> 7, 0x0);
			break;
		case OSC_MIDI_MAPPING_CONTROL_CHANGE:
			if(group->control <= 0xd)
				buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_LSV, eff & 0x7f);
			buf_ptr = oscmidi_serialize(buf_ptr, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_MSV, eff >> 7);
			break;
	}

	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

CMC_Engine oscmidi_engine = {
	oscmidi_init,
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
_oscmidi_multi(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.oscmidi.multi);
}

static uint_fast8_t
_oscmidi_path(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size = 0;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1)
		size = CONFIG_SUCCESS("iss", uuid, path, config.oscmidi.path);
	else
	{
		const char *opath;

		buf_ptr = osc_get_string(buf_ptr, &opath);

		if(osc_check_path(opath))
		{
			strcpy(config.oscmidi.path, opath);
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "invalid OSC path");
	}

	CONFIG_SEND(size);

	return 1;
}

static const OSC_Query_Value oscmidi_format_args_values [] = {
	[OSC_MIDI_FORMAT_MIDI]			= { .s = "midi" },
	[OSC_MIDI_FORMAT_INT32]			= { .s = "int32" },
};

static uint_fast8_t
_oscmidi_format(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size = 0;
	uint8_t *format = &config.oscmidi.format;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1)
		size = CONFIG_SUCCESS("iss", uuid, path, oscmidi_format_args_values[*format].s);
	else
	{
		uint_fast8_t i;
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		for(i=0; i<sizeof(oscmidi_format_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(s, oscmidi_format_args_values[i].s))
			{
				*format = i;
				break;
			}
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_oscmidi_reset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)path;
	(void)fmt;
	(void)argc;
	(void)buf;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;
	
	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	uint_fast8_t i;
	OSC_MIDI_Group *group = config.oscmidi_groups;
	for(i=0; i<GROUP_MAX; i++, group++)
	{
		group->mapping = OSC_MIDI_MAPPING_CONTROL_CHANGE;
		group->control = 0x07;
		group->offset = MIDI_BOT;
		group->range = MIDI_RANGE;
		mul[i] = (float)0x1fff / group->range;
	}

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	return 1;
}

static const OSC_Query_Value oscmidi_mapping_args_values [] = {
	[OSC_MIDI_MAPPING_NOTE_PRESSURE]			= { .s = "note_pressure" },
	[OSC_MIDI_MAPPING_CHANNEL_PRESSURE]		= { .s = "channel_pressure" },
	[OSC_MIDI_MAPPING_CONTROL_CHANGE]			= { .s = "control_change" },
};

static uint_fast8_t
_oscmidi_attributes(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	uint16_t gid;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);
	
	sscanf(path, "/engines/oscmidi/attributes/%hu", &gid);
	OSC_MIDI_Group *group = &config.oscmidi_groups[gid];

	if(argc == 1)
	{
		size = CONFIG_SUCCESS("issffi", uuid, path, 
			oscmidi_mapping_args_values[group->mapping], group->offset, group->range, (int32_t)group->control);
	}
	else
	{
		const char *mapping;
		float offset;
		float range;
		int32_t control;

		buf_ptr = osc_get_string(buf_ptr, &mapping);
		buf_ptr = osc_get_float(buf_ptr, &offset);
		buf_ptr = osc_get_float(buf_ptr, &range);
		buf_ptr = osc_get_int32(buf_ptr, &control);

		uint_fast8_t i;
		for(i=0; i<sizeof(oscmidi_mapping_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(mapping, oscmidi_mapping_args_values[i].s))
			{
				group->mapping = i;
				break;
			}

		group->offset = offset;
		group->range = range;
		group->control = control;
		
		mul[gid] = (float)0x1fff / group->range;

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

static const OSC_Query_Argument oscmidi_path_args [] = {
	OSC_QUERY_ARGUMENT_STRING("String", OSC_QUERY_MODE_RW, 64),
};

static const OSC_Query_Argument oscmidi_format_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("Format", OSC_QUERY_MODE_RW, oscmidi_format_args_values)
};

static const OSC_Query_Argument oscmidi_attributes_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("Mapping", OSC_QUERY_MODE_RW, oscmidi_mapping_args_values),
	OSC_QUERY_ARGUMENT_FLOAT("Offset", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
	OSC_QUERY_ARGUMENT_FLOAT("Range", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f),
	OSC_QUERY_ARGUMENT_INT32("Controller", OSC_QUERY_MODE_RW, 0, 0x7f, 1)
};

static const OSC_Query_Item oscmidi_attribute_tree [] = {
	OSC_QUERY_ITEM_METHOD("%i", "Group %i", _oscmidi_attributes, oscmidi_attributes_args)
};

const OSC_Query_Item oscmidi_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _oscmidi_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("multi", "OSC Multi argument?", _oscmidi_multi, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("format", "OSC Format", _oscmidi_format, oscmidi_format_args),
	OSC_QUERY_ITEM_METHOD("path", "OSC Path", _oscmidi_path, oscmidi_path_args),
	OSC_QUERY_ITEM_METHOD("reset", "Reset attributes", _oscmidi_reset, NULL),
	OSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", oscmidi_attribute_tree, GROUP_MAX)
};
