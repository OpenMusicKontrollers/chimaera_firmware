/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
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
oscmidi_serialize(osc_data_t *buf, osc_data_t *end, OSC_MIDI_Format format, uint8_t channel, uint8_t status, uint8_t dat1, uint8_t dat2)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	uint_fast8_t multi = config.oscmidi.multi;

	if(!multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		buf_ptr = osc_set_path(buf_ptr, end, config.oscmidi.path);
		buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_1[format]);
	}

	switch(format)
	{
		case OSC_MIDI_FORMAT_MIDI:
		{
			uint8_t *M;
			buf_ptr = osc_set_midi_inline(buf_ptr, end, &M);
			if(buf_ptr)
			{
				M[0] = 0;
				M[1] = channel | status;
				M[2] = dat1;
				M[3] = dat2;
			}
			break;
		}
		case OSC_MIDI_FORMAT_INT32:
		{
			int32_t i = (dat2 << 16) | (dat1 << 8) | ( (channel | status) << 0);
			buf_ptr = osc_set_int32(buf_ptr, end, i);
		}
	}

	if(!multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_frame_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, end, fev->offset, &bndl);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_end_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	(void)fev;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, end, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, pack);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_on_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;
	OSC_MIDI_Group *group = &oscmidi_groups[bev->gid];
	OSC_MIDI_Mapping mapping = group->mapping;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		buf_ptr = osc_set_path(buf_ptr, end, config.oscmidi.path);
		if( (mapping == OSC_MIDI_MAPPING_CONTROL_CHANGE) && (group->control <= 0xd) )
			buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_4[format]);
		else
			buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_3[format]);
	}

	uint8_t ch = bev->gid % 0xf;
	float X = group->offset + bev->x*group->range;
	uint8_t key = floor(X);
	midi_add_key(oscmidi_hash, bev->sid, key);

	uint16_t bend =(X - key)*mul[bev->gid] + 0x1fff;
	uint16_t eff = bev->y * 0x3fff;

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_NOTE_ON, key, 0x7f);
	buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	switch(mapping)
	{
		case OSC_MIDI_MAPPING_NOTE_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_NOTE_PRESSURE, key, eff >> 7);
			break;
		case OSC_MIDI_MAPPING_CHANNEL_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CHANNEL_PRESSURE, eff >> 7, 0x0);
			break;
		case OSC_MIDI_MAPPING_CONTROL_CHANGE:
			if(group->control <= 0xd)
				buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_LSV, eff & 0x7f);
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_MSV, eff >> 7);
			break;
	}
	
	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t * 
oscmidi_engine_off_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		buf_ptr = osc_set_path(buf_ptr, end, config.oscmidi.path);
		buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_1[format]);
	}

	uint8_t ch = bev->gid % 0xf;
	uint8_t key = midi_rem_key(oscmidi_hash, bev->sid);

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_NOTE_OFF, key, 0x7f);

	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
oscmidi_engine_set_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm = NULL;
	OSC_MIDI_Format format = config.oscmidi.format;
	uint_fast8_t multi = config.oscmidi.multi;
	OSC_MIDI_Group *group = &oscmidi_groups[bev->gid];
	OSC_MIDI_Mapping mapping = group->mapping;

	if(multi)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		buf_ptr = osc_set_path(buf_ptr, end, config.oscmidi.path);
		if( (mapping == OSC_MIDI_MAPPING_CONTROL_CHANGE) && (group->control <= 0xd) )
			buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_3[format]);
		else
			buf_ptr = osc_set_fmt(buf_ptr, end, oscmidi_fmt_2[format]);
	}

	uint8_t ch = bev->gid % 0xf;
	float X = group->offset + bev->x*group->range;
	uint8_t key = midi_get_key(oscmidi_hash, bev->sid);
	uint16_t bend =(X - key)*mul[bev->gid] + 0x1fff;
	uint16_t eff = bev->y * 0x3fff;

	// serialize
	buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_PITCH_BEND, bend & 0x7f, bend >> 7);
	switch(mapping)
	{
		case OSC_MIDI_MAPPING_NOTE_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_NOTE_PRESSURE, key, eff >> 7);
			break;
		case OSC_MIDI_MAPPING_CHANNEL_PRESSURE:
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CHANNEL_PRESSURE, eff >> 7, 0x0);
			break;
		case OSC_MIDI_MAPPING_CONTROL_CHANGE:
			if(group->control <= 0xd)
				buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_LSV, eff & 0x7f);
			buf_ptr = oscmidi_serialize(buf_ptr, end, format, ch, MIDI_STATUS_CONTROL_CHANGE, group->control | MIDI_MSV, eff >> 7);
			break;
	}

	if(multi)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

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

#define OSCMIDI_GID(PATH) \
({ \
	uint16_t _gid = 0; \
 	sscanf(PATH, "/engines/oscmidi/attributes/%hu/", &_gid); \
 	(&config.oscmidi_groups[_gid]); \
})

static uint_fast8_t
_oscmidi_mapping(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;

	OSC_MIDI_Group *grp = OSCMIDI_GID(path);

	int32_t uuid;
	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1)
	{
		size = CONFIG_SUCCESS("iss", uuid, path, oscmidi_mapping_args_values[grp->mapping]);
	}
	else // argc == 2
	{
		const char *mapping;
		buf_ptr = osc_get_string(buf_ptr, &mapping);

		uint_fast8_t i;
		for(i=0; i<sizeof(oscmidi_mapping_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(mapping, oscmidi_mapping_args_values[i].s))
			{
				grp->mapping = i;
				break;
			}

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_oscmidi_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	OSC_MIDI_Group *grp = OSCMIDI_GID(path);

	return config_check_float(path, fmt, argc, buf, &grp->offset);
}

static uint_fast8_t
_oscmidi_range(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	OSC_MIDI_Group *grp = OSCMIDI_GID(path);

	return config_check_float(path, fmt, argc, buf, &grp->range);
}

static uint_fast8_t
_oscmidi_controller(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	OSC_MIDI_Group *grp = OSCMIDI_GID(path);

	return config_check_uint8(path, fmt, argc, buf, &grp->control);
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

static const OSC_Query_Argument mapping_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("Mapping", OSC_QUERY_MODE_RW, oscmidi_mapping_args_values)
};

static const OSC_Query_Argument offset_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Offset", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f)
};

static const OSC_Query_Argument range_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Range", OSC_QUERY_MODE_RW, 0.f, 0x7f, 0.f)
};

static const OSC_Query_Argument controller_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Controller", OSC_QUERY_MODE_RW, 0, 0x7f, 1)
};

static const OSC_Query_Item oscmidi_attributes_tree [] = {
	OSC_QUERY_ITEM_METHOD("mapping", "Mapping", _oscmidi_mapping, mapping_args),
	OSC_QUERY_ITEM_METHOD("offset", "Note offset", _oscmidi_offset, offset_args),
	OSC_QUERY_ITEM_METHOD("range", "Pitchbend range", _oscmidi_range, range_args),
	OSC_QUERY_ITEM_METHOD("controller", "Controller", _oscmidi_controller, controller_args),
};

static const OSC_Query_Item group_array [] = {
	OSC_QUERY_ITEM_NODE("%i/", "Group", oscmidi_attributes_tree)
};

const OSC_Query_Item oscmidi_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _oscmidi_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("multi", "OSC Multi argument?", _oscmidi_multi, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("format", "OSC Format", _oscmidi_format, oscmidi_format_args),
	OSC_QUERY_ITEM_METHOD("path", "OSC Path", _oscmidi_path, oscmidi_path_args),
	OSC_QUERY_ITEM_METHOD("reset", "Reset attributes", _oscmidi_reset, NULL),
	OSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", group_array, GROUP_MAX)
};
