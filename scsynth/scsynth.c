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
#include <stdio.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "scsynth_private.h"

static SCSynth_Group scsynth_groups [GROUP_MAX];

static nOSC_Item scsynth_early_bndl [BLOB_MAX];
static nOSC_Item scsynth_late_bndl [BLOB_MAX];
static nOSC_Item scsynth_bndl [2];

static char scsynth_early_fmt [BLOB_MAX+1];
static char scsynth_late_fmt [BLOB_MAX+1];
static char scsynth_fmt [3];

static SCSynth_Msg msgs[BLOB_MAX*2];

static const char *gate_str = "gate";
static const char *out_str = "out";

static const char *on_str = "/s_new";
static const char *set_str = "/n_set";

static const char *on_fmt = "siiisisi";
static const char *on_set_fmt = "iififiisi";
static const char *off_fmt = "isi";
static const char *set_fmt = "iififii";

static const char *default_fmt = "group%02i";

static nOSC_Timestamp tt;

static uint_fast8_t early_i = 0;
static uint_fast8_t late_i = 0;

nOSC_Bundle_Item scsynth_osc = {
	.bndl = scsynth_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = scsynth_fmt
};

void
scsynth_init ()
{
	memset (scsynth_early_fmt, nOSC_MESSAGE, BLOB_MAX);
	scsynth_early_fmt[BLOB_MAX] = nOSC_TERM;

	memset (scsynth_late_fmt, nOSC_MESSAGE, BLOB_MAX);
	scsynth_late_fmt[BLOB_MAX] = nOSC_TERM;

	memset (scsynth_fmt, nOSC_BUNDLE, 2);
	scsynth_fmt[2] = nOSC_TERM;

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
}

static void
scsynth_engine_frame_cb (uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	scsynth_early_fmt[0] = nOSC_TERM;
	scsynth_late_fmt[0] = nOSC_TERM;

	tt = offset;
	early_i = 0;
	late_i = 0;

	nosc_item_bundle_set (scsynth_bndl, 0, scsynth_late_bndl, tt, scsynth_late_fmt);
	scsynth_fmt[0] = nOSC_BUNDLE;
	scsynth_fmt[1] = nOSC_TERM;
}

static void
scsynth_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint32_t id;
	nOSC_Message msg;
	SCSynth_Group *group = &scsynth_groups[gid];
	
	id = group->is_group ? group->group : group->sid + sid;

	// message to create synth (sent early, e.g immediately)
	if(group->alloc)
	{
		msg = msgs[early_i + late_i];
		nosc_message_set_string (msg, 0, group->name); // synthdef name 
		nosc_message_set_int32 (msg, 1, id);
		nosc_message_set_int32 (msg, 2, group->add_action);
		nosc_message_set_int32 (msg, 3, group->group); // group id
		nosc_message_set_string (msg, 4, (char *)gate_str);
		nosc_message_set_int32 (msg, 5, 0); // do not start synth yet
		nosc_message_set_string (msg, 6, (char *)out_str);
		nosc_message_set_int32 (msg, 7, group->out);

		nosc_item_message_set (scsynth_early_bndl, early_i, msg, (char *)on_str, (char *)on_fmt);
		scsynth_early_fmt[early_i++] = nOSC_MESSAGE;
		scsynth_early_fmt[early_i] = nOSC_TERM;

		// we have an early bundle, so instantiate it
		nosc_item_bundle_set (scsynth_bndl, 0, scsynth_early_bndl, nOSC_IMMEDIATE, scsynth_early_fmt);
		nosc_item_bundle_set (scsynth_bndl, 1, scsynth_late_bndl, tt, scsynth_late_fmt);
		scsynth_fmt[0] = nOSC_BUNDLE;
		scsynth_fmt[1] = nOSC_BUNDLE;
		scsynth_fmt[2] = nOSC_TERM;
	}

	// message to start synth (sent late, e.g. with lag)
	if(group->gate)
	{
		msg = msgs[early_i + late_i];
		nosc_message_set_int32 (msg, 0, id);
		nosc_message_set_int32 (msg, 1, group->arg + 0);
		nosc_message_set_float (msg, 2, x);
		nosc_message_set_int32 (msg, 3, group->arg + 1);
		nosc_message_set_float (msg, 4, y);
		nosc_message_set_int32 (msg, 5, group->arg + 2);
		nosc_message_set_int32 (msg, 6, pid);
		nosc_message_set_string (msg, 7, (char *)gate_str);
		nosc_message_set_int32 (msg, 8, 1);

		nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)on_set_fmt);
		scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
		scsynth_late_fmt[late_i] = nOSC_TERM;
	}
}

static void
scsynth_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	uint32_t id;
	nOSC_Message msg;
	SCSynth_Group *group = &scsynth_groups[gid];

	id = group->is_group ? group->group : group->sid + sid;

	if(group->gate)
	{
		msg = msgs[early_i + late_i];
		nosc_message_set_int32 (msg, 0, id);
		nosc_message_set_string (msg, 1, (char *)gate_str);
		nosc_message_set_int32 (msg, 2, 0);

		nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)off_fmt);
		scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
		scsynth_late_fmt[late_i] = nOSC_TERM;
	}
}

static void
scsynth_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	uint32_t id;
	nOSC_Message msg;
	SCSynth_Group *group = &scsynth_groups[gid];

	id = group->is_group ? group->group : group->sid + sid;
	
	msg = msgs[early_i + late_i];
	nosc_message_set_int32 (msg, 0, id);
	nosc_message_set_int32 (msg, 1, group->arg + 0);
	nosc_message_set_float (msg, 2, x);
	nosc_message_set_int32 (msg, 3, group->arg + 1);
	nosc_message_set_float (msg, 4, y);
	nosc_message_set_int32 (msg, 5, group->arg + 2);
	nosc_message_set_int32 (msg, 6, pid);

	nosc_item_message_set (scsynth_late_bndl, late_i, msg, (char *)set_str, (char *)set_fmt);
	scsynth_late_fmt[late_i++] = nOSC_MESSAGE;
	scsynth_late_fmt[late_i] = nOSC_TERM;
}

CMC_Engine scsynth_engine = {
	scsynth_engine_frame_cb,
	scsynth_engine_on_cb,
	scsynth_engine_off_cb,
	scsynth_engine_set_cb
};

uint_fast8_t
scsynth_group_get (uint_fast8_t gid, char **name, uint16_t *sid, uint16_t *group, uint16_t *out, uint8_t *arg,
										uint8_t *alloc, uint8_t *gate, uint8_t *add_action, uint8_t *is_group)
{
	if(gid >= GROUP_MAX)
		return 0;

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

	return 1;
}

uint_fast8_t
scsynth_group_set (uint_fast8_t gid, char *name, uint16_t sid, uint16_t group, uint16_t out, uint8_t arg,
										uint8_t alloc, uint8_t gate, uint8_t add_action, uint8_t is_group)
{
	if( (gid >= GROUP_MAX) || (strlen(name)+1>offsetof(SCSynth_Group, sid)) ) //TODO checks for remaining arguments
		return 0;

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

	return 1;
}

/*
 * Config
 */

static uint_fast8_t
_scsynth_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = config_check_bool (path, fmt, argc, args, &config.scsynth.enabled);
	cmc_engines_update ();
	return res;
}

static uint_fast8_t
_scsynth_group (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	uint16_t gid = args[1].i;

	char *name;
	uint16_t sid;
	uint16_t group;
	uint16_t out;
	uint8_t arg;
	uint8_t alloc;
	uint8_t gate;
	uint8_t add_action;
	uint8_t is_group;

	if(argc == 2)
	{
		if(scsynth_group_get(gid, &name, &sid, &group, &out, &arg, &alloc, &gate, &add_action, &is_group))
			size = CONFIG_SUCCESS ("isisiiiiiiii", uuid, path, gid, name, sid, group, out, arg,
				alloc?1:0, gate?1:0, add_action, is_group?1:0);
		else
			size = CONFIG_FAIL ("iss", uuid, path, "argument out of bounds"); // TODO remove
	}
	else // argc == 11
	{
		name = args[2].s;
		sid = args[3].i;
		group = args[4].i;
		out = args[5].i;
		arg = args[6].i;
		alloc = args[7].i;
		gate = args[8].i;
		add_action = args[9].i;
		is_group = args[10].i;

		if(scsynth_group_set(gid, name, sid, group, out, arg, alloc, gate, add_action, is_group))
			size = CONFIG_SUCCESS ("is", uuid, path);
		else
			size = CONFIG_FAIL ("iss", uuid, path, "argument out of bounds"); // TODO remove
	}

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

/*
 * Query
 */

static const nOSC_Query_Argument scsynth_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument scsynth_group_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("group", nOSC_QUERY_MODE_RWX, 0, GROUP_MAX),
	nOSC_QUERY_ARGUMENT_STRING("name", nOSC_QUERY_MODE_RW),
	nOSC_QUERY_ARGUMENT_INT32("sid", nOSC_QUERY_MODE_RW, 0, UINT16_MAX),
	nOSC_QUERY_ARGUMENT_INT32("gid", nOSC_QUERY_MODE_RW, 0, UINT16_MAX),
	nOSC_QUERY_ARGUMENT_INT32("out", nOSC_QUERY_MODE_RW, 0, UINT16_MAX),
	nOSC_QUERY_ARGUMENT_INT32("arg", nOSC_QUERY_MODE_RW, 0, UINT8_MAX),
	nOSC_QUERY_ARGUMENT_BOOL("alloc", nOSC_QUERY_MODE_RW),
	nOSC_QUERY_ARGUMENT_BOOL("gate", nOSC_QUERY_MODE_RW),
	nOSC_QUERY_ARGUMENT_INT32("add_action", nOSC_QUERY_MODE_RW, 0, 4),
	nOSC_QUERY_ARGUMENT_BOOL("is_group", nOSC_QUERY_MODE_RW)
};

const nOSC_Query_Item scsynth_tree [] = {
	// read-write
	nOSC_QUERY_ITEM_METHOD("enabled", "enable/disable", _scsynth_enabled, scsynth_enabled_args),
	nOSC_QUERY_ITEM_METHOD("group", "group specs", _scsynth_group, scsynth_group_args),
};
