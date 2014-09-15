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

#include <oscquery.h>

const OSC_Query_Item *
osc_query_find(const OSC_Query_Item *item, const char *path, int_fast8_t argc)
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

	if(item->type == OSC_QUERY_NODE)
	{
		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const OSC_Query_Item *sub = &item->item.node.tree[i];
			const OSC_Query_Item *subsub = osc_query_find(sub, end+1, -1);
			if(subsub)
				return subsub;
		}
	}
	else if(item->type == OSC_QUERY_ARRAY)
	{
		const OSC_Query_Item *sub = item->item.node.tree;
		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const OSC_Query_Item *subsub = osc_query_find(sub, end+1, i);
			if(subsub)
				return subsub;
		}
	}

	return NULL;
}

uint_fast8_t
osc_query_check(const OSC_Query_Item *item, const char *fmt, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint_fast8_t len = strlen(fmt);
	uint_fast8_t argc = item->item.method.argc;
	const OSC_Query_Argument *args = item->item.method.args;
	uint_fast8_t i;

	uint_fast8_t argc_w = 0;
	for(i=0; i<argc; i++)
		if(args[i].mode & OSC_QUERY_MODE_W)
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
			case OSC_INT32:
			{
				int32_t I;
				buf_ptr = osc_get_int32(buf_ptr, &I);
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					uint_fast8_t found = 0;
					for(j=0; j<args[i].values.argc; j++)
						if(I == args[i].values.ptr[j].i)
						{
							found = 1;
							break;
						}
					if(!found)
						return 0;
				}
				else if( (I < args[i].range.min.i) || (I > args[i].range.max.i) )
					return 0;
				break;
			}
			case OSC_FLOAT:
			{
				float F;
				buf_ptr = osc_get_float(buf_ptr, &F);
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					uint_fast8_t found = 0;
					for(j=0; j<args[i].values.argc; j++)
						if(F == args[i].values.ptr[j].f)
						{
							found = 1;
							break;
						}
					if(!found)
						return 0;
				}
				else if( (F < args[i].range.min.f) || (F > args[i].range.max.f) )
					return 0;
				break;
			}
			case OSC_STRING:
			{
				const char *S;
				buf_ptr = osc_get_string(buf_ptr, &S);
				if(args[i].values.ptr)
				{
					uint_fast8_t j;
					uint_fast8_t found = 0;
					for(j=0; j<args[i].values.argc; j++)
						if(!strcmp(S, args[i].values.ptr[j].s))
						{
							found = 1;
							break;
						}
					if(!found)
						return 0;
				}
				else if((int32_t)strlen(S) > (args[i].range.max.i - 1) ) // including terminating zero
					return 0;
				break;
			}
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
osc_query_response(char *buf, const OSC_Query_Item *item, const char *path)
{
	*buf++ = '{';

	if(item->type == OSC_QUERY_NODE)
	{
		sprintf(buf, "\"path\":\"%s\",\"type\":\"node\",\"description\":\"%s\",\"items\":[",
			path, item->description);
		buf += strlen(buf);

		uint_fast8_t i;
		for(i=0; i<item->item.node.argc; i++)
		{
			const OSC_Query_Item *sub = &item->item.node.tree[i];
			sprintf(buf, "\"%s\"", sub->path);
			buf += strlen(buf);
			if(i < item->item.node.argc-1)
				*buf++ = ',';
		}

		*buf++ = ']';
	}
	else if(item->type == OSC_QUERY_ARRAY)
	{
		sprintf(buf, "\"path\":\"%s\",\"type\":\"node\",\"description\":\"%s\",\"items\":[",
			path, item->description);
		buf += strlen(buf);

		const OSC_Query_Item *sub = item->item.node.tree;
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
	else // OSC_QUERY_METHOD
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
			const OSC_Query_Argument *arg = &item->item.method.args[i];
			sprintf(buf, "{\"type\":\"%c\",\"description\":\"%s\",\"read\":%s,\"write\":%s",
				arg->type, arg->description,
				arg->mode & OSC_QUERY_MODE_R ? "true" : "false",
				arg->mode & OSC_QUERY_MODE_W ? "true" : "false");
			buf += strlen(buf);

			switch(arg->type)
			{
				case OSC_INT32:
					if(arg->values.argc)
					{
						sprintf(buf, ",\"values\":[");
						buf += strlen(buf);
						uint_fast8_t j;
						for(j=0; j<arg->values.argc; j++)
						{
							sprintf(buf, "%li,", arg->values.ptr[j].i);
							buf += strlen(buf);
						}
						buf--;
						*buf++ = ']';
					}
					else // !values
					{
						sprintf(buf, ",\"range\":[%li,%li,%li]", arg->range.min.i, arg->range.max.i, arg->range.step.i);
						buf += strlen(buf);
					}
					break;
				case OSC_FLOAT:
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
				case OSC_STRING:
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
						sprintf(buf, ",\"range\":[%li,%li,%li]", arg->range.min.i, arg->range.max.i, arg->range.step.i);
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
