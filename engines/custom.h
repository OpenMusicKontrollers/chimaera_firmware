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

#ifndef _CUSTOM_H_
#define _CUSTOM_H_

#include <cmc.h>
#include <oscquery.h>

#define CUSTOM_MAX_EXPR		12
#define CUSTOM_MAX_INST		24

#define CUSTOM_PATH_LEN		24
#define CUSTOM_FMT_LEN		(CUSTOM_MAX_EXPR / 2)
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
	RPN_PUSH_N,

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
