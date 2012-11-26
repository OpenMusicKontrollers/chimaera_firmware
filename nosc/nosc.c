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

nOSC_Message dispatch_msg = {
	.path = "/dispatch",
	.args = { //FIXME make this configurable with OSC_ARGS_MAX
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end
	}
};

nOSC_Message vararg_msg = {
	.path = "/vararg",
	.args = { //FIXME make this configurable with OSC_ARGS_MAX
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end,
		nosc_end
	}
};

static const char *bundle = "#bundle";

/*
 * Method
 */
void
_nosc_method_message_dispatch (nOSC_Method *meth)
{
	nOSC_Message *msg = &dispatch_msg;
	nOSC_Method *ptr = meth;
	while (ptr->cb)
	{
		// raw matches only of path and format strings
		if ( !ptr->path || !strcmp (ptr->path, msg->path))
		{
			char fmt [32]; //FIXME how big an array!
			nOSC_Arg *arg;
			int i;
			for (i=0; (arg = &msg->args[i])->type != nOSC_END; i++)
				fmt[i] = arg->type;
			fmt[i] = '\0';

			if ( !ptr->fmt || !strcmp (ptr->fmt, fmt))
			{
				uint8_t res = ptr->cb (ptr->path, ptr->fmt, i, msg->args);

				if (res) // return when handled
					return;
			}
		}
		ptr++;
	}
}

void
nosc_method_dispatch (nOSC_Method *meth, uint8_t *buf, uint16_t size)
{
	nOSC_Message *msg;

	if (buf[0] == '#') // check whether we are a bundle
	{
		int32_t msg_size;
		nOSC_Message *msg;

		uint8_t *buf_ptr = buf;

		buf_ptr += 8; // skip "#bundle"
		buf_ptr += 8; // XXX timestamp is ignored fo now and messages dispatched immediately, we have too little memory to store arbitrary amounts of messages for later dispatching

		while (buf_ptr-buf < size)
		{
			msg_size = (int32_t) *buf_ptr;
			buf_ptr += 4;

			_nosc_message_deserialize (buf_ptr, msg_size);
			_nosc_method_message_dispatch (meth);
			buf_ptr += msg_size;
		}
	}
	else if (buf[0] == '/') // check whether we are a message
	{
		_nosc_message_deserialize (buf, size);
		_nosc_method_message_dispatch (meth);
	}
}

void
_nosc_message_deserialize (uint8_t *buf, uint16_t size)
{
	nOSC_Message *msg = &dispatch_msg;

	uint8_t *buf_ptr = buf;
	uint8_t len;

	// find path
	msg->path = buf_ptr;
	len = strlen (msg->path)+1;
	if (len%4)
		len += 4 - len%4;
	buf_ptr += len;

	// find format
	char *fmt = buf_ptr;
	len = strlen (fmt)+1;
	if (len%4)
		len += 4 - len%4;
	buf_ptr += len;
	
	fmt++; // skip ','

	char *type = fmt;
	uint8_t pos;
	while (*type != '\0')
	{
		pos = type - fmt;
		switch (*type)
		{
			case nOSC_INT32:
				nosc_message_set_int32 (msg, pos, ref_ntohl (buf_ptr));
				buf_ptr += 4;
				break;
			case nOSC_FLOAT:
				nosc_message_set_float (msg, pos, 0.0);
				msg->args[pos].val.i = ref_ntohl (buf_ptr);
				buf_ptr += 4;
				break;
			case nOSC_STRING:
				nosc_message_set_string (msg, pos, buf_ptr);
				len = strlen (buf_ptr)+1;
				if (len%4)
					len += 4 - len%4;
				buf_ptr += len;
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

			case nOSC_TIMESTAMP:
				nosc_message_add_timestamp (msg, pos, ref_ntohll (buf_ptr));
				buf_ptr += 8;
				break;
		}
		type++;
	}
}

/*
 * Bundle
 */

uint16_t
nosc_bundle_serialize (nOSC_Message **bund, uint64_t timestamp, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	memcpy (buf_ptr, bundle, 8);
	buf_ptr += 8;

	ref_htonll (buf_ptr, timestamp);
	buf_ptr += 8;

	nOSC_Message *msg;
	for (msg=bund[0]; msg!=NULL; msg++)
	{
		uint16_t msg_size;
		int32_t i;

		msg_size = nosc_message_serialize (msg, buf_ptr+4);
		ref_htonl (buf_ptr, msg_size);
		buf_ptr += msg_size + 4;
	}

	return buf_ptr - buf;
}

/*
 * Message
 */

void
nosc_message_set_int32 (nOSC_Message *msg, uint8_t pos, int32_t i)
{
	msg->args[pos].type = nOSC_INT32;
	msg->args[pos].val.i = i;
}

void
nosc_message_set_float (nOSC_Message *msg, uint8_t pos, float f)
{
	msg->args[pos].type = nOSC_FLOAT;
	msg->args[pos].val.f = f;
}

void
nosc_message_set_string (nOSC_Message *msg, uint8_t pos, char *s)
{
	msg->args[pos].type = nOSC_STRING;
	msg->args[pos].val.s = s;
}

void
nosc_message_set_true (nOSC_Message *msg, uint8_t pos)
{
	msg->args[pos].type = nOSC_TRUE;
}

void
nosc_message_set_false (nOSC_Message *msg, uint8_t pos)
{
	msg->args[pos].type = nOSC_FALSE;
}

void
nosc_message_set_nil (nOSC_Message *msg, uint8_t pos)
{
	msg->args[pos].type = nOSC_NIL;
}

void
nosc_message_set_infty (nOSC_Message *msg, uint8_t pos)
{
	msg->args[pos].type = nOSC_INFTY;
}

void
nosc_message_set_timestamp (nOSC_Message *msg, uint8_t pos, uint64_t t)
{
	msg->args[pos].type = nOSC_TIMESTAMP;
	msg->args[pos].val.t = t;
}

/*
 * (de)serialization
 */

uint16_t
nosc_message_serialize (nOSC_Message *msg, uint8_t *buf)
{
	uint8_t i;
	nOSC_Arg *arg;
	uint16_t len;

	// resetting buf_ptr to start of buf
	uint8_t *buf_ptr = buf;

	// write path
	len = strlen (msg->path) + 1;
	memcpy (buf_ptr, msg->path, len);
	buf_ptr += len;
	if (len%4)
		for (i=len%4; i<4; i++)
			*buf_ptr++ = '\0';

	// write format
	*buf_ptr++ = ',';
	len = 1;
	i = 0;
	for (i=0; (arg = &msg->args[i])->type != nOSC_END; i++)
	{
		*buf_ptr++ = arg->type;
		len++;
	}
	*buf_ptr++ = '\0';
	len++;
	if (len%4)
		for (i=len%4; i<4; i++)
			*buf_ptr++ = '\0';

	// write arguments
	for (i=0; (arg = &msg->args[i])->type != nOSC_END; i++)
		switch (arg->type)
		{
			case nOSC_INT32:
			case nOSC_FLOAT:
				ref_htonl (buf_ptr, arg->val.i);
				buf_ptr += 4;
				break;

			case nOSC_STRING:
			{
				uint8_t i;
				char *s = arg->val.s;
				uint16_t len = strlen (s) + 1;
				memcpy (buf_ptr, s, len);
				buf_ptr += len;
				if (len%4)
					for (i=len%4; i<4; i++)
						*buf_ptr++ = '\0';
				break;
			}

			case nOSC_TRUE:
			case nOSC_FALSE:
			case nOSC_NIL:
			case nOSC_INFTY:
				break;

			case nOSC_TIMESTAMP:
				ref_htonll (buf_ptr, arg->val.h);
				buf_ptr += 8;
				break;
		}

	return buf_ptr - buf;
}

uint16_t
nosc_message_vararg_serialize (uint8_t *buf, char *path, char *fmt, ...)
{
	nOSC_Message *msg = &vararg_msg;
	msg->path = path;

  va_list args;
  va_start (args, fmt);

  const char *p;
	uint8_t pos;
  for (p = fmt; *p != '\0'; p++)
	{
		pos = p - fmt;
		switch (*p)
		{
			case nOSC_INT32:
				nosc_message_set_int32 (msg, pos, va_arg (args, int32_t));
        break;
			case nOSC_FLOAT:
				 nosc_message_set_float (msg, pos, (float) va_arg (args, double));
        break;
			case nOSC_STRING:
				nosc_message_set_string (msg, pos, va_arg (args, char *));
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

			case nOSC_TIMESTAMP:
				nosc_message_set_timestamp (msg, pos, va_arg (args, uint64_t));
        break;
		}
	}
  va_end (args);


	uint16_t size = nosc_message_serialize (msg, buf);

	return size;
}
