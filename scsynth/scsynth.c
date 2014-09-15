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
static const char *set_str = "/n_set";

static const char *on_fmt = "siiisisi";
static const char *on_set_fmt = "iififiisi";
static const char *off_fmt = "isi";
static const char *set_fmt = "iififii";

static const char *default_fmt = "group%02i";

static OSC_Timetag tt;

static uint_fast8_t early_i = 0;
static uint_fast8_t late_i = 0;

static osc_data_t *pack;
static osc_data_t *bndl;

static osc_data_t *
scsynth_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	(void)nblob_old;
	(void)nblob_new;
	tt = offset;
	early_i = 0;
	late_i = 0;

	osc_data_t *buf_ptr = buf;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	return buf_ptr;
}

static osc_data_t *
scsynth_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
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
scsynth_engine_on_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	(void)pid;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[gid];
	
	id = group->is_group ? group->group : group->sid + sid;

	// message to create synth(sent early, e.g immediately)
	if(group->alloc)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		{
			//osc_data_t *sub;
			//osc_data_t *s;
			//buf_ptr = osc_start_bundle(buf_ptr, OSC_IMMEDIATE, &s);
			//buf_ptr = osc_start_bundle_item(buf_ptr, &sub);
			//{
				buf_ptr = osc_set_path(buf_ptr, on_str);
				buf_ptr = osc_set_fmt(buf_ptr, on_fmt);

				buf_ptr = osc_set_string(buf_ptr, group->name); // synthdef name 
				buf_ptr = osc_set_int32(buf_ptr, id);
				buf_ptr = osc_set_int32(buf_ptr, group->add_action);
				buf_ptr = osc_set_int32(buf_ptr, group->group); // group id
				buf_ptr = osc_set_string(buf_ptr,(char *)gate_str);
				buf_ptr = osc_set_int32(buf_ptr, 0); // do not start synth yet
				buf_ptr = osc_set_string(buf_ptr,(char *)out_str);
				buf_ptr = osc_set_int32(buf_ptr, group->out);
			//}
			//buf_ptr = osc_end_bundle_item(buf_ptr, sub);
			//buf_ptr = osc_end_bundle(buf_ptr, s);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);
	}

	// message to start synth(sent late, e.g. with lag)
	if(group->gate)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, set_str);
			buf_ptr = osc_set_fmt(buf_ptr, on_set_fmt);

			buf_ptr = osc_set_int32(buf_ptr, id);
			buf_ptr = osc_set_int32(buf_ptr, group->arg + 0);
			buf_ptr = osc_set_float(buf_ptr, x);
			buf_ptr = osc_set_int32(buf_ptr, group->arg + 1);
			buf_ptr = osc_set_float(buf_ptr, y);
			buf_ptr = osc_set_int32(buf_ptr, group->arg + 2);
			buf_ptr = osc_set_int32(buf_ptr, pid);
			buf_ptr = osc_set_string(buf_ptr, (char *)gate_str);
			buf_ptr = osc_set_int32(buf_ptr, 1);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);
	}
	
	return buf_ptr;
}

static osc_data_t *
scsynth_engine_off_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid)
{
	(void)pid;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[gid];

	id = group->is_group ? group->group : group->sid + sid;

	if(group->gate)
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, set_str);
			buf_ptr = osc_set_fmt(buf_ptr, off_fmt);

			buf_ptr = osc_set_int32(buf_ptr, id);
			buf_ptr = osc_set_string(buf_ptr, (char *)gate_str);
			buf_ptr = osc_set_int32(buf_ptr, 0);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);
	}

	return buf_ptr;
}

static osc_data_t *
scsynth_engine_set_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint32_t id;
	SCSynth_Group *group = &scsynth_groups[gid];

	id = group->is_group ? group->group : group->sid + sid;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, set_str);
		buf_ptr = osc_set_fmt(buf_ptr, set_fmt);

		buf_ptr = osc_set_int32(buf_ptr, id);
		buf_ptr = osc_set_int32(buf_ptr, group->arg + 0);
		buf_ptr = osc_set_float(buf_ptr, x);
		buf_ptr = osc_set_int32(buf_ptr, group->arg + 1);
		buf_ptr = osc_set_float(buf_ptr, y);
		buf_ptr = osc_set_int32(buf_ptr, group->arg + 2);
		buf_ptr = osc_set_int32(buf_ptr, pid);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

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

static void
scsynth_group_get(uint_fast8_t gid, char **name, uint16_t *sid, uint16_t *group, uint16_t *out, uint8_t *arg,
										uint8_t *alloc, uint8_t *gate, uint8_t *add_action, uint8_t *is_group)
{
	SCSynth_Group *grp = &scsynth_groups[gid];

	*name = grp->name;
	*sid = grp->sid;
	*group = grp->group;
	*out = grp->out;
	*arg = grp->arg;
	*alloc = grp->alloc;
	*gate = grp->gate;
	*add_action = grp->add_action;
	*is_group = grp->is_group;
}

static void
scsynth_group_set(uint_fast8_t gid, const char *name, uint16_t sid, uint16_t group, uint16_t out, uint8_t arg,
										uint8_t alloc, uint8_t gate, uint8_t add_action, uint8_t is_group)
{
	SCSynth_Group *grp = &scsynth_groups[gid];

	strcpy(grp->name, name);
	grp->sid = sid;
	grp->group = group;
	grp->out = out;
	grp->arg = arg;
	grp->alloc = alloc;
	grp->gate = gate;
	grp->add_action = add_action;
	grp->is_group = is_group;
}

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
_scsynth_attributes(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	uint16_t gid;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);
	
	sscanf(path, "/engines/scsynth/attributes/%hu", &gid);

	if(argc == 1)
	{
		char *name;
		uint16_t sid;
		uint16_t group;
		uint16_t out;
		uint8_t arg;
		uint8_t alloc;
		uint8_t gate;
		uint8_t add_action;
		uint8_t is_group;

		scsynth_group_get(gid, &name, &sid, &group, &out, &arg, &alloc, &gate, &add_action, &is_group);
		size = CONFIG_SUCCESS("issiiiiiiii", uuid, path, name, sid, group, out, arg,
			alloc?1:0, gate?1:0, add_action, is_group?1:0);
	}
	else // argc == 11
	{
		const char *name;
		int32_t sid;
		int32_t group;
		int32_t out;
		int32_t arg;
		int32_t alloc;
		int32_t gate;
		int32_t add_action;
		int32_t is_group;

		buf_ptr = osc_get_string(buf_ptr, &name);
		buf_ptr = osc_get_int32(buf_ptr, &sid);
		buf_ptr = osc_get_int32(buf_ptr, &group);
		buf_ptr = osc_get_int32(buf_ptr, &out);
		buf_ptr = osc_get_int32(buf_ptr, &arg);
		buf_ptr = osc_get_int32(buf_ptr, &alloc);
		buf_ptr = osc_get_int32(buf_ptr, &gate);
		buf_ptr = osc_get_int32(buf_ptr, &add_action);
		buf_ptr = osc_get_int32(buf_ptr, &is_group);

		scsynth_group_set(gid, name, sid, group, out, arg, alloc, gate, add_action, is_group);
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
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
		group->sid = 1000;
		group->group = i;
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

/*
 * Query
 */

static const OSC_Query_Argument scsynth_attributes_args [] = {
	OSC_QUERY_ARGUMENT_STRING("Name", OSC_QUERY_MODE_RW, 8),
	OSC_QUERY_ARGUMENT_INT32("Synth ID offset", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1),
	OSC_QUERY_ARGUMENT_INT32("Group ID offset", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1),
	OSC_QUERY_ARGUMENT_INT32("Out channel", OSC_QUERY_MODE_RW, 0, UINT16_MAX, 1),
	OSC_QUERY_ARGUMENT_INT32("Argument offset", OSC_QUERY_MODE_RW, 0, UINT8_MAX, 1),
	OSC_QUERY_ARGUMENT_BOOL("Allocate?", OSC_QUERY_MODE_RW),
	OSC_QUERY_ARGUMENT_BOOL("Gate?", OSC_QUERY_MODE_RW),
	OSC_QUERY_ARGUMENT_BOOL("AddToTail?", OSC_QUERY_MODE_RW),
	OSC_QUERY_ARGUMENT_BOOL("Group?", OSC_QUERY_MODE_RW)
};

static const OSC_Query_Item scsynth_attribute_tree [] = {
	OSC_QUERY_ITEM_METHOD("%i", "Group %i", _scsynth_attributes, scsynth_attributes_args),
};

const OSC_Query_Item scsynth_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _scsynth_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("reset", "Reset attributes", _scsynth_reset, NULL),
	OSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", scsynth_attribute_tree, GROUP_MAX)
};

#undef M
