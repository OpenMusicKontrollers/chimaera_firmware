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

#ifndef _POSC_H_
#define _POSC_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <netdef.h>
#include <armfix.h>
#include <math.h>
#include <string.h>

/*
 * Definitions
 */

#define quads(size) (((((size_t)size-1) & ~0x3) >> 2) + 1)
#define round_to_four_bytes(size) (quads((size_t)size) << 2)

typedef union _swap32_t swap32_t;
typedef union _swap64_t swap64_t;
typedef uint8_t osc_data_t;
typedef fix_32_32_t OSC_Timetag;
typedef struct _OSC_Blob OSC_Blob;
typedef struct _OSC_Method OSC_Method;

typedef uint_fast8_t (*OSC_Method_Cb)(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *arg);

typedef enum _OSC_Type {
	OSC_END = 0,

	OSC_INT32 = 'i',
	OSC_FLOAT = 'f',
	OSC_STRING = 's',
	OSC_BLOB = 'b',

	OSC_TRUE = 'T',
	OSC_FALSE = 'F',
	OSC_NIL = 'N',
	OSC_BANG = 'I',

	OSC_DOUBLE = 'd',
	OSC_INT64 = 'h',
	OSC_TIMETAG = 't',

	OSC_SYMBOL = 'S',
	OSC_CHAR = 'c',
	OSC_MIDI = 'm'
} OSC_Type;

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

struct _OSC_Blob {
	int32_t size;
	uint8_t *payload;
};

struct _OSC_Method {
	const char *path;
	const char *fmt;
	OSC_Method_Cb cb;
};

/*
 * Constants
 */

#define OSC_IMMEDIATE	(1ULLK >> 32)

/*
 * Methods
 */

int osc_check_path(const char *path);
int osc_check_fmt(const char *format, int offset);

int osc_method_match(OSC_Method *methods, const char *path, const char *fmt);
void osc_method_dispatch(osc_data_t *buf, size_t size, const OSC_Method *methods);
int osc_message_check(osc_data_t *buf, size_t size);
int osc_bundle_check(osc_data_t *buf, size_t size);
int osc_packet_check(osc_data_t *buf, size_t size);

// OSC object lengths
extern inline size_t
osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

extern inline size_t
osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

extern inline size_t
osc_bloblen(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return 4 + round_to_four_bytes(s.i);
}

extern inline size_t
osc_blobsize(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return s.i;
}

// get OSC arguments from raw buffer
extern inline osc_data_t *
osc_get_path(osc_data_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + osc_strlen(*path);
}

extern inline osc_data_t *
osc_get_fmt(osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + osc_strlen(*fmt);
}

extern inline osc_data_t *
osc_get_int32(osc_data_t *buf, int32_t *i)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

extern inline osc_data_t *
osc_get_float(osc_data_t *buf, float *f)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

extern inline osc_data_t *
osc_get_string(osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + osc_strlen(*s);
}

extern inline osc_data_t *
osc_get_blob(osc_data_t *buf, OSC_Blob *b)
{
	b->size = osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + 4 + round_to_four_bytes(b->size);
}

extern inline osc_data_t *
osc_get_int64(osc_data_t *buf, int64_t *h)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*h = s1.h;
	return buf + 8;
}

extern inline osc_data_t *
osc_get_double(osc_data_t *buf, double *d)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*d = s1.d;
	return buf + 8;
}

extern inline osc_data_t *
osc_get_timetag(osc_data_t *buf, uint64_t *t)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*t = s1.t;
	return buf + 8;
}

extern inline osc_data_t *
osc_get_symbol(osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + osc_strlen(*S);
}

extern inline osc_data_t *
osc_get_char(osc_data_t *buf, char *c)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*c = s.i & 0xff;
	return buf + 4;
}

extern inline osc_data_t *
osc_get_midi(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

// write OSC argument to raw buffer
extern inline osc_data_t *
osc_set_path(osc_data_t *buf, const char *path)
{
	size_t len = osc_strlen(path);
	strncpy((char *)buf, path, len);
	return buf + len;
}

extern inline osc_data_t *
osc_set_fmt(osc_data_t *buf, const char *fmt)
{
	size_t len = osc_fmtlen(fmt);
	*buf++ = ',';
	strncpy((char *)buf, fmt, len);
	return buf + len;
}

extern inline osc_data_t *
osc_set_int32(osc_data_t *buf, int32_t i)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = i;
	s->u = htonl(s->u);
	return buf + 4;
}

extern inline osc_data_t *
osc_set_float(osc_data_t *buf, float f)
{
	swap32_t *s = (swap32_t *)buf;
	s->f = f;
	s->u = htonl(s->u);
	return buf + 4;
}

extern inline osc_data_t *
osc_set_string(osc_data_t *buf, const char *s)
{
	size_t len = osc_strlen(s);
	strncpy((char *)buf, s, len);
	return buf + len;
}

extern inline osc_data_t *
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

extern inline osc_data_t *
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

extern inline osc_data_t *
osc_set_int64(osc_data_t *buf, int64_t h)
{
	swap64_t s0 = { .h = h };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

extern inline osc_data_t *
osc_set_double(osc_data_t *buf, double d)
{
	swap64_t s0 = { .d = d };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

extern inline osc_data_t *
osc_set_timetag(osc_data_t *buf, uint64_t t)
{
	swap64_t s0 = { .t = t };
	swap64_t *s1 = (swap64_t *)buf;
	s1->u.upper = htonl(s0.u.lower);
	s1->u.lower = htonl(s0.u.upper);
	return buf + 8;
}

extern inline osc_data_t *
osc_set_symbol(osc_data_t *buf, const char *S)
{
	size_t len = osc_strlen(S);
	strncpy((char *)buf, S, len);
	return buf + len;
}

extern inline osc_data_t *
osc_set_char(osc_data_t *buf, char c)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = c;
	s->u = htonl(s->u);
	return buf + 4;
}

extern inline osc_data_t *
osc_set_midi(osc_data_t *buf, uint8_t *m)
{
	buf[0] = m[0];
	buf[1] = m[1];
	buf[2] = m[2];
	buf[3] = m[3];
	return buf + 4;
}

extern inline osc_data_t *
osc_set_midi_inline(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

// create bundle
extern inline osc_data_t *
osc_start_bundle(osc_data_t *buf, OSC_Timetag timetag, osc_data_t **bndl)
{
	strncpy(buf, "#bundle", 8);
	osc_set_timetag(buf + 8, timetag);
	return buf + 16;
}

extern inline osc_data_t *
osc_end_bundle(osc_data_t *buf, osc_data_t *bndl)
{
	size_t len = buf - (bndl + 16);
	if(len > 0)
		return buf;
	else // empty bundle
		return bndl;
}

// create item
extern inline osc_data_t *
osc_start_bundle_item(osc_data_t *buf, osc_data_t **itm)
{
	*itm = buf;
	return buf + 4;
}

extern inline osc_data_t *
osc_end_bundle_item(osc_data_t *buf, osc_data_t *itm)
{
	size_t len = buf - (itm + 4);
	if(len > 0)
	{
		osc_set_int32(itm, len);
		return buf;
	}
	else // empty item
		return itm;
}

osc_data_t *osc_vararg_set(osc_data_t *buf, const char *path, const char *fmt, ...);
osc_data_t *osc_varlist_set(osc_data_t *buf, const char *path, const char *fmt, va_list args);

#endif // _POSC_H_
