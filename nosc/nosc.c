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
