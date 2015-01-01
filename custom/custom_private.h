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

#ifndef _CUSTOM_PRIVATE_H_
#define _CUSTOM_PRIVATE_H_

#include <custom.h>

#define RPN_STACK_HEIGHT 16
#define RPN_REG_HEIGHT 8

typedef struct _RPN_Stack RPN_Stack;
typedef struct _RPN_Compiler RPN_Compiler;

struct _RPN_Stack {
	uint32_t fid;
	//OSC_Timetag now;
	//OSC_Timetag offset;
	//uint_fast8_t old_n;
	//uint_fast8_t new_n;
	uint32_t sid;
	uint16_t gid;
	uint16_t pid;
	float x;
	float z;
	float vx;
	float vz;

	float reg [RPN_REG_HEIGHT]; //FIXME use MAX_BLOB instead?
	float arr [RPN_STACK_HEIGHT];
	float *ptr;
};

struct _RPN_Compiler {
	uint_fast8_t offset;
	int_fast8_t pp;
};

osc_data_t *rpn_run(osc_data_t *buf, osc_data_t *end, Custom_Item *itm, RPN_Stack *stack);
uint_fast8_t rpn_compile(const char *args, Custom_Item *itm);

#endif // _CUSTOM_PRIVATE_H_
