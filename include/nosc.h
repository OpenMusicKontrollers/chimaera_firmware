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

/*
 * Definitions
 */

typedef union _nOSC_Arg nOSC_Arg;
typedef nOSC_Arg *nOSC_Message;
typedef union _nOSC_Item nOSC_Item;
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

union _nOSC_Arg {
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

typedef enum _nOSC_Item_Type {
	nOSC_MESSAGE = 'M',
	nOSC_BUNDLE = 'B',
	nOSC_TERM = '\0'
} nOSC_Item_Type;

union _nOSC_Item {
	struct {
		nOSC_Bundle bndl;
		uint64_t tt;
		char *fmt;
	} bundle;

	struct {
		nOSC_Message msg;
		char *path;
		char *fmt;
	} message;
} __attribute__((packed,aligned(4)));

#define nosc_message(m,p,f)	(nOSC_Item){.message={.msg=m, .path=p, .fmt=f}}
#define nosc_bundle(b,t,f)	(nOSC_Item){.bundle={.bndl=b, .tt=t, .fmt=f}}

#define nosc_item_message_set(ITM,POS,MSG,PATH,FMT) \
({ \
	nOSC_Item *itm = (nOSC_Item *)(ITM); \
	uint8_t pos = (uint8_t)(POS); \
	itm[pos].message.msg = (nOSC_Message)(MSG); \
	itm[pos].message.path = (char *)PATH; \
	itm[pos].message.fmt = (char *)FMT; \
})

#define nosc_item_bundle_set(ITM,POS,BNDL,TIMESTAMP,FMT) \
({ \
	nOSC_Item *itm = (nOSC_Item *)(ITM); \
	uint8_t pos = (uint8_t)(POS); \
	itm[pos].bundle.bndl = (nOSC_Item *)BNDL; \
	itm[pos].bundle.tt = (uint64_t)TIMESTAMP; \
	itm[pos].bundle.fmt = (char *)FMT; \
})

struct _nOSC_Method {
	char *path;
	char *fmt;
	nOSC_Method_Cb cb;
};

/*
 * Constants
 */

#define nOSC_IMMEDIATE	1ULL
#define nOSC_Nil				INT32_MIN
#define nOSC_Infty			INT32_MAX

/*
 * Method functions
 */

void nosc_method_dispatch (nOSC_Method *meth, uint8_t *buf, uint16_t size, nOSC_Bundle_Start_Cb start, nOSC_Bundle_End_Cb end);

/*
 * Bundle functions
 */

uint16_t nosc_bundle_serialize (nOSC_Bundle bund, uint64_t timestamp, char *fmt, uint8_t *buf);

/*
 * Message functions
 */

#define nosc_message_set_int32(MSG,POS,I) (((nOSC_Message)(MSG))[POS].i = (int32_t)(I))
#define nosc_message_set_float(MSG,POS,F) (((nOSC_Message)(MSG))[POS].f = (float)(F))
#define nosc_message_set_string(MSG,POS,S) (((nOSC_Message)(MSG))[POS].s = (char *)(S))
#define nosc_message_set_blob(MSG,POS,S,P) \
({ \
	((nOSC_Message)(MSG))[POS].b.size = (int32_t)(S); \
	((nOSC_Message)(MSG))[POS].b.data = (uint8_t *)(P); \
})

#define nosc_message_set_true(MSG,POS) (((nOSC_Message)(MSG))[POS].i = 0)
#define nosc_message_set_false(MSG,POS) (((nOSC_Message)(MSG))[POS].i = 1)
#define nosc_message_set_nil(MSG,POS) (((nOSC_Message)(MSG))[POS].i = nOSC_Nil)
#define nosc_message_set_infty(MSG,POS) (((nOSC_Message)(MSG))[POS].i = nOSC_Infty)

#define nosc_message_set_double(MSG,POS,D) (((nOSC_Message)(MSG))[POS].d = (double)(D))
#define nosc_message_set_int64(MSG,POS,H) (((nOSC_Message)(MSG))[POS].h = (int64_t)(H))
#define nosc_message_set_timestamp(MSG,POS,T) (((nOSC_Message)(MSG))[POS].t = (uint64_t)(T))

#define nosc_message_set_midi(MSG,POS,M) (memcpy (((nOSC_Message)(MSG))[(POS)].m, (uint8_t *)(M), 4))
#define nosc_message_set_symbol(MSG,POS,_S) (((nOSC_Message)(MSG))[POS].S = (char *)(_S))
#define nosc_message_set_char(MSG,POS,C) (((nOSC_Message)(MSG))[POS].c = (char)(C))

uint16_t nosc_message_serialize (nOSC_Message msg, const char *path, const char *fmt, uint8_t *buf);
uint16_t nosc_message_vararg_serialize (uint8_t *buf, const char *path, const char *fmt, ...);

#endif
