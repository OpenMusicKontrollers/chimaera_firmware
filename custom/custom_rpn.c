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

#include <string.h>

#include "custom_private.h"

static float
pop(RPN_Stack *stack)
{
	float v = stack->arr[0];

	int32_t i;
	for(i=0; i<RPN_STACK_HEIGHT-1; i++)
		stack->arr[i] = stack->arr[i+1];
	stack->arr[RPN_STACK_HEIGHT-1] = 0.f;

	return v;
}

static void
push(RPN_Stack *stack, float v)
{
	int32_t i;
	for(i=RPN_STACK_HEIGHT-1; i>0; i--)
		stack->arr[i] = stack->arr[i-1];
	stack->arr[0] = v;
}

static void
xchange(RPN_Stack *stack)
{
	float v = stack->arr[0];
	stack->arr[0] = stack->arr[1];
	stack->arr[1] = v;
}

osc_data_t *
rpn_run(osc_data_t *buf, Custom_Item *itm, RPN_Stack *stack)
{
	osc_data_t *buf_ptr = buf;
	RPN_VM *vm = &itm->vm;

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
				int32_t i = pop(stack);
				buf_ptr = osc_set_int32(buf_ptr, i);
				break;
			}
			case RPN_POP_FLOAT:
			{
				float f = pop(stack);
				buf_ptr = osc_set_float(buf_ptr, f);
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
		}
	
	return buf_ptr;
}

static uint_fast8_t
rpn_compile_sub(const char *str, size_t len, RPN_VM *vm, uint_fast8_t offset)
{
	const char *ptr = str;
	const char *end = str + len;

	RPN_Instruction *inst = &vm->inst[offset];

	while(ptr < end)
	{
		switch(*ptr)
		{
			case '$':
			{
				switch(ptr[1])
				{
					case 'f':
						*inst++ = RPN_PUSH_FID;
						ptr++;
						break;
					case 'b':
						*inst++ = RPN_PUSH_SID;
						ptr++;
						break;
					case 'g':
						*inst++ = RPN_PUSH_GID;
						ptr++;
						break;
					case 'p':
						*inst++ = RPN_PUSH_PID;
						ptr++;
						break;
					case 'x':
						*inst++ = RPN_PUSH_X;
						ptr++;
						break;
					case 'z':
						*inst++ = RPN_PUSH_Z;
						ptr++;
						break;
					default:
						return 0; // parse error
				}
				ptr++;
				break;
			}

			case ' ':
			case '\t':
				// skip
				ptr++;
				break;

			case '+':
				*inst++ = RPN_ADD;
				ptr++;
				break;
			case '-':
				*inst++ = RPN_SUB;
				ptr++;
				break;
			case '*':
				*inst++ = RPN_MUL;
				ptr++;
				break;
			case '/':
				*inst++ = RPN_DIV;
				ptr++;
				break;
			case '%':
				*inst++ = RPN_MOD;
				ptr++;
				break;
			case '^':
				*inst++ = RPN_POW;
				ptr++;
				break;
			case '~':
				*inst++ = RPN_NEG;
				ptr++;
				break;
			case '#':
				*inst++ = RPN_XCHANGE;
				ptr++;
				break;

			case '!':
				switch(ptr[1])
				{
					case '=':
						*inst++ = RPN_NOTEQ;
						ptr++;
						break;
					default:
						*inst++ = RPN_NOT;
						break;
				}
				ptr++;
				break;
			case '?':
				*inst++ = RPN_COND;
				ptr++;
				break;
			case '<':
				switch(ptr[1])
				{
					case '=':
						*inst++ = RPN_LEQ;
						ptr++;
						break;
					default:
						*inst++ = RPN_LT;
						break;
				}
				ptr++;
				break;
			case '>':
				switch(ptr[1])
				{
					case '=':
						*inst++ = RPN_GEQ;
						ptr++;
						break;
					default:
						*inst++ = RPN_GT;
						break;
				}
				ptr++;
				break;
			case '=':
				switch(ptr[1])
				{
					case '=':
						*inst++ = RPN_EQ;
						ptr++;
						break;
					default:
						return 0; // parse error
				}
				ptr++;
				break;

			default:
			{
				char *endptr = NULL;
				float v = strtod(ptr, &endptr);
				if(ptr != endptr)
				{
					vm->val[inst - vm->inst] = v;
					*inst++ = RPN_PUSH_VALUE;
					ptr = endptr;
				}
				else
					return 0; // parse error
			}
		}
	}

	return inst - vm->inst; // new offset
}

uint_fast8_t
rpn_compile(const char *args, Custom_Item *itm)
{
	RPN_VM *vm = &itm->vm;

	const char *ptr = args;
	const char *end = args + strlen(args);
	uint_fast8_t offset = 0;
	uint_fast8_t counter = 0;

	while(ptr < end)
		switch(*ptr)
		{
			case ' ':
			case '\t':
				//skip white space
				ptr++;
				break;

			case 'i':
			{
				ptr++; // skip 'i'
				if(*ptr == '(')
				{
					ptr++; // skip '('
					char *closing = strchr(ptr, ')');
					if(closing)
					{
						size_t size = closing - ptr;
						offset = rpn_compile_sub(ptr, size, vm, offset);
						if(offset)
						{
							ptr += size;
							ptr++; // skip ')'

							itm->fmt[counter++] = OSC_INT32;
							vm->inst[offset++] = RPN_POP_INT32;
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

			case 'f':
			{
				ptr++; // skip 'f'
				if(*ptr == '(')
				{
					ptr++; // skip '('
					char *closing = strchr(ptr, ')');
					if(closing)
					{
						size_t size = closing - ptr;
						offset = rpn_compile_sub(ptr, size, vm, offset);
						if(offset)
						{
							ptr += size;
							ptr++; // skip ')'

							itm->fmt[counter++] = OSC_FLOAT;
							vm->inst[offset++] = RPN_POP_FLOAT;
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

			default:
				return 0; // parse error
		}

	itm->fmt[counter] = '\0'; //TODO check overflow
	vm->inst[offset] = RPN_TERMINATOR;

	return ptr == end;
}
