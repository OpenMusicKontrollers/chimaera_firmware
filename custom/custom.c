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
 *     distribution->
 */

#include <string.h>
#include <math.h> // floor

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include "custom_private.h"

static Custom_Item *items = config.custom.items;
static RPN_Stack stack;

static osc_data_t *pack;
static osc_data_t *bndl;

static osc_data_t *
custom_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	stack.fid = fid;
	stack.sid = stack.gid = stack.pid = 0;
	stack.x = stack.z = 0.f;

	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	uint_fast8_t i;
	Custom_Item *item;
	if(nblob_old + nblob_new)
	{
		for(i=0; i<CUSTOM_MAX_EXPR; i++)
		{
			item = &items[i];
			if(item->dest == RPN_FRAME)
			{
				buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
				{
					buf_ptr = osc_set_path(buf_ptr, item->path);
					buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

					buf_ptr = rpn_run(buf_ptr, item, &stack);
				}
				buf_ptr = osc_end_bundle_item(buf_ptr, itm);
			}
			else if(item->dest == RPN_NONE)
				break;
		}
	}
	else
	{
		for(i=0; i<CUSTOM_MAX_EXPR; i++)
		{
			item = &items[i];
			if(item->dest == RPN_IDLE)
			{
				buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
				{
					buf_ptr = osc_set_path(buf_ptr, item->path);
					buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

					buf_ptr = rpn_run(buf_ptr, item, &stack);
				}
				buf_ptr = osc_end_bundle_item(buf_ptr, itm);
			}
			else if(item->dest == RPN_NONE)
				break;
		}
	}

	return buf_ptr;
}

static osc_data_t *
custom_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	Custom_Item *item;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		item = &items[i];
		if(item->dest == RPN_END)
		{
			buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
			{
				buf_ptr = osc_set_path(buf_ptr, item->path);
				buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

				buf_ptr = rpn_run(buf_ptr, item, &stack);
			}
			buf_ptr = osc_end_bundle_item(buf_ptr, itm);
		}
		else if(item->dest == RPN_NONE)
			break;
	}

	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
custom_engine_on_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = x;
	stack.z = y;

	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	Custom_Item *item;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		item = &items[i];
		if(item->dest == RPN_ON)
		{
			buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
			{
				buf_ptr = osc_set_path(buf_ptr, item->path);
				buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

				buf_ptr = rpn_run(buf_ptr, item, &stack);
			}
			buf_ptr = osc_end_bundle_item(buf_ptr, itm);
		}
		else if(item->dest == RPN_NONE)
			break;
	}
	
	return buf_ptr;
}

static osc_data_t *
custom_engine_off_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid)
{
	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = stack.z = 0.f;

	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	Custom_Item *item;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		item = &items[i];
		if(item->dest == RPN_OFF)
		{
			buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
			{
				buf_ptr = osc_set_path(buf_ptr, item->path);
				buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

				buf_ptr = rpn_run(buf_ptr, item, &stack);
			}
			buf_ptr = osc_end_bundle_item(buf_ptr, itm);
		}
		else if(item->dest == RPN_NONE)
			break;
	}
	
	return buf_ptr;
}

static osc_data_t *
custom_engine_set_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = x;
	stack.z = y;

	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	Custom_Item *item;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		item = &items[i];
		if(item->dest == RPN_SET)
		{
			buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
			{
				buf_ptr = osc_set_path(buf_ptr, item->path);
				buf_ptr = osc_set_fmt(buf_ptr, item->fmt);

				buf_ptr = rpn_run(buf_ptr, item, &stack);
			}
			buf_ptr = osc_end_bundle_item(buf_ptr, itm);
		}
		else if(item->dest == RPN_NONE)
			break;
	}
	
	return buf_ptr;
}

CMC_Engine custom_engine = {
	NULL,
	custom_engine_frame_cb,
	custom_engine_on_cb,
	custom_engine_off_cb,
	custom_engine_set_cb,
	custom_engine_end_cb
};

/*
 * Config
 */
static uint_fast8_t
_custom_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.custom.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_custom_reset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	uint_fast8_t i;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		Custom_Item *item = &items[i];

		item->dest = RPN_NONE;
		item->path[0] = '\0';
		item->fmt[0] = '\0';
		item->vm.inst[0] = RPN_TERMINATOR;
	}

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);
	return 1;
}

static const OSC_Query_Value custom_append_destination_args_values [] = {
	[RPN_FRAME]	= { .s = "frame" },
	[RPN_ON]	= { .s = "on" },
	[RPN_OFF]	= { .s = "off" },
	[RPN_SET]	= { .s = "set" },
	[RPN_END]	= { .s = "end" },
	[RPN_IDLE]	= { .s = "idle" }
};

static uint_fast8_t
_custom_append(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1)
	{
		//TODO this is write only for now 
		//size = CONFIG_SUCCESS("isss", uuid, path, item->path, item->fmt);
	}
	else
	{
		uint_fast8_t i;
		Custom_Item *item;
		for(i=0; i<CUSTOM_MAX_EXPR; i++)
		{
			item = &items[i];
			if(item->dest == RPN_NONE)
				break;
		}

		const char *dest;
		const char *opath;
		const char *argv;

		buf_ptr = osc_get_string(buf_ptr, &dest);
		buf_ptr = osc_get_string(buf_ptr, &opath);
		buf_ptr = osc_get_string(buf_ptr, &argv);

		if( (item->dest == RPN_NONE) && osc_check_path(opath) && rpn_compile(argv, item) )
		{
			for(i=0; i<sizeof(custom_append_destination_args_values)/sizeof(OSC_Query_Value); i++)
				if(!strcmp(dest, custom_append_destination_args_values[i].s))
				{
					item->dest = i;
					break;
				}
			strcpy(item->path, opath);
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "parse error and/or stack under/overflow");
	}

	CONFIG_SEND(size);

	return 1;
}

static const OSC_Query_Argument custom_append_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("Destination", OSC_QUERY_MODE_W, custom_append_destination_args_values),
	OSC_QUERY_ARGUMENT_STRING("Name", OSC_QUERY_MODE_W, CUSTOM_PATH_LEN),
	OSC_QUERY_ARGUMENT_STRING("Arguments", OSC_QUERY_MODE_W, CUSTOM_ARGS_LEN)
};

/*
 * Query
 */

const OSC_Query_Item custom_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _custom_enabled, config_boolean_args),

	OSC_QUERY_ITEM_METHOD("reset", "Reset", _custom_reset, NULL),
	OSC_QUERY_ITEM_METHOD("append", "Append format", _custom_append, custom_append_args)
};
