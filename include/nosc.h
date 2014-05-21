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

#ifndef _NOSC_H_
#define _NOSC_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <netdef.h>
#include <armfix.h>
#include <math.h>

/*
 * Definitions
 */

typedef fix_32_32_t nOSC_Timestamp;
typedef union _nOSC_Arg nOSC_Arg;
typedef nOSC_Arg *nOSC_Message;
typedef struct _nOSC_Bundle_Item nOSC_Bundle_Item;
typedef struct _nOSC_Message_Item nOSC_Message_Item;
typedef union _nOSC_Item nOSC_Item;
typedef nOSC_Item *nOSC_Bundle;
typedef struct _nOSC_Blob nOSC_Blob;
typedef struct _nOSC_Method nOSC_Method;

typedef uint_fast8_t(*nOSC_Method_Cb)(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args);
typedef void(*nOSC_Bundle_Start_Cb)(nOSC_Timestamp timestamp);
typedef void(*nOSC_Bundle_End_Cb)();

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

	nOSC_INT32_ZERO = 'j',
	nOSC_FLOAT_ZERO = 'g',
	nOSC_BLOB_INLINE = 'B',

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
	nOSC_Timestamp t;
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

struct _nOSC_Bundle_Item {
	nOSC_Bundle bndl;
	nOSC_Timestamp tt;
	char *fmt;
};

struct _nOSC_Message_Item {
	nOSC_Message msg;
	char *path;
	char *fmt;
};

union _nOSC_Item {
	nOSC_Bundle_Item bundle;
	nOSC_Message_Item message;
};

#define nosc_message(m,p,f)	(nOSC_Item){.message={.msg=m, .path=p, .fmt=f}}
#define nosc_bundle(b,t,f)	(nOSC_Item){.bundle={.bndl=b, .tt=t, .fmt=f}}

#define nosc_item_message_set(ITM,POS,MSG,PATH,FMT) \
({ \
	nOSC_Item *itm =(nOSC_Item *)(ITM); \
	uint_fast8_t pos =(uint_fast8_t)(POS); \
	itm[pos].message.msg =(nOSC_Message)(MSG); \
	itm[pos].message.path =(char *)PATH; \
	itm[pos].message.fmt =(char *)FMT; \
})

#define nosc_item_bundle_set(ITM,POS,BNDL,TIMESTAMP,FMT) \
({ \
	nOSC_Item *itm =(nOSC_Item *)(ITM); \
	uint_fast8_t pos =(uint_fast8_t)(POS); \
	itm[pos].bundle.bndl =(nOSC_Item *)BNDL; \
	itm[pos].bundle.tt =(nOSC_Timestamp)TIMESTAMP; \
	itm[pos].bundle.fmt =(char *)FMT; \
})

struct _nOSC_Method {
	char *path;
	char *fmt;
	nOSC_Method_Cb cb;
};

/*
 * Constants
 */

#define nOSC_IMMEDIATE	(1ULLK >> 32)
#define nOSC_Nil				INT32_MIN
#define nOSC_Infty			INT32_MAX

/*
 * Method functions
 */

void nosc_method_dispatch(nOSC_Method *meth, uint8_t *buf, uint16_t size, nOSC_Bundle_Start_Cb start, nOSC_Bundle_End_Cb end);

/*
 * Bundle functions
 */

uint16_t nosc_bundle_serialize(nOSC_Bundle bund, nOSC_Timestamp timestamp, char *fmt, uint8_t *buf, uint_fast8_t tcp);

/*
 * Message functions
 */

#define nosc_message_set_int32(MSG,POS,I)(((nOSC_Message)(MSG))[POS].i =(int32_t)(I))
#define nosc_message_set_float(MSG,POS,F)(((nOSC_Message)(MSG))[POS].f =(float)(F))
#define nosc_message_set_string(MSG,POS,S)(((nOSC_Message)(MSG))[POS].s =(char *)(S))
#define nosc_message_set_blob(MSG,POS,S,P) \
({ \
	((nOSC_Message)(MSG))[POS].b.size =(int32_t)(S); \
	((nOSC_Message)(MSG))[POS].b.data =(uint8_t *)(P); \
})

#define nosc_message_set_true(MSG,POS)(((nOSC_Message)(MSG))[POS].i = 0)
#define nosc_message_set_false(MSG,POS)(((nOSC_Message)(MSG))[POS].i = 1)
#define nosc_message_set_nil(MSG,POS)(((nOSC_Message)(MSG))[POS].i = nOSC_Nil)
#define nosc_message_set_infty(MSG,POS)(((nOSC_Message)(MSG))[POS].i = nOSC_Infty)

#define nosc_message_set_double(MSG,POS,D)(((nOSC_Message)(MSG))[POS].d =(double)(D))
#define nosc_message_set_int64(MSG,POS,H)(((nOSC_Message)(MSG))[POS].h =(int64_t)(H))
#define nosc_message_set_timestamp(MSG,POS,T)(((nOSC_Message)(MSG))[POS].t =(nOSC_Timestamp)(T))

#define nosc_message_set_midi(MSG,POS,M0,M1,M2,M3) \
({ \
	((nOSC_Message)(MSG))[POS].m[0] =(M0); \
	((nOSC_Message)(MSG))[POS].m[1] =(M1); \
	((nOSC_Message)(MSG))[POS].m[2] =(M2); \
	((nOSC_Message)(MSG))[POS].m[3] =(M3); \
})
#define nosc_message_set_symbol(MSG,POS,_S)(((nOSC_Message)(MSG))[POS].S =(char *)(_S))
#define nosc_message_set_char(MSG,POS,C)(((nOSC_Message)(MSG))[POS].c =(char)(C))

uint16_t nosc_message_serialize(nOSC_Message msg, const char *path, const char *fmt, uint8_t *buf, uint_fast8_t tcp);
uint16_t nosc_message_vararg_serialize(uint8_t *buf, uint_fast8_t tcp, const char *path, const char *fmt, ...);
uint16_t nosc_message_varlist_serialize(uint8_t *buf, uint_fast8_t tcp, const char *path, const char *fmt, va_list argv);

/*
 * Query system
 */
	
typedef struct _nOSC_Query_Item nOSC_Query_Item;
typedef struct _nOSC_Query_Node nOSC_Query_Node;
typedef struct _nOSC_Query_Method nOSC_Query_Method;
typedef struct _nOSC_Query_Argument nOSC_Query_Argument;

typedef enum _nOSC_Query_Type {
	nOSC_QUERY_NODE,
	nOSC_QUERY_ARRAY,
	nOSC_QUERY_METHOD,
} nOSC_Query_Type;

typedef enum _nOSC_Query_Mode {
	nOSC_QUERY_MODE_R		= 0b001,
	nOSC_QUERY_MODE_W		= 0b010,
	nOSC_QUERY_MODE_RW	= 0b011,
} nOSC_Query_Mode;

typedef union _nOSC_Query_Value {
	int32_t i;
	float f;
	char *s;
} nOSC_Query_Value;

struct _nOSC_Query_Argument {
	nOSC_Type type;
	nOSC_Query_Mode mode;
	const char *description;
	struct {
		nOSC_Query_Value min;
		nOSC_Query_Value max;
		nOSC_Query_Value step;
	} range;
	struct {
		uint_fast8_t argc;
		const nOSC_Query_Value *ptr;
	} values;
};

struct _nOSC_Query_Method {
	nOSC_Method_Cb cb;
	uint_fast8_t argc;
	const nOSC_Query_Argument *args;
};

struct _nOSC_Query_Node {
	uint_fast8_t argc;
	const nOSC_Query_Item *tree;
};

struct _nOSC_Query_Item {
	const char *path;
	const char *description;
	nOSC_Query_Type type;
	union {
		nOSC_Query_Node node;
		nOSC_Query_Method method;
	} item;
};

const nOSC_Query_Item *nosc_query_find(const nOSC_Query_Item *item, const char *path, int_fast8_t argc);
void nosc_query_response(uint8_t *buf, const nOSC_Query_Item *item, const char *path);
uint_fast8_t nosc_query_format(const nOSC_Query_Item *item, const char *fmt);
uint_fast8_t nosc_query_check(const nOSC_Query_Item *item, const char *fmt,  nOSC_Arg *args);

#define nOSC_QUERY_ITEM_NODE(PATH, DESCRIPTION, TREE) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = nOSC_QUERY_NODE, \
	.item.node.tree =(TREE), \
	.item.node.argc = sizeof((TREE)) / sizeof(nOSC_Query_Item) \
}

#define nOSC_QUERY_ITEM_ARRAY(PATH, DESCRIPTION, TREE, SIZE) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = nOSC_QUERY_ARRAY, \
	.item.node.tree =(TREE), \
	.item.node.argc = (SIZE) \
}

#define nOSC_QUERY_ITEM_METHOD(PATH, DESCRIPTION, CB, ARGS) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = nOSC_QUERY_METHOD, \
	.item.method.cb =(CB), \
	.item.method.args =(ARGS), \
	.item.method.argc = sizeof((ARGS)) / sizeof(nOSC_Query_Argument) \
}

#define nOSC_QUERY_ARGUMENT(TYPE, DESCRIPTION, MODE) \
	.type =(TYPE), \
	.description =(DESCRIPTION), \
	.mode =(MODE)

#define nOSC_QUERY_ARGUMENT_BOOL(DESCRIPTION, MODE) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_INT32,(DESCRIPTION),(MODE)), \
	.range.min.i = 0, \
	.range.max.i = 1, \
	.range.step.i = 1 \
}

#define nOSC_QUERY_ARGUMENT_INT32(DESCRIPTION, MODE, MIN, MAX, STEP) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_INT32,(DESCRIPTION),(MODE)), \
	.range.min.i =(MIN), \
	.range.max.i =(MAX), \
	.range.step.i =(STEP), \
}

#define nOSC_QUERY_ARGUMENT_INT32_VALUES(DESCRIPTION, MODE, VALUES) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_INT32,(DESCRIPTION),(MODE)), \
	.values.ptr = (VALUES), \
	.values.argc = sizeof((VALUES)) / sizeof(nOSC_Query_Value) \
}

#define nOSC_QUERY_ARGUMENT_FLOAT(DESCRIPTION, MODE, MIN, MAX, STEP) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_FLOAT,(DESCRIPTION),(MODE)), \
	.range.min.f =(MIN), \
	.range.max.f =(MAX), \
	.range.step.f =(STEP) \
}

#define nOSC_QUERY_ARGUMENT_STRING(DESCRIPTION, MODE, MAX) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_STRING,(DESCRIPTION),(MODE)), \
	.range.min.i = 0, \
	.range.max.i = (MAX), \
	.range.step.i = 1 \
}

#define nOSC_QUERY_ARGUMENT_STRING_VALUES(DESCRIPTION, MODE, VALUES) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_STRING,(DESCRIPTION),(MODE)), \
	.values.ptr = (VALUES), \
	.values.argc = sizeof((VALUES)) / sizeof(nOSC_Query_Value) \
}

#define nOSC_QUERY_ARGUMENT_BLOB(DESCRIPTION, MODE) \
{ \
	nOSC_QUERY_ARGUMENT(nOSC_BLOB,(DESCRIPTION),(MODE)) \
}

#endif // _NOSC_H_
