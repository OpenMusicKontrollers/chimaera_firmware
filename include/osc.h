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

#define osc_padded_size(size) ( ( (size_t)(size) + 3 ) & ( ~3 ) )

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
	OSC_BLOB_INLINE = 'B',

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
	void *payload;
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

int osc_match_method(OSC_Method *methods, const char *path, const char *fmt);
void osc_dispatch_method(osc_data_t *buf, size_t size, const OSC_Method *methods);
int osc_check_message(osc_data_t *buf, size_t size);
int osc_check_bundle(osc_data_t *buf, size_t size);
int osc_check_packet(osc_data_t *buf, size_t size);

/* Suppress GCC's bogus "no previous prototype for 'FOO'"
   and "no previous declaration for 'FOO'"  diagnostics,
   when FOO is an inline function in the header; see
   <http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54113>.  */
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")

inline size_t
osc_len(osc_data_t *buf, osc_data_t *base)
{
	return buf ? buf - base : 0;
}

// OSC object lengths
inline size_t
osc_strlen(const char *buf)
{
	return osc_padded_size(strlen(buf) + 1);
}

inline size_t
osc_fmtlen(const char *buf)
{
	return osc_padded_size(strlen(buf) + 2) - 1;
}

inline size_t
osc_bloblen(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return 4 + osc_padded_size(s.i);
}

inline size_t
osc_blobsize(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return s.i;
}

// get OSC arguments from raw buffer
inline osc_data_t *
osc_get_path(osc_data_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + osc_strlen(*path);
}

inline osc_data_t *
osc_get_fmt(osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + osc_strlen(*fmt);
}

inline osc_data_t *
osc_get_int32(osc_data_t *buf, int32_t *i)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

inline osc_data_t *
osc_get_float(osc_data_t *buf, float *f)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

inline osc_data_t *
osc_get_string(osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + osc_strlen(*s);
}

inline osc_data_t *
osc_get_blob(osc_data_t *buf, OSC_Blob *b)
{
	b->size = osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + 4 + osc_padded_size(b->size);
}

inline osc_data_t *
osc_get_int64(osc_data_t *buf, int64_t *h)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*h = s1.h;
	return buf + 8;
}

inline osc_data_t *
osc_get_double(osc_data_t *buf, double *d)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*d = s1.d;
	return buf + 8;
}

inline osc_data_t *
osc_get_timetag(osc_data_t *buf, OSC_Timetag *t)
{
	swap64_t *s0 = (swap64_t *)buf;
	swap64_t s1;
	s1.u.lower = ntohl(s0->u.upper);
	s1.u.upper = ntohl(s0->u.lower);
	*t = s1.t;
	return buf + 8;
}

inline osc_data_t *
osc_get_symbol(osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + osc_strlen(*S);
}

inline osc_data_t *
osc_get_char(osc_data_t *buf, char *c)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*c = s.i & 0xff;
	return buf + 4;
}

inline osc_data_t *
osc_get_midi(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

_Pragma("GCC diagnostic pop")

osc_data_t * osc_set_path(osc_data_t *buf, osc_data_t *end, const char *path);
osc_data_t * osc_set_fmt(osc_data_t *buf, osc_data_t *end, const char *fmt);
osc_data_t * osc_set_int32(osc_data_t *buf, osc_data_t *end, int32_t i);
osc_data_t * osc_set_float(osc_data_t *buf, osc_data_t *end, float f);
osc_data_t * osc_set_string(osc_data_t *buf, osc_data_t *end, const char *s);
osc_data_t * osc_set_blob(osc_data_t *buf, osc_data_t *end, int32_t size, void *payload);
osc_data_t * osc_set_blob_inline(osc_data_t *buf, osc_data_t *end, int32_t size, void **payload);
osc_data_t * osc_set_int64(osc_data_t *buf, osc_data_t *end, int64_t h);
osc_data_t * osc_set_double(osc_data_t *buf, osc_data_t *end, double d);
osc_data_t * osc_set_timetag(osc_data_t *buf, osc_data_t *end, OSC_Timetag t);
osc_data_t * osc_set_symbol(osc_data_t *buf, osc_data_t *end, const char *S);
osc_data_t * osc_set_char(osc_data_t *buf, osc_data_t *end, char c);
osc_data_t * osc_set_midi(osc_data_t *buf, osc_data_t *end, uint8_t *m);
osc_data_t * osc_set_midi_inline(osc_data_t *buf, osc_data_t *end, uint8_t **m);

osc_data_t * osc_start_bundle(osc_data_t *buf, osc_data_t *end, OSC_Timetag timetag, osc_data_t **bndl);
osc_data_t * osc_end_bundle(osc_data_t *buf, osc_data_t *end, osc_data_t *bndl);

// create item
osc_data_t * osc_start_bundle_item(osc_data_t *buf, osc_data_t *end, osc_data_t **itm);
osc_data_t * osc_end_bundle_item(osc_data_t *buf, osc_data_t *end, osc_data_t *itm);

osc_data_t *osc_set_vararg(osc_data_t *buf, osc_data_t *end, const char *path, const char *fmt, ...);
osc_data_t *osc_set_varlist(osc_data_t *buf, osc_data_t *end, const char *path, const char *fmt, va_list args);

#endif // _POSC_H_
