/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _OSCQUERY_H_
#define _OSCQUERY_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <osc.h>

typedef struct _OSC_Query_Item OSC_Query_Item;
typedef struct _OSC_Query_Node OSC_Query_Node;
typedef struct _OSC_Query_Method OSC_Query_Method;
typedef struct _OSC_Query_Argument OSC_Query_Argument;

typedef enum _OSC_Query_Type {
	OSC_QUERY_NODE,
	OSC_QUERY_ARRAY,
	OSC_QUERY_METHOD,
} OSC_Query_Type;

typedef enum _OSC_Query_Mode {
	OSC_QUERY_MODE_R		= 0b001,
	OSC_QUERY_MODE_W		= 0b010,
	OSC_QUERY_MODE_RW	= 0b011,
} OSC_Query_Mode;

typedef union _OSC_Query_Value {
	int32_t i;
	float f;
	char *s;
} OSC_Query_Value;

struct _OSC_Query_Argument {
	OSC_Type type;
	OSC_Query_Mode mode;
	const char *description;
	struct {
		OSC_Query_Value min;
		OSC_Query_Value max;
		OSC_Query_Value step;
	} range;
	struct {
		uint_fast8_t argc;
		const OSC_Query_Value *ptr;
	} values;
};

struct _OSC_Query_Method {
	OSC_Method_Cb cb;
	uint_fast8_t argc;
	const OSC_Query_Argument *args;
};

struct _OSC_Query_Node {
	uint_fast8_t argc;
	const OSC_Query_Item *tree;
};

struct _OSC_Query_Item {
	const char *path;
	const char *description;
	OSC_Query_Type type;
	union {
		OSC_Query_Node node;
		OSC_Query_Method method;
	} item;
};

const OSC_Query_Item *osc_query_find(const OSC_Query_Item *item, const char *path, int_fast8_t argc);
void osc_query_response(char *buf, const OSC_Query_Item *item, const char *path);
uint_fast8_t osc_query_format(const OSC_Query_Item *item, const char *fmt);
uint_fast8_t osc_query_check(const OSC_Query_Item *item, const char *fmt, osc_data_t *buf);

#define OSC_QUERY_ITEM_NODE(PATH, DESCRIPTION, TREE) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = OSC_QUERY_NODE, \
	.item.node = { \
		.tree =(TREE), \
		.argc = sizeof((TREE)) / sizeof(OSC_Query_Item) \
	} \
}

#define OSC_QUERY_ITEM_ARRAY(PATH, DESCRIPTION, TREE, SIZE) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = OSC_QUERY_ARRAY, \
	.item.node = { \
		.tree =(TREE), \
		.argc = (SIZE) \
	} \
}

#define OSC_QUERY_ITEM_METHOD(PATH, DESCRIPTION, CB, ARGS) \
{ \
	.path =(PATH), \
	.description =(DESCRIPTION), \
	.type = OSC_QUERY_METHOD, \
	.item.method = { \
		.cb =(CB), \
		.args =(ARGS), \
		.argc = sizeof((ARGS)) / sizeof(OSC_Query_Argument) \
	} \
}

#define OSC_QUERY_ARGUMENT(TYPE, DESCRIPTION, MODE) \
	.type =(TYPE), \
	.description =(DESCRIPTION), \
	.mode =(MODE)

#define OSC_QUERY_ARGUMENT_BOOL(DESCRIPTION, MODE) \
{ \
	OSC_QUERY_ARGUMENT(OSC_INT32,(DESCRIPTION),(MODE)), \
	.range.min.i = 0, \
	.range.max.i = 1, \
	.range.step.i = 1 \
}

#define OSC_QUERY_ARGUMENT_INT32(DESCRIPTION, MODE, MIN, MAX, STEP) \
{ \
	OSC_QUERY_ARGUMENT(OSC_INT32,(DESCRIPTION),(MODE)), \
	.range.min.i =(MIN), \
	.range.max.i =(MAX), \
	.range.step.i =(STEP), \
}

#define OSC_QUERY_ARGUMENT_INT32_VALUES(DESCRIPTION, MODE, VALUES) \
{ \
	OSC_QUERY_ARGUMENT(OSC_INT32,(DESCRIPTION),(MODE)), \
	.values.ptr = (VALUES), \
	.values.argc = sizeof((VALUES)) / sizeof(OSC_Query_Value) \
}

#define OSC_QUERY_ARGUMENT_FLOAT(DESCRIPTION, MODE, MIN, MAX, STEP) \
{ \
	OSC_QUERY_ARGUMENT(OSC_FLOAT,(DESCRIPTION),(MODE)), \
	.range.min.f =(MIN), \
	.range.max.f =(MAX), \
	.range.step.f =(STEP) \
}

#define OSC_QUERY_ARGUMENT_STRING(DESCRIPTION, MODE, MAX) \
{ \
	OSC_QUERY_ARGUMENT(OSC_STRING,(DESCRIPTION),(MODE)), \
	.range.min.i = 0, \
	.range.max.i = (MAX), \
	.range.step.i = 1 \
}

#define OSC_QUERY_ARGUMENT_STRING_VALUES(DESCRIPTION, MODE, VALUES) \
{ \
	OSC_QUERY_ARGUMENT(OSC_STRING,(DESCRIPTION),(MODE)), \
	.values.ptr = (VALUES), \
	.values.argc = sizeof((VALUES)) / sizeof(OSC_Query_Value) \
}

#define OSC_QUERY_ARGUMENT_BLOB(DESCRIPTION, MODE) \
{ \
	OSC_QUERY_ARGUMENT(OSC_BLOB,(DESCRIPTION),(MODE)) \
}

#endif // _OSCQUERY_H_
