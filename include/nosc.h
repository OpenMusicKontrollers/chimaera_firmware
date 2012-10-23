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

#ifndef _NOSC_H_
#define _NOSC_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <netdef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definitions
 */

typedef struct _nOSC_Blob nOSC_Blob;
typedef struct _nOSC_Bundle nOSC_Bundle;
typedef struct _nOSC_Message nOSC_Message;
typedef struct _nOSC_Method nOSC_Method;
typedef union _nOSC_Arg nOSC_Arg;

struct _nOSC_Blob {
	int32_t len;
	uint8_t *dat;
};

typedef enum _nOSC_Type {
	nOSC_INT32 = 'i',
	nOSC_FLOAT = 'f',
	nOSC_STRING = 's',
	nOSC_BLOB = 'b',

	nOSC_TRUE = 'T',
	nOSC_FALSE = 'F',
	nOSC_NIL = 'N',
	nOSC_INFTY = 'I',

	nOSC_DOUBLE = 'd',
	nOSC_INT64 = 'h',
	nOSC_TIMESTAMP = 't',

	nOSC_MIDI = 'm'
} nOSC_Type;

union _nOSC_Arg {
	int32_t i;
	float f;
	char *s;
	nOSC_Blob b;

	double d;
	int64_t h;
	timestamp64u_t t;

	uint8_t m[4];
};

typedef uint8_t (*nOSC_Method_Cb) (const char *path, const char *fmt, uint8_t argc, nOSC_Arg **argb);

struct _nOSC_Method {
	char *path;
	char *fmt;
	nOSC_Method_Cb cb;
};

/*
 * Constants
 */

#define nOSC_IMMEDIATE ((timestamp64u_t){.part={.sec=0UL, .frac=1UL}})

/*
 * Method functions
 */

void nosc_method_dispatch (nOSC_Method *meth, uint8_t *buf, uint16_t size);

/*
 * Bundle functions
 */

nOSC_Bundle *nosc_bundle_add_message (nOSC_Bundle *bund, nOSC_Message *msg, const char *path);

uint16_t nosc_bundle_serialize (nOSC_Bundle *bund, timestamp64u_t timestamp, uint8_t *buf);

void nosc_bundle_free (nOSC_Bundle *bund);

/*
 * Message functions
 */

nOSC_Message *nosc_message_add_int32 (nOSC_Message *msg, int32_t i);
nOSC_Message *nosc_message_add_float (nOSC_Message *msg, float f);
nOSC_Message *nosc_message_add_string (nOSC_Message *msg, const char *s);
nOSC_Message *nosc_message_add_blob (nOSC_Message *msg, uint8_t *dat, int32_t len);

nOSC_Message *nosc_message_add_true (nOSC_Message *msg);
nOSC_Message *nosc_message_add_false (nOSC_Message *msg);
nOSC_Message *nosc_message_add_nil (nOSC_Message *msg);
nOSC_Message *nosc_message_add_infty (nOSC_Message *msg);

nOSC_Message *nosc_message_add_double (nOSC_Message *msg, double d);
nOSC_Message *nosc_message_add_int64 (nOSC_Message *msg, int64_t h);
nOSC_Message *nosc_message_add_timestamp (nOSC_Message *msg, timestamp64u_t t);

nOSC_Message *nosc_message_add_midi (nOSC_Message *msg, uint8_t m [4]);

uint16_t nosc_message_serialize (nOSC_Message *msg, const char *path, uint8_t *buf);
uint16_t nosc_message_vararg_serialize (uint8_t *buf, const char *path, const char *fmt, ...);

void nosc_message_free (nOSC_Message *msg);

#ifdef __cplusplus
}
#endif

#endif
