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

#include <osc.h>

#include <chimaera.h>
#include <debug.h>

typedef union _swap32_t swap32_t;
typedef union _swap64_t swap64_t;

union _swap32_t {
	uint32_t u;

	int32_t i;
	float f;
};

union _swap64_t {
	struct {
		uint32_t upper;
		uint32_t lower;
	} u;

	int64_t h;
	OSC_Timetag t;
	double d;
};

// characters not allowed in OSC path string
static const char invalid_path_chars [] = {
	' ', '#',
	'\0'
};

// allowed characters in OSC format string
static const char valid_format_chars [] = {
	OSC_INT32, OSC_FLOAT, OSC_STRING, OSC_BLOB,
	OSC_TRUE, OSC_FALSE, OSC_NIL, OSC_BANG,
	OSC_INT64, OSC_DOUBLE, OSC_TIMETAG,
	OSC_SYMBOL, OSC_CHAR, OSC_MIDI,
	'\0'
};

// check for valid path string
int
osc_check_path(const char *path)
{
	const char *ptr;

	if(path[0] != '/')
		return 0;

	for(ptr=path+1; *ptr!='\0'; ptr++)
		if( (isprint(*ptr) == 0) || (strchr(invalid_path_chars, *ptr) != NULL) )
			return 0;

	return 1;
}

// check for valid format string 
int
osc_check_fmt(const char *format, int offset)
{
	const char *ptr;

	if(offset)
		if(format[0] != ',')
			return 0;

	for(ptr=format+offset; *ptr!='\0'; ptr++)
		if(strchr(valid_format_chars, *ptr) == NULL)
			return 0;

	return 1;
}

int
osc_method_match(OSC_Method *methods, const char *path, const char *fmt)
{
	OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			return 1;
	return 0;
}

static void
_osc_method_dispatch_message(osc_data_t *buf, size_t size, const OSC_Method *methods)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = osc_get_path(ptr, &path);
	ptr = osc_get_fmt(ptr, &fmt);

	const OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			if(meth->cb(path, fmt+1, strlen(fmt)-1, ptr))
				break;
}

static void
_osc_method_dispatch_bundle(osc_data_t *buf, size_t size, const OSC_Method *methods)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	ptr += 16; // skip bundle header

	while(ptr < end)
	{
		int32_t len = ntohl(*((int32_t *)ptr));
		ptr += sizeof(int32_t);
		switch(*ptr)
		{
			case '#':
				_osc_method_dispatch_bundle(ptr, len, methods);
				break;
			case '/':
				_osc_method_dispatch_message(ptr, len, methods);
				break;
		}
		ptr += len;
	}
}

void
osc_method_dispatch(osc_data_t *buf, size_t size, const OSC_Method *methods)
{
	switch(*buf)
	{
		case '#':
			_osc_method_dispatch_bundle(buf, size, methods);
			break;
		case '/':
			_osc_method_dispatch_message(buf, size, methods);
			break;
	}
}

int
osc_message_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = osc_get_path(ptr, &path);
	if( (ptr > end) || !osc_check_path(path) )
		return 0;

	ptr = osc_get_fmt(ptr, &fmt);
	if( (ptr > end) || !osc_check_fmt(fmt, 1) )
		return 0;

	const char *type;
	for(type=fmt+1; (*type!='\0') && (ptr <= end); type++)
	{
		switch(*type)
		{
			case OSC_INT32:
			case OSC_FLOAT:
			case OSC_MIDI:
			case OSC_CHAR:
				ptr += 4;
				break;

			case OSC_STRING:
			case OSC_SYMBOL:
				ptr += osc_strlen((const char *)ptr);
				break;

			case OSC_BLOB:
				ptr += osc_bloblen(ptr);
				break;

			case OSC_INT64:
			case OSC_DOUBLE:
			case OSC_TIMETAG:
				ptr += 8;
				break;

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				break;
		}
	}

	return ptr == end;
}

int
osc_bundle_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;
	
	if(strncmp((char *)ptr, "#bundle", 8)) // bundle header valid?
		return 0;
	ptr += 16; // skip bundle header

	while(ptr < end)
	{
		int32_t *len = (int32_t *)ptr;
		int32_t hlen = htonl(*len);
		ptr += sizeof(int32_t);

		switch(*ptr)
		{
			case '#':
				if(!osc_bundle_check(ptr, hlen))
					return 0;
				break;
			case '/':
				if(!osc_message_check(ptr, hlen))
					return 0;
				break;
			default:
				return 0;
		}
		ptr += hlen;
	}

	return ptr == end;
}

int
osc_packet_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	
	switch(*ptr)
	{
		case '#':
			if(!osc_bundle_check(ptr, size))
				return 0;
			break;
		case '/':
			if(!osc_message_check(ptr, size))
				return 0;
			break;
		default:
			return 0;
	}

	return 1;
}

size_t
osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

size_t
osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

size_t
osc_bloblen(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return 4 + round_to_four_bytes(s.i);
}

size_t
osc_blobsize(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return s.i;
}

osc_data_t *
osc_get_path(osc_data_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + osc_strlen(*path);
}

osc_data_t *
osc_get_fmt(osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + osc_strlen(*fmt);
}

osc_data_t *
osc_get_int32(osc_data_t *buf, int32_t *i)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

osc_data_t *
osc_get_float(osc_data_t *buf, float *f)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

osc_data_t *
osc_get_string(osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + osc_strlen(*s);
}

osc_data_t *
osc_get_blob(osc_data_t *buf, OSC_Blob *b)
{
	b->size = osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + 4 + round_to_four_bytes(b->size);
}

osc_data_t *
osc_get_int64(osc_data_t *buf, int64_t *h)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*h = s1.h;
	return buf + 8;
}

osc_data_t *
osc_get_double(osc_data_t *buf, double *d)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*d = s1.d;
	return buf + 8;
}

osc_data_t *
osc_get_timetag(osc_data_t *buf, uint64_t *t)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*t = s1.t;
	return buf + 8;
}

osc_data_t *
osc_get_symbol(osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + osc_strlen(*S);
}

osc_data_t *
osc_get_char(osc_data_t *buf, char *c)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*c = s.i & 0xff;
	return buf + 4;
}

osc_data_t *
osc_get_midi(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

osc_data_t *
osc_start_preamble(osc_data_t *buf, osc_data_t **itm)
{
	*itm = buf;
	return buf + 4;
}

osc_data_t *
osc_end_preamble(osc_data_t *buf, osc_data_t *itm)
{
	swap32_t *s = (swap32_t *)itm;
	s->i = buf - (itm + 4);
	s->u = htonl(s->u);
	return buf;
}

osc_data_t *
osc_start_bundle(osc_data_t *buf, OSC_Timetag timetag)
{
	strncpy(buf, "#bundle", 8);
	swap64_t s0 = { .t = timetag };
	swap64_t *s1 = (swap64_t *)(buf + 8);
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 16;
}

osc_data_t *
osc_start_item_fixed(osc_data_t *buf, int32_t size)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = size;
	s->u = htonl(s->u);
	return buf + 4;
}

osc_data_t *
osc_start_item_variable(osc_data_t *buf, osc_data_t **itm)
{
	*itm = buf;
	return buf + 4;
}

osc_data_t *
osc_end_item_variable(osc_data_t *buf, osc_data_t *itm)
{
	swap32_t *s = (swap32_t *)itm;
	s->i = buf - (itm + 4);
	s->u = htonl(s->u);
	return buf;
}

osc_data_t *
osc_set_path(osc_data_t *buf, const char *path)
{
	size_t len = osc_strlen(path);
	strncpy((char *)buf, path, len);
	return buf + len;
}

osc_data_t *
osc_set_fmt(osc_data_t *buf, const char *fmt)
{
	size_t len = osc_fmtlen(fmt);
	*buf++ = ',';
	strncpy((char *)buf, fmt, len);
	return buf + len;
}

osc_data_t *
osc_set_int32(osc_data_t *buf, int32_t i)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = i;
	s->u = htonl(s->u);
	return buf + 4;
}

osc_data_t *
osc_set_float(osc_data_t *buf, float f)
{
	swap32_t *s = (swap32_t *)buf;
	s->f = f;
	s->u = htonl(s->u);
	return buf + 4;
}

osc_data_t *
osc_set_string(osc_data_t *buf, const char *s)
{
	size_t len = osc_strlen(s);
	strncpy((char *)buf, s, len);
	return buf + len;
}

osc_data_t *
osc_set_blob(osc_data_t *buf, int32_t size, void *payload)
{
	size_t len = round_to_four_bytes(size);
	swap32_t *s = (swap32_t *)buf;
	s->i = size;
	s->u = htonl(s->u);
	buf += 4;
	memcpy(buf, payload, size);
	memset(buf+size, '\0', len-size); // zero padding
	return buf + len;
}

osc_data_t *
osc_set_blob_inline(osc_data_t *buf, int32_t size, void **payload)
{
	size_t len = round_to_four_bytes(size);
	swap32_t *s = (swap32_t *)buf;
	s->i = size;
	s->u = htonl(s->u);
	buf += 4;
	*payload = buf;
	memset(buf+size, '\0', len-size); // zero padding
	return buf + len;
}

osc_data_t *
osc_set_int64(osc_data_t *buf, int64_t h)
{
	swap64_t s0 = { .h = h };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

osc_data_t *
osc_set_double(osc_data_t *buf, double d)
{
	swap64_t s0 = { .d = d };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

osc_data_t *
osc_set_timetag(osc_data_t *buf, uint64_t t)
{
	swap64_t s0 = { .t = t };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

osc_data_t *
osc_set_symbol(osc_data_t *buf, const char *S)
{
	size_t len = osc_strlen(S);
	strncpy((char *)buf, S, len);
	return buf + len;
}

osc_data_t *
osc_set_char(osc_data_t *buf, char c)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = c;
	s->u = htonl(s->u);
	return buf + 4;
}

osc_data_t *
osc_set_midi(osc_data_t *buf, uint8_t *m)
{
	buf[0] = m[0];
	buf[1] = m[1];
	buf[2] = m[2];
	buf[3] = m[3];
	return buf + 4;
}

osc_data_t *
osc_set_midi_inline(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

osc_data_t *
osc_vararg_set(osc_data_t *buf, const char *path, const char *fmt, ...)
{
	osc_data_t *buf_ptr = buf;

  va_list args;

  va_start(args, fmt);
	buf_ptr = osc_varlist_set(buf_ptr, path, fmt, args);

  va_end(args);

	return buf_ptr;
}

osc_data_t *
osc_varlist_set(osc_data_t *buf, const char *path, const char *fmt, va_list args)
{
	osc_data_t *buf_ptr = buf;

	if(!(buf_ptr = osc_set_path(buf_ptr, path)))
		return 0;
	if(!(buf_ptr = osc_set_fmt(buf_ptr, fmt)))
		return 0;

  const char *type;
  for(type=fmt; *type != '\0'; type++)
		switch(*type)
		{
			case OSC_INT32:
				buf_ptr = osc_set_int32(buf_ptr, va_arg(args, int32_t));
				break;
			case OSC_FLOAT:
				buf_ptr = osc_set_float(buf_ptr, (float)va_arg(args, double));
				break;
			case OSC_STRING:
				buf_ptr = osc_set_string(buf_ptr, va_arg(args, char *));
				break;
			case OSC_BLOB:
				buf_ptr = osc_set_blob(buf_ptr, va_arg(args, int32_t), va_arg(args, void *));
				break;

			case OSC_INT64:
				buf_ptr = osc_set_int64(buf_ptr, va_arg(args, int64_t));
				break;
			case OSC_DOUBLE:
				buf_ptr = osc_set_double(buf_ptr, va_arg(args, double));
				break;
			case OSC_TIMETAG:
				buf_ptr = osc_set_timetag(buf_ptr, va_arg(args, uint64_t));
				break;

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				break;

			case OSC_SYMBOL:
				buf_ptr = osc_set_symbol(buf_ptr, va_arg(args, char *));
				break;
			case OSC_CHAR:
				buf_ptr = osc_set_char(buf_ptr, (char)va_arg(args, int));
				break;
			case OSC_MIDI:
				buf_ptr = osc_set_midi(buf_ptr, va_arg(args, uint8_t *));
				break;

			default:
				//TODO report error
				break;
		}

	return buf_ptr;
}
