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

#define CUSTOM_PATH_LEN		16
#define CUSTOM_FMT_LEN		16
#define CUSTOM_ARGS_LEN		64

typedef struct _Custom_Item  Custom_Item;
typedef enum _RPN_Instruction RPN_Instruction;
typedef struct _RPN_VM RPN_VM;

enum _RPN_Instruction {
	RPN_TERMINATOR = 0,

	RPN_PUSH_VALUE,
	RPN_POP_INT32,
	RPN_POP_FLOAT,

	RPN_PUSH_FID,
	RPN_PUSH_SID,
	RPN_PUSH_GID,
	RPN_PUSH_PID,
	RPN_PUSH_X,
	RPN_PUSH_Z,

	RPN_ADD,
	RPN_SUB,
	RPN_MUL,
	RPN_DIV,
	RPN_MOD,
	RPN_POW,

	RPN_LT,
	RPN_LEQ,
	RPN_GT,
	RPN_GEQ,
	RPN_EQ
};

struct _RPN_VM {
	RPN_Instruction inst [32];
	float val [32];
};

struct _Custom_Item {
	char path [CUSTOM_PATH_LEN];
	char fmt [CUSTOM_FMT_LEN];
	RPN_VM vm;
};

extern nOSC_Bundle_Item custom_osc;
extern CMC_Engine custom_engine;
extern const nOSC_Query_Item custom_tree [6];

void custom_init ();

#endif // _CUSTOM_H_
