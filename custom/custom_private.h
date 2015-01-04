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
