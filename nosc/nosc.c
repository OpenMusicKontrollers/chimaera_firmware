/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

#include <chimutil.h>

#include "nosc_private.h"

/*
 * static variabled
 */

static nOSC_Arg dispatch_msg [OSC_ARGS_MAX+1];
static nOSC_Arg vararg_msg [OSC_ARGS_MAX+1];

static const char *bundle_str = "#bundle";

/*
 * Method
 */

void
_nosc_method_message_dispatch (nOSC_Method *meth, char *path, char *fmt)
{
	nOSC_Message msg = dispatch_msg;
	nOSC_Method *ptr;

	for (ptr=meth; ptr->cb!=NULL; ptr++)
	{
		// raw matches only of path and format strings
		if ( !ptr->path || !strcmp (ptr->path, path))
		{
			if ( !ptr->fmt || !strcmp (ptr->fmt, fmt))
			{
				uint8_t res = ptr->cb (ptr->path, ptr->fmt, strlen (fmt), msg);

				if (res) // return when handled
					return;
			}
		}
	}
}

void
nosc_method_dispatch (nOSC_Method *meth, uint8_t *buf, uint16_t size, nOSC_Bundle_Start_Cb start, nOSC_Bundle_End_Cb end)
{
	if (!strncmp (buf, bundle_str, 8))
	{
		int32_t msg_size;
		uint8_t *buf_ptr = buf;

		buf_ptr += 8; // skip "#bundle"

		if (start) // call start callback with bundle timestamp if existent
		{
			uint64_t timestamp = ref_ntohll (buf_ptr);
			start (timestamp);
		}

		buf_ptr += 8; // skip timestamp

		while (buf_ptr-buf < size)
		{
			msg_size = ref_ntohl (buf_ptr);
			buf_ptr += 4;

			// bundles may be nested
			nosc_method_dispatch (meth, buf_ptr, msg_size, start, end);
			buf_ptr += msg_size;
		}

		// call bundle end callback if existent
		if (end)
			end ();
	}
	else if (buf[0] == '/') // check whether we are a message
	{
		char *path;
		char *fmt;
		_nosc_message_deserialize (buf, size, &path, &fmt);
		_nosc_method_message_dispatch (meth, path, fmt);
	}
	// else THIS IS NO OSC MESSAGE, IGNORE IT
}

void
_nosc_bundle_deserialize (uint8_t *buf, uint16_t size, uint64_t *timestamp)
{
	//FIXME
}

void
_nosc_message_deserialize (uint8_t *buf, uint16_t size, char **path, char **fmt)
{
	nOSC_Message msg = dispatch_msg;

	uint8_t *buf_ptr = buf;
	uint8_t len, rem;

	// find path
	*path = buf_ptr;
	len = strlen (*path)+1;
	if (rem=len%4)
		len += 4 - rem;
	buf_ptr += len;

	// find format
	*fmt = buf_ptr;
	len = strlen (*fmt)+1;
	if (rem=len%4)
		len += 4 - rem;
	*fmt = buf_ptr + 1; // skip ','
	buf_ptr += len;

	char *type;
	uint8_t pos;
	for (type=*fmt; *type!='\0'; type++)
	{
		pos = type - *fmt;
		switch (*type)
		{
			case nOSC_INT32:
				nosc_message_set_int32 (msg, pos, ref_ntohl (buf_ptr));
				buf_ptr += 4;
				break;
			case nOSC_FLOAT:
				nosc_message_set_float (msg, pos, 0.0);
				msg[pos].val.i = ref_ntohl (buf_ptr);
				buf_ptr += 4;
				break;
			case nOSC_STRING:
				nosc_message_set_string (msg, pos, buf_ptr);
				len = strlen (buf_ptr)+1;
				if (rem=len%4)
					len += 4 - rem;
				buf_ptr += len;
				break;
			case nOSC_BLOB:
			{
				int32_t size = ref_ntohl (buf_ptr);
				buf_ptr += 4;
				nosc_message_set_blob (msg, pos, size, buf_ptr);
				buf_ptr += size;
				if (rem=size%4) // add zero pad offset
					buf_ptr += 4-rem;
				break;
			}

			case nOSC_TRUE:
				nosc_message_set_true (msg, pos);
				break;
			case nOSC_FALSE:
				nosc_message_set_false (msg, pos);
				break;
			case nOSC_NIL:
				nosc_message_set_nil (msg, pos);
				break;
			case nOSC_INFTY:
				nosc_message_set_infty (msg, pos);
				break;

			case nOSC_DOUBLE:
				nosc_message_set_double (msg, pos, 0.0);
				msg[pos].val.h = ref_ntohll (buf_ptr);
				buf_ptr += 8;
				break;
			case nOSC_INT64:
				nosc_message_set_int64 (msg, pos, ref_ntohll (buf_ptr));
				buf_ptr += 8;
				break;
			case nOSC_TIMESTAMP:
				nosc_message_set_timestamp (msg, pos, ref_ntohll (buf_ptr));
				buf_ptr += 8;
				break;

			case nOSC_MIDI:
				nosc_message_set_midi (msg, pos, buf_ptr);
				buf_ptr += 4;
				break;
			case nOSC_SYMBOL:
				nosc_message_set_symbol (msg, pos, buf_ptr);
				len = strlen (buf_ptr)+1;
				if (rem=len%4)
					len += 4 - rem;
				buf_ptr += len;
				break;
			case nOSC_CHAR:
				nosc_message_set_char (msg, pos, buf_ptr[3]);
				buf_ptr += 4;
				break;
		}
	}
}

/*
 * Bundle
 */

uint16_t
nosc_bundle_serialize (nOSC_Bundle bund, uint64_t timestamp, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;
	int32_t msg_size;

	memcpy (buf_ptr, bundle_str, 8);
	buf_ptr += 8;

	ref_htonll (buf_ptr, timestamp);
	buf_ptr += 8;

	nOSC_Item *itm;
	for (itm=bund; itm->path!=NULL; itm++)
	{
		msg_size = nosc_message_serialize (itm->msg, itm->path, buf_ptr+4);
		ref_htonl (buf_ptr, msg_size);
		buf_ptr += msg_size + 4;
	}

	return buf_ptr - buf;
}

/*
 * Message
 */

inline void
nosc_message_set_int32 (nOSC_Message msg, uint8_t pos, int32_t i)
{
	msg[pos].type = nOSC_INT32;
	msg[pos].val.i = i;
}

inline void
nosc_message_set_float (nOSC_Message msg, uint8_t pos, float f)
{
	msg[pos].type = nOSC_FLOAT;
	msg[pos].val.f = f;
}

inline void
nosc_message_set_string (nOSC_Message msg, uint8_t pos, char *s)
{
	msg[pos].type = nOSC_STRING;
	msg[pos].val.s = s;
}

inline void
nosc_message_set_blob (nOSC_Message msg, uint8_t pos, int32_t size, uint8_t *data)
{
	msg[pos].type = nOSC_BLOB;
	msg[pos].val.b.size = size;
	msg[pos].val.b.data = data;
}

inline void
nosc_message_set_true (nOSC_Message msg, uint8_t pos)
{
	msg[pos].type = nOSC_TRUE;
}

inline void
nosc_message_set_false (nOSC_Message msg, uint8_t pos)
{
	msg[pos].type = nOSC_FALSE;
}

inline void
nosc_message_set_nil (nOSC_Message msg, uint8_t pos)
{
	msg[pos].type = nOSC_NIL;
}

inline void
nosc_message_set_infty (nOSC_Message msg, uint8_t pos)
{
	msg[pos].type = nOSC_INFTY;
}

inline void
nosc_message_set_double (nOSC_Message msg, uint8_t pos, double d)
{
	msg[pos].type = nOSC_DOUBLE;
	msg[pos].val.d = d;
}

inline void
nosc_message_set_int64 (nOSC_Message msg, uint8_t pos, int64_t h)
{
	msg[pos].type = nOSC_INT64;
	msg[pos].val.h = h;
}

inline void
nosc_message_set_timestamp (nOSC_Message msg, uint8_t pos, uint64_t t)
{
	msg[pos].type = nOSC_TIMESTAMP;
	msg[pos].val.t = t;
}

inline void
nosc_message_set_midi (nOSC_Message msg, uint8_t pos, uint8_t *m)
{
	msg[pos].type = nOSC_MIDI;
	memcpy (msg[pos].val.m, m, 4);
}

inline void
nosc_message_set_symbol (nOSC_Message msg, uint8_t pos, char *S)
{
	msg[pos].type = nOSC_SYMBOL;
	msg[pos].val.S = S;
}

inline void
nosc_message_set_char (nOSC_Message msg, uint8_t pos, char c)
{
	msg[pos].type = nOSC_CHAR;
	msg[pos].val.c = c;
}

inline void
nosc_message_set_end (nOSC_Message msg, uint8_t pos)
{
	msg[pos].type = nOSC_END;
}

/*
 * (de)serialization
 */

uint16_t
nosc_message_serialize (nOSC_Message msg, const char *path, uint8_t *buf)
{
	uint8_t i, rem;
	uint16_t len;
	nOSC_Arg *arg;

	// resetting buf_ptr to start of buf
	uint8_t *buf_ptr = buf;

	// write path
	len = strlen (path) + 1;
	memcpy (buf_ptr, path, len);
	buf_ptr += len;
	if (rem=len%4)
	{
		memset (buf_ptr, '\0', 4-rem);
		buf_ptr += 4-rem;
	}

	// write format
	uint8_t *fmt = buf_ptr;
	*buf_ptr++ = ',';
	for (arg=msg; arg->type!=nOSC_END; arg++)
		*buf_ptr++ = arg->type;
	*buf_ptr++ = '\0';
	len = buf_ptr - fmt;
	if (rem=len%4)
	{
		memset (buf_ptr, '\0', 4-rem);
		buf_ptr += 4-rem;
	}

	// write arguments
	for (arg=msg; arg->type!=nOSC_END; arg++)
		switch (arg->type)
		{
			case nOSC_INT32:
			case nOSC_FLOAT:
				ref_htonl (buf_ptr, arg->val.i);
				buf_ptr += 4;
				break;

			case nOSC_STRING:
				len = strlen (arg->val.s) + 1;
				memcpy (buf_ptr, arg->val.s, len);
				buf_ptr += len;
				if (rem=len%4)
				{
					memset (buf_ptr, '\0', 4-rem);
					buf_ptr += 4-rem;
				}
				break;

			case nOSC_BLOB:
				ref_htonl (buf_ptr, arg->val.b.size);
				buf_ptr += 4;
				memcpy (buf_ptr, arg->val.b.data, arg->val.b.size);
				buf_ptr += arg->val.b.size;
				if (rem=arg->val.b.size%4)
				{
					memset (buf_ptr, '\0', 4-rem);
					buf_ptr += 4-rem;
				}
				break;

			case nOSC_TRUE:
			case nOSC_FALSE:
			case nOSC_NIL:
			case nOSC_INFTY:
				break;

			case nOSC_DOUBLE:
			case nOSC_INT64:
			case nOSC_TIMESTAMP:
			{
				ref_htonll (buf_ptr, arg->val.h);
				//uint64_t tmp = htonll (arg->val.t);
				//memcpy (buf_ptr, &tmp, 8);
				buf_ptr += 8;
				break;
			}

			case nOSC_MIDI:
				memcpy (buf_ptr, arg->val.m, 4);
				buf_ptr += 4;
				break;
			case nOSC_SYMBOL:
				len = strlen (arg->val.S) + 1;
				memcpy (buf_ptr, arg->val.S, len);
				buf_ptr += len;
				if (rem=len%4)
				{
					memset (buf_ptr, '\0', 4-rem);
					buf_ptr += 4-rem;
				}
				break;
			case nOSC_CHAR:
				memset (buf_ptr, '\0', 3);
				buf_ptr[3] = arg->val.c;
				buf_ptr += 4;
				break;
		}

	return buf_ptr - buf;
}

uint16_t
nosc_message_vararg_serialize (uint8_t *buf, const char *path, const char *fmt, ...)
{
	nOSC_Message msg = vararg_msg;

  va_list args;
  va_start (args, fmt);

  const char *p;
	uint8_t pos;
  for (p = fmt; *p != '\0'; p++)
	{
		pos = p - fmt;

		if (pos >= OSC_ARGS_MAX) // ignore arguments exceeding OSC_ARGS_MAX
			break;

		switch (*p)
		{
			case nOSC_INT32:
				nosc_message_set_int32 (msg, pos, va_arg (args, int32_t));
        break;
			case nOSC_FLOAT:
				 nosc_message_set_float (msg, pos, (float) va_arg (args, double)); // float is promoted to double in ...
        break;
			case nOSC_STRING:
				nosc_message_set_string (msg, pos, va_arg (args, char *));
        break;
			case nOSC_BLOB:
				nosc_message_set_blob (msg, pos, va_arg (args, int32_t), va_arg (args, uint8_t *));
				break;

			case nOSC_TRUE:
				nosc_message_set_true (msg, pos);
        break;
			case nOSC_FALSE:
				nosc_message_set_false (msg, pos);
        break;
			case nOSC_NIL:
				nosc_message_set_nil (msg, pos);
        break;
			case nOSC_INFTY:
				nosc_message_set_infty (msg, pos);
        break;

			case nOSC_DOUBLE:
				nosc_message_set_double (msg, pos, va_arg (args, double));
				break;
			case nOSC_INT64:
				nosc_message_set_int64 (msg, pos, va_arg (args, int64_t));
				break;
			case nOSC_TIMESTAMP:
				nosc_message_set_timestamp (msg, pos, va_arg (args, uint64_t));
        break;

			case nOSC_MIDI:
				nosc_message_set_midi (msg, pos, va_arg (args, uint8_t *));
				break;
			case nOSC_SYMBOL:
				nosc_message_set_symbol (msg, pos, va_arg (args, char *));
				break;
			case nOSC_CHAR:
				nosc_message_set_char (msg, pos, (char) va_arg (args, int)); // char is promoted to int in ...
				break;
		}
	}
  va_end (args);

	nosc_message_set_end (msg, pos+1);

	uint16_t size = nosc_message_serialize (msg, path, buf);

	return size;
}
