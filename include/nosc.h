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

typedef union _nOSC_Union nOSC_Union;
typedef struct _nOSC_Arg nOSC_Arg;
typedef nOSC_Arg *nOSC_Message;
typedef struct _nOSC_Item nOSC_Item;
typedef nOSC_Item *nOSC_Bundle;
typedef struct _nOSC_Blob nOSC_Blob;
typedef struct _nOSC_Method nOSC_Method;

typedef uint8_t (*nOSC_Method_Cb) (const char *path, const char *fmt, uint8_t argc, nOSC_Arg *args);
typedef void (*nOSC_Bundle_Start_Cb) (uint64_t timestamp);
typedef void (*nOSC_Bundle_End_Cb) ();

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

	nOSC_SYMBOL = 'S',
	nOSC_CHAR = 'c',
	nOSC_MIDI = 'm',

	nOSC_END = '\0'
} nOSC_Type;

struct _nOSC_Blob {
	int32_t size;
	uint8_t *data;
};

union _nOSC_Union {
	// 8 bytes
	int64_t h;
	double d;
	uint64_t t;
	nOSC_Blob b;
	
	// 4 bytes
	int32_t i;
	float f;
	char *s;
	uint8_t m[4];
	char *S;

	// 1 byte
	char c;
};

struct _nOSC_Arg {
	nOSC_Type type;
	nOSC_Union val;
} __attribute__((packed,aligned(4))); // if not it gets aligned to sizeof(nOSC_Union)=8 by GCC, which is a silly waste of ram

#define nosc_int32(x)			(nOSC_Arg){.type=nOSC_INT32, .val={.i=(x)}}
#define nosc_float(x)			(nOSC_Arg){.type=nOSC_FLOAT, .val={.f=(x)}}
#define nosc_string(x)		(nOSC_Arg){.type=nOSC_STRING, .val={.s=(x)}}
#define nosc_blob(s,x)		(nOSC_Arg){.type=nOSC_BLOB, .val={.b={.size=(s), .data=(x)}}}

#define nosc_true					(nOSC_Arg){.type=nOSC_TRUE}
#define nosc_false				(nOSC_Arg){.type=nOSC_FALSE}
#define nosc_nil					(nOSC_Arg){.type=nOSC_NIL}
#define nosc_infty				(nOSC_Arg){.type=nOSC_INFTY}

#define nosc_double(x)		(nOSC_Arg){.type=nOSC_DOUBLE, .val={.d=(x)}}
#define nosc_int64(x)			(nOSC_Arg){.type=nOSC_INT64, .val={.h=(x)}}
#define nosc_timestamp(x)	(nOSC_Arg){.type=nOSC_TIMESTAMP, .val={.t=(x)}}

#define nosc_midi(x)			(nOSC_Arg){.type=nOSC_MIDI, .val={.m={(x)[0],(x)[1],(x)[2],(x)[3]}}}
#define nosc_symbol(x)		(nOSC_Arg){.type=nOSC_SYMBOL, .val={.S=(x)}}
#define nosc_char(x)			(nOSC_Arg){.type=nOSC_CHAR, .val={.c=(x)}}

#define nosc_end					(nOSC_Arg){.type=nOSC_END}

typedef enum _nOSC_Item_Type {
	nOSC_MESSAGE = 1,
	nOSC_BUNDLE = 2,
	nOSC_TERM = 0
} nOSC_Item_Type;

struct _nOSC_Item {
	nOSC_Item_Type type;
	union {
		struct {
			nOSC_Item *bndl;
			uint64_t tt;
		} bundle;
		struct {
			nOSC_Message msg;
			char *path;
		} message;
	} content;
} __attribute__((packed,aligned(4)));

#define nosc_message(m,p)	(nOSC_Item){.type=nOSC_MESSAGE, .content={.message={.msg=m, .path=p}}}
#define nosc_bundle(b,t)	(nOSC_Item){.type=nOSC_BUNDLE, .content={.bundle={.bndl=b, .tt=t}}}
#define nosc_term (nOSC_Item){.type=nOSC_TERM}

void nosc_item_message_set (nOSC_Item *itm, uint8_t pos, nOSC_Message msg, char *path);
void nosc_item_bundle_set (nOSC_Item *itm, uint8_t pos, nOSC_Item *bundle, uint64_t timestamp);
void nosc_item_term_set (nOSC_Item *itm, uint8_t pos);

struct _nOSC_Method {
	char *path;
	char *fmt;
	nOSC_Method_Cb cb;
};

/*
 * Constants
 */

#define nOSC_IMMEDIATE 1ULL

/*
 * Method functions
 */

void nosc_method_dispatch (nOSC_Method *meth, uint8_t *buf, uint16_t size, nOSC_Bundle_Start_Cb start, nOSC_Bundle_End_Cb end);

/*
 * Bundle functions
 */

uint16_t nosc_bundle_serialize (nOSC_Bundle bund, uint64_t timestamp, uint8_t *buf);

/*
 * Message functions
 */

void nosc_message_set_int32 (nOSC_Message msg, uint8_t pos, int32_t i);
void nosc_message_set_float (nOSC_Message msg, uint8_t pos, float f);
void nosc_message_set_string (nOSC_Message msg, uint8_t pos, char *s);
void nosc_message_set_blob (nOSC_Message msg, uint8_t pos, int32_t size, uint8_t *data);

void nosc_message_set_true (nOSC_Message msg, uint8_t pos);
void nosc_message_set_false (nOSC_Message msg, uint8_t pos);
void nosc_message_set_nil (nOSC_Message msg, uint8_t pos);
void nosc_message_set_infty (nOSC_Message msg, uint8_t pos);

void nosc_message_set_double (nOSC_Message msg, uint8_t pos, double d);
void nosc_message_set_int64 (nOSC_Message msg, uint8_t pos, int64_t h);
void nosc_message_set_timestamp (nOSC_Message msg, uint8_t pos, uint64_t t);

void nosc_message_set_midi (nOSC_Message msg, uint8_t pos, uint8_t *m);
void nosc_message_set_symbol (nOSC_Message msg, uint8_t pos, char *S);
void nosc_message_set_char (nOSC_Message msg, uint8_t pos, char c);

void nosc_message_set_end (nOSC_Message msg, uint8_t pos);

uint16_t nosc_message_serialize (nOSC_Message msg, const char *path, uint8_t *buf);
uint16_t nosc_message_vararg_serialize (uint8_t *buf, const char *path, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
