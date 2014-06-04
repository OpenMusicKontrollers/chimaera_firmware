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

static nOSC_Item custom_bndl [BLOB_MAX];
static char custom_fmt [BLOB_MAX+1];

static Custom_Item *items = config.custom.items;

static RPN_Stack stack;

// global
nOSC_Bundle_Item custom_osc = {
	.bndl = custom_bndl,
	.tt = nOSC_IMMEDIATE,
	.fmt = custom_fmt
};

static Custom_Msg msgs [BLOB_MAX];

static uint_fast8_t custom_tok;

void
custom_init()
{
	// do nothing
}

static void
custom_engine_frame_cb(uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	nOSC_Message msg;

	custom_osc.tt = offset;
	
	custom_tok = 0;

	stack.fid = fid;
	stack.sid = stack.gid = stack.pid = 0;
	stack.x = stack.z = 0.f;

	Custom_Item *item;
	for(item=items; item-items < CUSTOM_MAX_EXPR; item++)
		if(item->dest == RPN_FRAME)
		{
			msg = msgs[custom_tok];

			rpn_run(msg, item, &stack);
			nosc_item_message_set(custom_bndl, custom_tok, msg, item->path, item->fmt);
			custom_fmt[custom_tok++] = nOSC_MESSAGE;
		}
		else if(item->dest == RPN_NONE)
			break;

	custom_fmt[custom_tok] = nOSC_TERM;
}

static void
custom_engine_on_cb(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;

	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = x;
	stack.z = y;

	Custom_Item *item;
	for(item=items; item-items < CUSTOM_MAX_EXPR; item++)
		if(item->dest == RPN_ON)
		{
			msg = msgs[custom_tok];

			rpn_run(msg, item, &stack);
			nosc_item_message_set(custom_bndl, custom_tok, msg, item->path, item->fmt);
			custom_fmt[custom_tok++] = nOSC_MESSAGE;
		}
		else if(item->dest == RPN_NONE)
			break;

	custom_fmt[custom_tok] = nOSC_TERM;
}

static void
custom_engine_off_cb(uint32_t sid, uint16_t gid, uint16_t pid)
{
	nOSC_Message msg;

	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = stack.z = 0.f;

	Custom_Item *item;
	for(item=items; item-items < CUSTOM_MAX_EXPR; item++)
		if(item->dest == RPN_OFF)
		{
			msg = msgs[custom_tok];

			rpn_run(msg, item, &stack);
			nosc_item_message_set(custom_bndl, custom_tok, msg, item->path, item->fmt);
			custom_fmt[custom_tok++] = nOSC_MESSAGE;
		}
		else if(item->dest == RPN_NONE)
			break;

	custom_fmt[custom_tok] = nOSC_TERM;
}

static void
custom_engine_set_cb(uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	nOSC_Message msg;

	stack.sid = sid;
	stack.gid = gid;
	stack.pid = pid;
	stack.x = x;
	stack.z = y;

	Custom_Item *item;
	for(item=items; item-items < CUSTOM_MAX_EXPR; item++)
		if(item->dest == RPN_SET)
		{
			msg = msgs[custom_tok];
			
			rpn_run(msg, item, &stack);
			nosc_item_message_set(custom_bndl, custom_tok, msg, item->path, item->fmt);
			custom_fmt[custom_tok++] = nOSC_MESSAGE;
		}
		else if(item->dest == RPN_NONE)
			break;

	custom_fmt[custom_tok] = nOSC_TERM;
}

CMC_Engine custom_engine = {
	custom_engine_frame_cb,
	custom_engine_on_cb,
	custom_engine_off_cb,
	custom_engine_set_cb
};

/*
 * Config
 */
static uint_fast8_t
_custom_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, args, &config.custom.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_custom_reset(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	uint_fast8_t i;
	for(i=0; i<CUSTOM_MAX_EXPR; i++)
	{
		Custom_Item *item = &items[i];

		item->dest = RPN_NONE;
		item->path[0] = '\0';
		item->vm.inst[0] = RPN_TERMINATOR;
	}

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);
	return 1;
}

static const nOSC_Query_Value custom_append_destination_args_values [] = {
	[RPN_FRAME]	= { .s = "frame" },
	[RPN_ON]	= { .s = "on" },
	[RPN_OFF]	= { .s = "off" },
	[RPN_SET]	= { .s = "set" },
	[RPN_IDLE]	= { .s = "idle" }
};

static uint_fast8_t
_custom_append(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1)
	{
		//TODO this is write only for now 
		//size = CONFIG_SUCCESS("isss", uuid, path, item->path, item->fmt);
	}
	else
	{
		Custom_Item *item;
		for(item=items; item-items < CUSTOM_MAX_EXPR; item++)
			if(item->dest == RPN_NONE)
				break;

		if( (item-items < CUSTOM_MAX_EXPR) && strcmp(args[2].s, "") && rpn_compile(args[3].s, &item->vm) )
		{
			uint_fast8_t i;
			for(i=0; i<sizeof(custom_append_destination_args_values)/sizeof(nOSC_Query_Value); i++)
				if(!strcmp(args[1].s, custom_append_destination_args_values[i].s))
				{
					item->dest = i;
					break;
				}
			strcpy(item->path, args[2].s); // TODO check for valid path
			size = CONFIG_SUCCESS("is", uuid, path);
		}
		else
			size = CONFIG_FAIL("iss", uuid, path, "parse error");
	}

	CONFIG_SEND(size);

	return 1;
}

static const nOSC_Query_Argument custom_append_args [] = {
	nOSC_QUERY_ARGUMENT_STRING_VALUES("Destination", nOSC_QUERY_MODE_W, custom_append_destination_args_values),
	nOSC_QUERY_ARGUMENT_STRING("Name", nOSC_QUERY_MODE_W, CUSTOM_PATH_LEN),
	nOSC_QUERY_ARGUMENT_STRING("Arguments", nOSC_QUERY_MODE_W, CUSTOM_ARGS_LEN)
};

/*
 * Query
 */

const nOSC_Query_Item custom_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _custom_enabled, config_boolean_args),

	nOSC_QUERY_ITEM_METHOD("reset", "Reset", _custom_reset, NULL),
	nOSC_QUERY_ITEM_METHOD("append", "Append format", _custom_append, custom_append_args)
};
