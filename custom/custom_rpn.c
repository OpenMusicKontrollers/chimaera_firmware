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

static inline __always_inline float
duplicate(RPN_Stack *stack, int32_t pos)
{
	float v = *(stack->ptr - pos);
	push(stack, v);
}

osc_data_t *
rpn_run(osc_data_t *buf, Custom_Item *itm, RPN_Stack *stack)
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
				buf_ptr = osc_set_int32(buf_ptr, i);
				break;
			}
			case RPN_POP_FLOAT:
			{
				float f = pop(stack);
				buf_ptr = osc_set_float(buf_ptr, f);
				break;
			}
			case RPN_POP_MIDI:
			{
				uint8_t *m;
				buf_ptr = osc_set_midi_inline(buf_ptr, &m);
				m[3] = pop(stack);
				m[2] = pop(stack);
				m[1] = pop(stack);
				m[0] = pop(stack);
				break;
			}
			case RPN_POP_BLOB:
			{
				uint8_t *b;
				volatile int32_t len = stack->ptr - stack->arr; // pop whole stack
				buf_ptr = osc_set_blob_inline(buf_ptr, len, (void **)&b);
				for(len--; len>=0; len--)
					b[len] = pop(stack);
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
			case RPN_PUSH_N:
			{
				push(stack, SENSOR_N);
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
		}
	
	return buf_ptr;
}

static uint_fast8_t
rpn_compile_sub(const char *str, size_t len, RPN_VM *vm, RPN_Compiler *compiler)
{
	const char *ptr = str;
	const char *end = str + len;

	RPN_Instruction *inst = &vm->inst[compiler->offset];

	while(ptr < end)
	{
		switch(*ptr)
		{
			case '$':
			{
				switch(ptr[1])
				{
					case 'f':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_FID;
						ptr++;
						break;
					case 'b':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_SID;
						ptr++;
						break;
					case 'g':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_GID;
						ptr++;
						break;
					case 'p':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_PID;
						ptr++;
						break;
					case 'x':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_X;
						ptr++;
						break;
					case 'z':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_Z;
						ptr++;
						break;
					case 'n':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_PUSH_N;
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
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_ADD;
				ptr++;
				break;
			case '-':
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_SUB;
				ptr++;
				break;
			case '*':
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_MUL;
				ptr++;
				break;
			case '/':
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_DIV;
				ptr++;
				break;
			case '%':
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_MOD;
				ptr++;
				break;
			case '^':
				if(compiler->pp - 2 < 0) return 0;
				compiler->pp--;
				*inst++ = RPN_POW;
				ptr++;
				break;
			case '~':
				if(compiler->pp - 1 < 0) return 0;
				*inst++ = RPN_NEG;
				ptr++;
				break;
			case '#':
				if(compiler->pp - 2 < 0) return 0;
				*inst++ = RPN_XCHANGE;
				ptr++;
				break;
			case '@':
				switch(ptr[1])
				{
					case '@':
						if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
						compiler->pp++;
						*inst++ = RPN_DUPL_TOP;
						ptr++;
						break;
					default:
						if(compiler->pp - 1 < 0) return 0;
						*inst++ = RPN_DUPL_AT;
						break;
				}
				ptr++;
				break;

			case '!':
				switch(ptr[1])
				{
					case '=':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_NOTEQ;
						ptr++;
						break;
					default:
						if(compiler->pp - 1 < 0) return 0;
						*inst++ = RPN_NOT;
						break;
				}
				ptr++;
				break;
			case '?':
				if(compiler->pp - 3 < 0) return 0;
				compiler->pp -= 2;
				*inst++ = RPN_COND;
				ptr++;
				break;
			case '<':
				switch(ptr[1])
				{
					case '=':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_LEQ;
						ptr++;
						break;
					case '<':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_LSHIFT;
						ptr++;
						break;
					default:
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_LT;
						break;
				}
				ptr++;
				break;
			case '>':
				switch(ptr[1])
				{
					case '=':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_GEQ;
						ptr++;
						break;
					case '>':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_RSHIFT;
						ptr++;
						break;
					default:
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_GT;
						break;
				}
				ptr++;
				break;
			case '=':
				switch(ptr[1])
				{
					case '=':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_EQ;
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
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_LOGICAL_AND;
						ptr++;
						break;
					default:
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_BITWISE_AND;
						break;
				}
				ptr++;
				break;
			case '|':
				switch(ptr[1])
				{
					case '|':
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_LOGICAL_OR;
						ptr++;
						break;
					default:
						if(compiler->pp - 2 < 0) return 0;
						compiler->pp--;
						*inst++ = RPN_BITWISE_OR;
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
					vm->val[inst - vm->inst] = v;
					*inst++ = RPN_PUSH_VALUE;
					if(compiler->pp + 1 > RPN_STACK_HEIGHT) return 0;
					compiler->pp++;
					ptr = endptr;
				}
				else
					return 0; // parse error
			}
		}
	}

	compiler->offset = inst - vm->inst;
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

							itm->fmt[counter++] = OSC_INT32;
							vm->inst[compiler.offset++] = RPN_POP_INT32;
							compiler.pp--;
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

							itm->fmt[counter++] = OSC_FLOAT;
							vm->inst[compiler.offset++] = RPN_POP_FLOAT;
							compiler.pp--;
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

							itm->fmt[counter++] = OSC_MIDI;
							vm->inst[compiler.offset++] = RPN_POP_MIDI;
							compiler.pp -= 4;
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

			case OSC_BLOB:
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

							itm->fmt[counter++] = OSC_BLOB;
							vm->inst[compiler.offset++] = RPN_POP_BLOB;
							compiler.pp -= compiler.pp;
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
				itm->fmt[counter++] = *ptr;
				break;

			//TODO OSC_STRING

			default:
				return 0; // parse error
		}

	itm->fmt[counter] = '\0'; //TODO check overflow CUTOM_MAX_INST
	vm->inst[compiler.offset] = RPN_TERMINATOR;

	return (ptr == end) && (compiler.pp >= 0);
}
