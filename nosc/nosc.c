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

#include <nosc.h>

#include <chimaera.h>
#include <debug.h>

/*
 * static variabled
 */

static nOSC_Arg dispatch_msg [OSC_ARGS_MAX];
static nOSC_Arg vararg_msg [OSC_ARGS_MAX];

static const char *bundle_str = "#bundle";

/*
 * Method
 */

static inline __always_inline uint_fast8_t
_pattern_match(char *pattern , char *string)
{
	char *qm;

	// handle wildcard
	if( (qm = strchr(pattern, '*')) )
		return !strncmp(pattern, string, qm-pattern);
	else
		return !strcmp(pattern, string);
}

static inline __always_inline void
_nosc_method_message_dispatch(nOSC_Method *meth, char *path, char *fmt)
{
	nOSC_Message msg = dispatch_msg;
	nOSC_Method *ptr;

	for(ptr=meth; ptr->cb!=NULL; ptr++)
	{
		// raw matches only of path and format strings
		if( !ptr->path || _pattern_match(ptr->path, path) )
		{
			if(!ptr->fmt || _pattern_match(ptr->fmt, fmt) )
			{
				uint_fast8_t res = ptr->cb(path, fmt, strlen(fmt), msg);

				if(res) // return when handled
					return;
			}
		}
	}
}

void
_nosc_message_deserialize(uint8_t *buf, uint16_t size, char **path, char **fmt)
{
	nOSC_Message msg = dispatch_msg;

	uint8_t *buf_ptr = buf;
	uint_fast8_t len, rem;

	// find path
	*path = buf_ptr;
	len = strlen(*path)+1;
	if(rem=len%4)
		len += 4 - rem;
	buf_ptr += len;

	// find format
	*fmt = buf_ptr;
	len = strlen(*fmt)+1;
	if(rem=len%4)
		len += 4 - rem;
	*fmt = buf_ptr + 1; // skip ','
	buf_ptr += len;

	char *type;
	uint_fast8_t pos;
	for(type=*fmt; *type!='\0'; type++)
	{
		pos = type - *fmt;
		switch(*type)
		{
			case nOSC_INT32:
				nosc_message_set_int32(msg, pos, ref_ntohl(buf_ptr));
				buf_ptr += 4;
				break;
			case nOSC_FLOAT:
				nosc_message_set_float(msg, pos, 0.0);
				msg[pos].i = ref_ntohl(buf_ptr);
				buf_ptr += 4;
				break;
			case nOSC_STRING:
				nosc_message_set_string(msg, pos, buf_ptr);
				len = strlen(buf_ptr)+1;
				if(rem=len%4)
					len += 4 - rem;
				buf_ptr += len;
				break;
			case nOSC_BLOB:
			{
				int32_t size = ref_ntohl(buf_ptr);
				buf_ptr += 4;
				nosc_message_set_blob(msg, pos, size, buf_ptr);
				buf_ptr += size;
				if(rem=size%4) // add zero pad offset
					buf_ptr += 4-rem;
				break;
			}

			case nOSC_TRUE:
				nosc_message_set_true(msg, pos);
				break;
			case nOSC_FALSE:
				nosc_message_set_false(msg, pos);
				break;
			case nOSC_NIL:
				nosc_message_set_nil(msg, pos);
				break;
			case nOSC_INFTY:
				nosc_message_set_infty(msg, pos);
				break;

			case nOSC_DOUBLE:
				nosc_message_set_double(msg, pos, 0.0);
				msg[pos].h = ref_ntohll(buf_ptr);
				buf_ptr += 8;
				break;
			case nOSC_INT64:
				nosc_message_set_int64(msg, pos, ref_ntohll(buf_ptr));
				buf_ptr += 8;
				break;
			case nOSC_TIMESTAMP:
				nosc_message_set_timestamp(msg, pos, 0ULLK);
				msg[pos].h = ref_ntohll(buf_ptr);
				buf_ptr += 8;
				break;

			case nOSC_MIDI:
				nosc_message_set_midi(msg, pos, buf_ptr[0], buf_ptr[1], buf_ptr[2], buf_ptr[3]);
				buf_ptr += 4;
				break;
			case nOSC_SYMBOL:
				nosc_message_set_symbol(msg, pos, buf_ptr);
				len = strlen(buf_ptr)+1;
				if(rem=len%4)
					len += 4 - rem;
				buf_ptr += len;
				break;
			case nOSC_CHAR:
				nosc_message_set_char(msg, pos, buf_ptr[3]);
				buf_ptr += 4;
				break;
		}
	}
}

void
nosc_method_dispatch(nOSC_Method *meth, uint8_t *buf, uint16_t size, nOSC_Bundle_Start_Cb start, nOSC_Bundle_End_Cb end)
{
	if(!strncmp(buf, bundle_str, 8))
	{
		int32_t msg_size;
		uint8_t *buf_ptr = buf;

		buf_ptr += 8; // skip "#bundle"

		if(start) // call start callback with bundle timestamp if existent
		{
			nOSC_Arg arg;
			arg.h = ref_ntohll(buf_ptr);
			start(arg.t);
		}

		buf_ptr += 8; // skip timestamp

		while(buf_ptr-buf < size)
		{
			msg_size = ref_ntohl(buf_ptr);
			buf_ptr += 4;

			// bundles may be nested
			nosc_method_dispatch(meth, buf_ptr, msg_size, start, end);
			buf_ptr += msg_size;
		}

		// call bundle end callback if existent
		if(end)
			end();
	}
	else if(buf[0] == '/') // check whether we are a message
	{
		char *path;
		char *fmt;
		_nosc_message_deserialize(buf, size, &path, &fmt);
		_nosc_method_message_dispatch(meth, path, fmt);
	}
	// else THIS IS NO OSC MESSAGE, IGNORE IT
}

/*
 * Query system
 */

const nOSC_Query_Item *
nosc_query_find(const nOSC_Query_Item *item, const char *path, int_fast8_t argc)
{
	// is array element?
	if(argc >= 0)
	{
		char str [32]; //TODO how big?
		sprintf(str, item->path, argc);
		if(!strcmp(path, str))
			return item;
	}
	else if(!strcmp(path, item->path))
		return item;

	const char *end = strchr(path, '/');
	if(!end)
		return NULL;

	if(strncmp(path, item->path, end-path))
		return NULL;

	if(item->type == nOSC_QUERY_NODE)
	{
		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const nOSC_Query_Item *sub = &item->item.node.tree[i];
			const nOSC_Query_Item *subsub = nosc_query_find(sub, end+1, -1);
			if(subsub)
				return subsub;
		}
	}
	else if(item->type == nOSC_QUERY_ARRAY)
	{
		const nOSC_Query_Item *sub = item->item.node.tree;
		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const nOSC_Query_Item *subsub = nosc_query_find(sub, end+1, i);
			if(subsub)
				return subsub;
		}
	}

	return NULL;
}

uint_fast8_t
nosc_query_check(const nOSC_Query_Item *item, const char *fmt, nOSC_Arg *argv)
{
	uint_fast8_t len = strlen(fmt);
	uint_fast8_t argc = item->item.method.argc;
	const nOSC_Query_Argument *args = item->item.method.args;
	uint_fast8_t i;

	uint_fast8_t argc_w = 0;
	for(i=0; i<argc; i++)
		if(args[i].mode & nOSC_QUERY_MODE_W)
			argc_w++;

	// mandatory arguments and enough of them?
	if( (len != argc_w) && (len != 0) )
		return 0;

	for(i=0; i<len; i++)
	{
		// check for matching argument types TODO maybe allow some argument type translation here?
		if(fmt[i] != args[i].type)
			return 0;

		// check for matching argument ranges(for number arguments only)
		switch(fmt[i])
		{
			case nOSC_INT32:
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					for(j=0; j<args[i].values.argc; j++)
						if(argv[i].i == args[i].values.ptr[j].i)
							return 1;
					return 0;
				}
				else if( (argv[i].i < args[i].range.min.i) || (argv[i].i > args[i].range.max.i) )
					return 0;
				break;
			case nOSC_FLOAT:
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					for(j=0; j<args[i].values.argc; j++)
						if(argv[i].f == args[i].values.ptr[j].f)
							return 1;
					return 0;
				}
				else if( (argv[i].f < args[i].range.min.f) || (argv[i].f > args[i].range.max.f) )
					return 0;
				break;
			case nOSC_STRING:
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					for(j=0; j<args[i].values.argc; j++)
						if(!strcmp(argv[i].s, args[i].values.ptr[j].s))
							return 1;
					return 0;
				}
				else if(strlen(argv[i].s) > args[i].range.max.i)
					return 0;
				break;
			default:
				break;
		}
	}

	return 1;
}

//static const char *inf_s = "1e9999";
//static const char *ninf_s = "-1e9999";
static const char *inf_s = "null";
static const char *ninf_s = "null";
static const char *null_s = "null";

static void
_serialize_float(char *str, float f)
{
	if(isinf(f)) {
		if(f < 0.f)
			sprintf(str, ninf_s);
		else
			sprintf(str, inf_s);
	}
	else if(isnan(f))
		sprintf(str, null_s);
	else
		sprintf(str, "%f", f);
}

void
nosc_query_response(uint8_t *buf, const nOSC_Query_Item *item, const char *path)
{
	*buf++ = '{';

	if(item->type == nOSC_QUERY_NODE)
	{
		sprintf(buf, "\"path\":\"%s\",\"type\":\"node\",\"description\":\"%s\",\"items\":[",
			path, item->description);
		buf += strlen(buf);

		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const nOSC_Query_Item *sub = &item->item.node.tree[i];
			sprintf(buf, "\"%s\"", sub->path);
			buf += strlen(buf);
			if(i < item->item.node.argc-1)
				*buf++ = ',';
		}

		*buf++ = ']';
	}
	else if(item->type == nOSC_QUERY_ARRAY)
	{
		sprintf(buf, "\"path\":\"%s\",\"type\":\"node\",\"description\":\"%s\",\"items\":[",
			path, item->description);
		buf += strlen(buf);

		const nOSC_Query_Item *sub = item->item.node.tree;
		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			*buf++ = '"';
			sprintf(buf, sub->path, i);
			buf += strlen(buf);
			*buf++ = '"';
			if(i < item->item.node.argc-1)
				*buf++ = ',';
		}

		*buf++ = ']';
	}
	else // nOSC_QUERY_METHOD
	{
		char *meth = strrchr(path, '/');
		if(meth && strcmp(item->path, meth+1)) // is array element
		{
			int i;
			sscanf(meth+1, item->path, &i);

			sprintf(buf, "\"path\":\"%s\",\"type\":\"method\",\"description\":\"", path);
			buf += strlen(buf);
			sprintf(buf, item->description, i);
			buf += strlen(buf);
			sprintf(buf, "\",\"arguments\":[");
			buf += strlen(buf);
		}
		else
		{
			sprintf(buf, "\"path\":\"%s\",\"type\":\"method\",\"description\":\"%s\",\"arguments\":[",
				path, item->description);
			buf += strlen(buf);
		}

		uint_fast8_t i;
		for(i=0; i<item->item.method.argc; i++)
		{
			const nOSC_Query_Argument *arg = &item->item.method.args[i];
			sprintf(buf, "{\"type\":\"%c\",\"description\":\"%s\",\"read\":%s,\"write\":%s",
				arg->type, arg->description,
				arg->mode & nOSC_QUERY_MODE_R ? "true" : "false",
				arg->mode & nOSC_QUERY_MODE_W ? "true" : "false");
			buf += strlen(buf);

			switch(arg->type)
			{
				case nOSC_INT32:
					if(arg->values.argc)
					{
						sprintf(buf, ",\"values\":[");
						buf += strlen(buf);
						uint_fast8_t j;
						for(j=0; j<arg->values.argc; j++)
						{
							sprintf(buf, "%i,", arg->values.ptr[j].i);
							buf += strlen(buf);
						}
						buf--;
						*buf++ = ']';
					}
					else // !values
					{
						sprintf(buf, ",\"range\":[%i,%i,%i]", arg->range.min.i, arg->range.max.i, arg->range.step.i);
						buf += strlen(buf);
					}
					break;
				case nOSC_FLOAT:
				{
					char val[32];
					if(arg->values.argc)
					{
						sprintf(buf, ",\"values\":[");
						buf += strlen(buf);
						uint_fast8_t j;
						for(j=0; j<arg->values.argc; j++)
						{
							_serialize_float(val, arg->values.ptr[j].f);
							sprintf(buf, "%s,", val);
							buf += strlen(buf);
						}
						buf--;
						*buf++ = ']';
					}
					else // !values
					{
						sprintf(buf, ",\"range\":[");
						buf += strlen(buf);

						_serialize_float(val, arg->range.min.f);
						sprintf(buf, "%s,", val);
						buf += strlen(buf);

						_serialize_float(val, arg->range.max.f);
						sprintf(buf, "%s,", val);
						buf += strlen(buf);

						_serialize_float(val, arg->range.step.f);
						sprintf(buf, "%s", val);
						buf += strlen(buf);

						*buf++ = ']';
					}
					break;
				}
				case nOSC_STRING:
					if(arg->values.argc)
					{
						sprintf(buf, ",\"values\":[");
						buf += strlen(buf);
						uint_fast8_t j;
						for(j=0; j<arg->values.argc; j++)
						{
							sprintf(buf, "\"%s\",", arg->values.ptr[j].s);
							buf += strlen(buf);
						}
						buf--;
						*buf++ = ']';
					}
					else // !values
					{
						sprintf(buf, ",\"range\":[%i,%i,%i]", arg->range.min.i, arg->range.max.i, arg->range.step.i);
						buf += strlen(buf);
					}
					break;
				//FIXME add other types
				default:
					break;
			}
			*buf++ = '}';
			if(i < item->item.method.argc-1)
				*buf++ = ',';
		}

		*buf++ = ']';
	}

	*buf++ = '}';
	*buf++ = '\0';
}
