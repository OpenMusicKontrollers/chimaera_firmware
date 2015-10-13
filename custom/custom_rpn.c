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

#include <string.h>

#include "custom_private.h"

static inline __always_inline float
pop(RPN_Stack *stack)
{
	(stack->ptr)--; // underflow is checked for at compile time
	float v = *(stack->ptr);
	return v;
}

static inline __always_inline void
push(RPN_Stack *stack, float v)
{
	*(stack->ptr) = v;
	(stack->ptr)++; // overflow is checked for at compile time
}

static inline __always_inline void
xchange(RPN_Stack *stack)
{
	float b = pop(stack);
	float a = pop(stack);
	push(stack, b);
	push(stack, a);
}

static inline __always_inline void
duplicate(RPN_Stack *stack, int32_t pos)
{
	float v = *(stack->ptr - pos);
	push(stack, v);
}

osc_data_t *
rpn_run(osc_data_t *buf, osc_data_t *end, Custom_Item *itm, RPN_Stack *stack)
{
	osc_data_t *buf_ptr = buf;
	RPN_VM *vm = &itm->vm;

	stack->ptr = stack->arr; // reset stack

	RPN_Instruction *inst;
	for(inst = vm->inst; *inst != RPN_TERMINATOR; inst++)
		switch(*inst)
		{
			case RPN_PUSH_VALUE:
			{
				push(stack, vm->val[inst - vm->inst]);
				break;
			}
			case RPN_POP_INT32:
			{
				volatile int32_t i = pop(stack);
				buf_ptr = osc_set_int32(buf_ptr, end, i);
				break;
			}
			case RPN_POP_FLOAT:
			{
				float f = pop(stack);
				buf_ptr = osc_set_float(buf_ptr, end, f);
				break;
			}
			case RPN_POP_MIDI:
			{
				uint8_t *m;
				buf_ptr = osc_set_midi_inline(buf_ptr, end, &m);
				if(buf_ptr)
				{
					m[3] = pop(stack);
					m[2] = pop(stack);
					m[1] = pop(stack);
					m[0] = pop(stack);
				}
				break;
			}

			case RPN_PUSH_FID:
			{
				push(stack, stack->fid);
				break;
			}
			case RPN_PUSH_SID:
			{
				push(stack, stack->sid);
				break;
			}
			case RPN_PUSH_GID:
			{
				push(stack, stack->gid);
				break;
			}
			case RPN_PUSH_PID:
			{
				push(stack, stack->pid);
				break;
			}
			case RPN_PUSH_X:
			{
				push(stack, stack->x);
				break;
			}
			case RPN_PUSH_Z:
			{
				push(stack, stack->z);
				break;
			}
			case RPN_PUSH_VX:
			{
				push(stack, stack->vx);
				break;
			}
			case RPN_PUSH_VZ:
			{
				push(stack, stack->vz);
				break;
			}
			case RPN_PUSH_N:
			{
				push(stack, SENSOR_N);
				break;
			}

			case RPN_PUSH_REG:
			{
				int32_t pos = pop(stack);
				float c = pop(stack);
				if(pos < RPN_REG_HEIGHT)
					stack->reg[pos] = c;
				else
				{
					; //TODO warn
				}
				break;
			}
			case RPN_POP_REG:
			{
				int32_t pos = pop(stack);
				if(pos < RPN_REG_HEIGHT)
					push(stack, stack->reg[pos]);
				else // TODO warn
					push(stack, NAN);
				break;
			}

			// standard operators
			case RPN_ADD:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a + b;
				push(stack, c);
				break;
			}
			case RPN_SUB:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a - b;
				push(stack, c);
				break;
			}
			case RPN_MUL:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a * b;
				push(stack, c);
				break;
			}
			case RPN_DIV:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a / b;
				push(stack, c);
				break;
			}
			case RPN_MOD:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = fmod(a, b);
				push(stack, c);
				break;
			}
			case RPN_POW:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = pow(a, b);
				push(stack, c);
				break;
			}
			case RPN_NEG:
			{
				float c = pop(stack);
				push(stack, -c);
				break;
			}
			case RPN_XCHANGE:
			{
				xchange(stack);
				break;
			}
			case RPN_DUPL_AT:
			{
				int32_t pos = pop(stack);
				if(pos > RPN_STACK_HEIGHT)
					pos = RPN_STACK_HEIGHT;
				else if(pos < 1)
					pos = 1;
				duplicate(stack, pos);
				break;
			}
			case RPN_DUPL_TOP:
			{
				duplicate(stack, 1);
				break;
			}
			case RPN_LSHIFT:
			{
				int32_t b = pop(stack);
				int32_t a = pop(stack);
				int32_t c = a << b;
				push(stack, c);
				break;
			}
			case RPN_RSHIFT:
			{
				int32_t b = pop(stack);
				int32_t a = pop(stack);
				int32_t c = a >> b;
				push(stack, c);
				break;
			}
			case RPN_LOGICAL_AND:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a && b;
				push(stack, c);
				break;
			}
			case RPN_BITWISE_AND:
			{
				int32_t b = pop(stack);
				int32_t a = pop(stack);
				int32_t c = a & b;
				push(stack, c);
				break;
			}
			case RPN_LOGICAL_OR:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a || b;
				push(stack, c);
				break;
			}
			case RPN_BITWISE_OR:
			{
				int32_t b = pop(stack);
				int32_t a = pop(stack);
				int32_t c = a | b;
				push(stack, c);
				break;
			}

			// conditionals
			case RPN_NOT:
			{
				float c = pop(stack);
				push(stack, !c);
				break;
			}
			case RPN_NOTEQ:
			{
				float a = pop(stack);
				float b = pop(stack);
				float c = a != b;
				push(stack, c);
				break;
			}
			case RPN_COND:
			{
				float c = pop(stack);
				if(!c)
					xchange(stack);
				pop(stack);
				break;
			}
			case RPN_LT:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a < b;
				push(stack, c);
				break;
			}
			case RPN_LEQ:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a <= b;
				push(stack, c);
				break;
			}
			case RPN_GT:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a > b;
				push(stack, c);
				break;
			}
			case RPN_GEQ:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a >= b;
				push(stack, c);
				break;
			}
			case RPN_EQ:
			{
				float b = pop(stack);
				float a = pop(stack);
				float c = a == b;
				push(stack, c);
				break;
			}
			case RPN_TERMINATOR:
				// never reached
				break;
		}
	
	return buf_ptr;
}

static uint_fast8_t
rpn_add_inst(RPN_VM *vm, RPN_Compiler *compiler, RPN_Instruction inst, float val, uint_fast8_t pops, uint_fast8_t pushs)
{
	//check for stack underflow
	compiler->pp -= pops;
	if(compiler->pp < 0)
		return 0;

	// check for stack overflow
	compiler->pp += pushs;
	if(compiler->pp > RPN_STACK_HEIGHT)
		return 0;

	vm->inst[compiler->offset] = inst;
	vm->val[compiler->offset] = val;

	if(compiler->offset >= CUSTOM_MAX_INST)
		return 0;
	compiler->offset++;

	return 1;
}

static uint_fast8_t
rpn_compile_sub(const char *str, size_t len, RPN_VM *vm, RPN_Compiler *compiler)
{
	const char *ptr = str;
	const char *end = str + len;

	while(ptr < end)
	{
		switch(*ptr)
		{
			case '$':
			{
				switch(ptr[1])
				{
					case 'f':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_FID, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'b':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_SID, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'g':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_GID, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'p':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_PID, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'x':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_X, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'z':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_Z, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'X':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_VX, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'Z':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_VZ, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					case 'n':
						if(!rpn_add_inst(vm, compiler, RPN_PUSH_N, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					default:
						return 0; // parse error
				}
				ptr++;
				break;
			}

			case '[':
				if(!rpn_add_inst(vm, compiler, RPN_PUSH_REG, 0.f, 2, 0)) return 0;
				ptr++;
				break;
			case ']':
				if(!rpn_add_inst(vm, compiler, RPN_POP_REG, 0.f, 1, 1)) return 0;
				ptr++;
				break;

			case ' ':
			case '\t':
				// skip
				ptr++;
				break;

			case '+':
				if(!rpn_add_inst(vm, compiler, RPN_ADD, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '-':
				if(!rpn_add_inst(vm, compiler, RPN_SUB, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '*':
				if(!rpn_add_inst(vm, compiler, RPN_MUL, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '/':
				if(!rpn_add_inst(vm, compiler, RPN_DIV, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '%':
				if(!rpn_add_inst(vm, compiler, RPN_MOD, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '^':
				if(!rpn_add_inst(vm, compiler, RPN_POW, 0.f, 2, 1)) return 0;
				ptr++;
				break;
			case '~':
				if(!rpn_add_inst(vm, compiler, RPN_NEG, 0.f, 1, 1)) return 0;
				ptr++;
				break;
			case '#':
				if(!rpn_add_inst(vm, compiler, RPN_XCHANGE, 0.f, 2, 2)) return 0;
				ptr++;
				break;
			case '@':
				switch(ptr[1])
				{
					case '@':
						if(!rpn_add_inst(vm, compiler, RPN_DUPL_TOP, 0.f, 0, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_DUPL_AT, 0.f, 1, 1)) return 0;
						break;
				}
				ptr++;
				break;

			case '!':
				switch(ptr[1])
				{
					case '=':
						if(!rpn_add_inst(vm, compiler, RPN_NOTEQ, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_NOT, 0.f, 1, 1)) return 0;
						break;
				}
				ptr++;
				break;
			case '?':
				if(!rpn_add_inst(vm, compiler, RPN_COND, 0.f, 3, 1)) return 0;
				ptr++;
				break;
			case '<':
				switch(ptr[1])
				{
					case '=':
						if(!rpn_add_inst(vm, compiler, RPN_LEQ, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					case '<':
						if(!rpn_add_inst(vm, compiler, RPN_LSHIFT, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_LT, 0.f, 2, 1)) return 0;
						break;
				}
				ptr++;
				break;
			case '>':
				switch(ptr[1])
				{
					case '=':
						if(!rpn_add_inst(vm, compiler, RPN_GEQ, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					case '>':
						if(!rpn_add_inst(vm, compiler, RPN_RSHIFT, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_GT, 0.f, 2, 1)) return 0;
						break;
				}
				ptr++;
				break;
			case '=':
				switch(ptr[1])
				{
					case '=':
						if(!rpn_add_inst(vm, compiler, RPN_EQ, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						return 0; // parse error
				}
				ptr++;
				break;
			case '&':
				switch(ptr[1])
				{
					case '&':
						if(!rpn_add_inst(vm, compiler, RPN_LOGICAL_AND, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_BITWISE_AND, 0.f, 2, 1)) return 0;
						break;
				}
				ptr++;
				break;
			case '|':
				switch(ptr[1])
				{
					case '|':
						if(!rpn_add_inst(vm, compiler, RPN_LOGICAL_OR, 0.f, 2, 1)) return 0;
						ptr++;
						break;
					default:
						if(!rpn_add_inst(vm, compiler, RPN_BITWISE_OR, 0.f, 2, 1)) return 0;
						break;
				}
				ptr++;
				break;

			default:
			{
				char *endptr = NULL;
				float v = strtod(ptr, &endptr);
				if(ptr != endptr)
				{
					if(!rpn_add_inst(vm, compiler, RPN_PUSH_VALUE, v, 0, 1)) return 0;
					ptr = endptr;
				}
				else
					return 0; // parse error
			}
		}
	}

	return 1;
}

uint_fast8_t
rpn_compile(const char *args, Custom_Item *itm)
{
	RPN_VM *vm = &itm->vm;
	RPN_Compiler compiler = {
		.offset = 0,
		.pp = 0
	};

	const char *ptr = args;
	const char *end = args + strlen(args);
	uint_fast8_t counter = 0;

	// look for end of OSC path
	const char *path_end = strchr(ptr, ' ');
	if(!path_end)
		path_end = strchr(ptr, '\t');
	if(!path_end)
		path_end = strchr(ptr, '\0');
	if(!path_end)
		return 0; // parse error

	// copy and check OSC path
	size_t path_len = path_end + 1 - ptr;
	if(path_len > CUSTOM_PATH_LEN)
		return 0; // parse error
	strlcpy(itm->path, ptr, path_len);
	if(!osc_check_path(itm->path))
		return 0; // parse error

	// skip path
	ptr += path_len;

	while(ptr < end)
		switch(*ptr)
		{
			case ' ':
			case '\t':
				//skip white space
				ptr++;
				break;

			case OSC_INT32:
			{
				ptr++; // skip 'i'
				if(*ptr == '(')
				{
					ptr++; // skip '('
					char *closing = strchr(ptr, ')');
					if(closing)
					{
						size_t size = closing - ptr;
						if(rpn_compile_sub(ptr, size, vm, &compiler))
						{
							ptr += size;
							ptr++; // skip ')'

							if(!rpn_add_inst(vm, &compiler, RPN_POP_INT32, 0.f, 1, 0)) return 0;
							if(counter >= CUSTOM_FMT_LEN) return 0;
							itm->fmt[counter++] = OSC_INT32;
						}
						else
							return 0; // parse error
					}
					else
						return 0; // parse error
				}
				else
					return 0; // parse error
				break;
			}

			case OSC_FLOAT:
			{
				ptr++; // skip 'f'
				if(*ptr == '(')
				{
					ptr++; // skip '('
					char *closing = strchr(ptr, ')');
					if(closing)
					{
						size_t size = closing - ptr;
						if(rpn_compile_sub(ptr, size, vm, &compiler))
						{
							ptr += size;
							ptr++; // skip ')'

							if(!rpn_add_inst(vm, &compiler, RPN_POP_FLOAT, 0.f, 1, 0)) return 0;
							if(counter >= CUSTOM_FMT_LEN) return 0;
							itm->fmt[counter++] = OSC_FLOAT;
						}
						else
							return 0; // parse error
					}
					else
						return 0; // parse error
				}
				else
					return 0; // parse error
				break;
			}

			case OSC_MIDI:
			{
				ptr++; // skip 'm'
				if(*ptr == '(')
				{
					ptr++; // skip '('
					char *closing = strchr(ptr, ')');
					if(closing)
					{
						size_t size = closing - ptr;
						if(rpn_compile_sub(ptr, size, vm, &compiler))
						{
							ptr += size;
							ptr++; // skip ')'

							if(!rpn_add_inst(vm, &compiler, RPN_POP_MIDI, 0.f, 4, 0)) return 0;
							if(counter >= CUSTOM_FMT_LEN) return 0;
							itm->fmt[counter++] = OSC_MIDI;
						}
						else
							return 0; // parse error
					}
					else
						return 0; // parse error
				}
				else
					return 0; // parse error
				break;
			}

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				if(counter >= CUSTOM_FMT_LEN) return 0;
				itm->fmt[counter++] = *ptr;
				break;

			//TODO OSC_STRING

			default:
				return 0; // parse error
		}

	if(!rpn_add_inst(vm, &compiler, RPN_TERMINATOR, 0.f, 0, 0)) return 0;
	if(counter >= CUSTOM_FMT_LEN) return 0;
	itm->fmt[counter++] = '\0';

	return (ptr == end) && (compiler.pp >= 0);
}
