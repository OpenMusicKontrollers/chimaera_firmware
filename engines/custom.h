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

#ifndef _CUSTOM_H_
#define _CUSTOM_H_

#include <cmc.h>
#include <oscquery.h>

#define CUSTOM_MAX_EXPR		8
#define CUSTOM_MAX_INST		32

#define CUSTOM_PATH_LEN		64
#define CUSTOM_FMT_LEN		12
#define CUSTOM_ARGS_LEN		128

typedef struct _Custom_Item  Custom_Item;
typedef enum _RPN_Instruction RPN_Instruction;
typedef enum _RPN_Destination RPN_Destination;
typedef struct _RPN_VM RPN_VM;

enum _RPN_Instruction {
	RPN_TERMINATOR = 0,

	RPN_PUSH_VALUE,
	RPN_POP_INT32,
	RPN_POP_FLOAT,
	RPN_POP_MIDI,

	RPN_PUSH_FID,
	RPN_PUSH_SID,
	RPN_PUSH_GID,
	RPN_PUSH_PID,
	RPN_PUSH_X,
	RPN_PUSH_Z,
	RPN_PUSH_VX,
	RPN_PUSH_VZ,
	RPN_PUSH_N,

	RPN_PUSH_REG,
	RPN_POP_REG,

	RPN_ADD,
	RPN_SUB,
	RPN_MUL,
	RPN_DIV,
	RPN_MOD,
	RPN_POW,
	RPN_NEG,
	RPN_XCHANGE,
	RPN_DUPL_AT,
	RPN_DUPL_TOP,
	RPN_LSHIFT,
	RPN_RSHIFT,

	RPN_LOGICAL_AND,
	RPN_BITWISE_AND,
	RPN_LOGICAL_OR,
	RPN_BITWISE_OR,

	RPN_NOT,
	RPN_NOTEQ,
	RPN_COND,
	RPN_LT,
	RPN_LEQ,
	RPN_GT,
	RPN_GEQ,
	RPN_EQ
};

enum _RPN_Destination {
	RPN_NONE = 0,
	RPN_FRAME,
	RPN_ON,
	RPN_OFF,
	RPN_SET,
	RPN_END,
	RPN_IDLE
};

struct _RPN_VM {
	RPN_Instruction inst [CUSTOM_MAX_INST];
	float val [CUSTOM_MAX_INST];
};

struct _Custom_Item {
	RPN_Destination dest;
	char path [CUSTOM_PATH_LEN];
	char fmt [CUSTOM_FMT_LEN];
	RPN_VM vm;
};

extern CMC_Engine custom_engine;
extern const OSC_Query_Item custom_tree [3];

#endif // _CUSTOM_H_
