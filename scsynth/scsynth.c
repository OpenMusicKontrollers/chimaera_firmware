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

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include <scsynth.h>

// global
SCSynth_Group *scsynth_groups = config.scsynth_groups;

// local
static const char *gate_str = "gate";
static const char *out_str = "out";

static const char *on_str = "/s_new";
static const char *set_str = "/n_setn";
static const char *off_str = "/n_set";

static const char *on_fmt [2] = {
	[0] = "siiiiisi",		// !gate
	[1] = "siiiiisisi"	// gate
};
static const char *off_fmt = "isi";
static const char *set_fmt [2] = {
	[0] = "iiiff",	// !derivatives
	[1] = "iiiffff"	// derivatives
};

static const char *default_fmt = "synth_%i";

static OSC_Timetag tt;

static uint_fast8_t early_i = 0;
static uint_fast8_t late_i = 0;

static osc_data_t *pack;
static osc_data_t *bndl;

static osc_data_t *
scsynth_engine_frame_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	tt = fev->offset;
	early_i = 0;
	late_i = 0;

	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, end, fev->offset, &bndl);

	return buf_ptr;
}

static osc_data_t *
scsynth_engine_end_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	(void)fev;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, end, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, pack);

	return buf_ptr;
}

static osc_data_t *
scsynth_engine_on_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[bev->gid];
	
	id = group->is_group ? group->group : group->sid + bev->sid;

	// message to create synth
	if(group->alloc)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, end, on_str);
			buf_ptr = osc_set_fmt(buf_ptr, end, on_fmt[group->gate]);

			buf_ptr = osc_set_string(buf_ptr, end, group->name); // synthdef name 
			buf_ptr = osc_set_int32(buf_ptr, end, id);
			buf_ptr = osc_set_int32(buf_ptr, end, group->add_action);
			buf_ptr = osc_set_int32(buf_ptr, end, group->group); // group id
			buf_ptr = osc_set_int32(buf_ptr, end, group->arg + 4);
			buf_ptr = osc_set_int32(buf_ptr, end, bev->pid);
			if(group->gate)
			{
				buf_ptr = osc_set_string(buf_ptr, end, (char *)gate_str);
				buf_ptr = osc_set_int32(buf_ptr, end, 1);
			}
			buf_ptr = osc_set_string(buf_ptr, end, (char *)out_str);
			buf_ptr = osc_set_int32(buf_ptr, end, group->out);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);
	}
	else if(group->gate)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, end, off_str);
			buf_ptr = osc_set_fmt(buf_ptr, end, off_fmt);

			buf_ptr = osc_set_int32(buf_ptr, end, id);
			buf_ptr = osc_set_string(buf_ptr, end, (char *)gate_str);
			buf_ptr = osc_set_int32(buf_ptr, end, 1);

		}
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);
	}

	// first set message
	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, set_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, set_fmt[config.scsynth.derivatives]);

		buf_ptr = osc_set_int32(buf_ptr, end, id);
		buf_ptr = osc_set_int32(buf_ptr, end, group->arg + 0);
		buf_ptr = osc_set_int32(buf_ptr, end, config.scsynth.derivatives ? 4 : 2);
		buf_ptr = osc_set_float(buf_ptr, end, bev->x);
		buf_ptr = osc_set_float(buf_ptr, end, bev->y);
		if(config.scsynth.derivatives)
		{
			buf_ptr = osc_set_float(buf_ptr, end, bev->vx);
			buf_ptr = osc_set_float(buf_ptr, end, bev->vy);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);
	
	return buf_ptr;
}

static osc_data_t *
scsynth_engine_off_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[bev->gid];

	id = group->is_group ? group->group : group->sid + bev->sid;

	if(group->gate)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, end, off_str);
			buf_ptr = osc_set_fmt(buf_ptr, end, off_fmt);

			buf_ptr = osc_set_int32(buf_ptr, end, id);
			buf_ptr = osc_set_string(buf_ptr, end, (char *)gate_str);
			buf_ptr = osc_set_int32(buf_ptr, end, 0);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);
	}

	return buf_ptr;
}

static osc_data_t *
scsynth_engine_set_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[bev->gid];

	id = group->is_group ? group->group : group->sid + bev->sid;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, set_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, set_fmt[config.scsynth.derivatives]);

		buf_ptr = osc_set_int32(buf_ptr, end, id);
		buf_ptr = osc_set_int32(buf_ptr, end, group->arg + 0);
		buf_ptr = osc_set_int32(buf_ptr, end, config.scsynth.derivatives ? 4 : 2);
		buf_ptr = osc_set_float(buf_ptr, end, bev->x);
		buf_ptr = osc_set_float(buf_ptr, end, bev->y);
		if(config.scsynth.derivatives)
		{
			buf_ptr = osc_set_float(buf_ptr, end, bev->vx);
			buf_ptr = osc_set_float(buf_ptr, end, bev->vy);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

CMC_Engine scsynth_engine = {
	NULL,
	scsynth_engine_frame_cb,
	scsynth_engine_on_cb,
	scsynth_engine_off_cb,
	scsynth_engine_set_cb,
	scsynth_engine_end_cb
};

/*
 * Config
 */

static uint_fast8_t
_scsynth_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.scsynth.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_scsynth_derivatives(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.scsynth.derivatives);
}

static uint_fast8_t
_scsynth_reset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	uint_fast8_t i;
	for(i=0; i<GROUP_MAX; i++)
	{
		SCSynth_Group *group = &scsynth_groups[i];
		sprintf(group->name, default_fmt, i);
		group->sid = 200;
		group->group = 100 + i;
		group->out = i;
		group->arg = 0;
		group->alloc = 1;
		group->gate = 1;
		group->add_action = SCSYNTH_ADD_TO_HEAD;
		group->is_group = 0;
	}
	
	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	return 1;
}

#define SCSYNTH_GID(PATH) \
({ \
	uint16_t _gid = 0; \
 	sscanf(PATH, "/engines/scsynth/attributes/%hu/", &_gid); \
 	(&scsynth_groups[_gid]); \
})

static uint_fast8_t
_scsynth_name(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;

	SCSynth_Group *grp = SCSYNTH_GID(path);

	int32_t uuid;
	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1)
	{
		size = CONFIG_SUCCESS("iss", uuid, path, grp->name);
	}
	else // argc == 2
	{
		const char *name;
		buf_ptr = osc_get_string(buf_ptr, &name);
		strcpy(grp->name, name);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_scsynth_sid_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_uint16(path, fmt, argc, buf, &grp->sid);
}

static uint_fast8_t
_scsynth_gid_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_uint16(path, fmt, argc, buf, &grp->group);
}

static uint_fast8_t
_scsynth_out(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_uint16(path, fmt, argc, buf, &grp->out);
}

static uint_fast8_t
_scsynth_arg_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_uint8(path, fmt, argc, buf, &grp->arg);
}

static uint_fast8_t
_scsynth_allocate(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_bool(path, fmt, argc, buf, &grp->alloc);
}

static uint_fast8_t
_scsynth_gate(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_bool(path, fmt, argc, buf, &grp->gate);
}

static uint_fast8_t
_scsynth_tail(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_bool(path, fmt, argc, buf, &grp->add_action);
}

static uint_fast8_t
_scsynth_group(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	SCSynth_Group *grp = SCSYNTH_GID(path);

	return config_check_bool(path, fmt, argc, buf, &grp->is_group);
}

/*
 * Query
 */

static const OSC_Query_Argument name_args [] = {
	OSC_QUERY_ARGUMENT_STRING("String", OSC_QUERY_MODE_RW, 8),
};

static const OSC_Query_Argument sid_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Offset", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1)
};

static const OSC_Query_Argument gid_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Offset", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1)
};

static const OSC_Query_Argument out_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Channel", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1)
};

static const OSC_Query_Argument off_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Offset", OSC_QUERY_MODE_RW, 0, UINT8_MAX, 1)
};

static const OSC_Query_Item scsynth_attributes_tree [] = {
	OSC_QUERY_ITEM_METHOD("name", "Synth name", _scsynth_name, name_args),
	OSC_QUERY_ITEM_METHOD("sid_offset", "Synth ID offset", _scsynth_sid_offset, sid_args),
	OSC_QUERY_ITEM_METHOD("gid_offset", "Group ID offset", _scsynth_gid_offset, gid_args),
	OSC_QUERY_ITEM_METHOD("out", "Out channel", _scsynth_out, out_args),
	OSC_QUERY_ITEM_METHOD("arg_offset", "Argument offset", _scsynth_arg_offset, off_args),
	OSC_QUERY_ITEM_METHOD("allocate", "Allocate synth?", _scsynth_allocate, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("gate", "Toggle gate?", _scsynth_gate, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("tail", "Add to tail?", _scsynth_tail, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("group", "Is group?", _scsynth_group, config_boolean_args),
};

static const OSC_Query_Item group_array [] = {
	OSC_QUERY_ITEM_NODE("%i/", "Group", scsynth_attributes_tree)
};

const OSC_Query_Item scsynth_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _scsynth_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("derivatives", "Calculate derivatives", _scsynth_derivatives, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("reset", "Reset attributes", _scsynth_reset, NULL),
	OSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", group_array, GROUP_MAX)
};

#undef M
