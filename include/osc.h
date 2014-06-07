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

/*
 * Definitions
 */

typedef uint8_t osc_data_t;
typedef fix_32_32_t OSC_Timetag;
typedef struct _OSC_Blob OSC_Blob;
typedef struct _OSC_Method OSC_Method;

typedef uint_fast8_t(*OSC_Method_Cb)(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *args);

typedef enum _osc_Type {
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
	OSC_MIDI = 'm',

	OSC_END = '\0'
} osc_Type;

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

// OSC object lengths
size_t osc_strlen(const char *buf);
size_t osc_fmtlen(const char *buf);
size_t osc_bloblen(osc_data_t *buf);
size_t osc_blobsize(osc_data_t *buf);

// get OSC arguments from raw buffer
osc_data_t *osc_get_path(osc_data_t *buf, const char **path);
osc_data_t *osc_get_fmt(osc_data_t *buf, const char **fmt);

osc_data_t *osc_get_int32(osc_data_t *buf, int32_t *i);
osc_data_t *osc_get_float(osc_data_t *buf, float *f);
osc_data_t *osc_get_string(osc_data_t *buf, const char **s);
osc_data_t *osc_get_blob(osc_data_t *buf, OSC_Blob *b);

osc_data_t *osc_get_int64(osc_data_t *buf, int64_t *h);
osc_data_t *osc_get_double(osc_data_t *buf, double *d);
osc_data_t *osc_get_timetag(osc_data_t *buf, uint64_t *t);

osc_data_t *osc_get_symbol(osc_data_t *buf, const char **S);
osc_data_t *osc_get_char(osc_data_t *buf, char *c);
osc_data_t *osc_get_midi(osc_data_t *buf, uint8_t **m);

// create preamble for TCP mode
osc_data_t *osc_start_preamble(osc_data_t *buf, osc_data_t **itm);
osc_data_t *osc_end_preamble(osc_data_t *buf, osc_data_t *itm);

// create bundle
osc_data_t *osc_start_bundle(osc_data_t *buf, OSC_Timetag timetag);

// create item
osc_data_t *osc_start_item_fixed(osc_data_t *buf, int32_t size);
osc_data_t *osc_start_item_variable(osc_data_t *buf, osc_data_t **itm);
osc_data_t *osc_end_item_variable(osc_data_t *buf, osc_data_t *itm);

// write OSC argument to raw buffer
osc_data_t *osc_set_path(osc_data_t *buf, const char *path);
osc_data_t *osc_set_fmt(osc_data_t *buf, const char *fmt);

osc_data_t *osc_set_int32(osc_data_t *buf, int32_t i);
osc_data_t *osc_set_float(osc_data_t *buf, float f);
osc_data_t *osc_set_string(osc_data_t *buf, const char *s);
osc_data_t *osc_set_blob(osc_data_t *buf, int32_t size, void *payload);
osc_data_t *osc_set_blob_inline(osc_data_t *buf, int32_t size, void **payload);

osc_data_t *osc_set_int64(osc_data_t *buf, int64_t h);
osc_data_t *osc_set_double(osc_data_t *buf, double d);
osc_data_t *osc_set_timetag(osc_data_t *buf, uint64_t t);

osc_data_t *osc_set_symbol(osc_data_t *buf, const char *S);
osc_data_t *osc_set_char(osc_data_t *buf, char c);
osc_data_t *osc_set_midi(osc_data_t *buf, uint8_t *m);
osc_data_t *osc_set_midi_inline(osc_data_t *buf, uint8_t **m);

osc_data_t *osc_vararg_set(osc_data_t *buf, const char *path, const char *fmt, ...);
osc_data_t *osc_varlist_set(osc_data_t *buf, const char *path, const char *fmt, va_list args);

#define quads(size) (((((size_t)size-1) & ~0x3) >> 2) + 1)
#define round_to_four_bytes(size) (quads((size_t)size) << 2)

#endif // _POSC_H_
