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

static int32_t
pop_i(RPN_Stack *stack)
{
	int32_t v = stack->arr[0].i;

	int32_t i;
	for(i=0; i<RPN_STACK_HEIGHT-1; i++)
		stack->arr[i].i = stack->arr[i+1].i;
	stack->arr[RPN_STACK_HEIGHT-1].i = 0;

	return v;
}

static void
push_i(RPN_Stack *stack, int32_t v)
{
	int32_t i;
	for(i=RPN_STACK_HEIGHT-1; i>0; i--)
		stack->arr[i].i = stack->arr[i-1].i;
	stack->arr[0].i = v;
}

static float
pop_f(RPN_Stack *stack)
{
	float v = stack->arr[0].f;

	int32_t i;
	for(i=0; i<RPN_STACK_HEIGHT-1; i++)
		stack->arr[i].f = stack->arr[i+1].f;
	stack->arr[RPN_STACK_HEIGHT-1].f = 0.f;

	return v;
}

static void
push_f(RPN_Stack *stack, float v)
{
	int32_t i;
	for(i=RPN_STACK_HEIGHT-1; i>0; i--)
		stack->arr[i].f = stack->arr[i-1].f;
	stack->arr[0].f = v;
}

static int32_t
eval_i(char *str, size_t len, RPN_Stack *stack)
{
	char *ptr = str;
	char *end = str + len;

	while(ptr < end)
	{
		switch(*ptr)
		{
			case '$':
			{
				switch(ptr[1])
				{
					case 'f':
						push_i(stack, stack->fid);
						ptr++;
						break;
					case 'b':
						push_i(stack, stack->sid);
						ptr++;
						break;
					case 'g':
						push_i(stack, stack->gid);
						ptr++;
						break;
					case 'p':
						push_i(stack, stack->pid);
						ptr++;
						break;
					case 'x':
						push_i(stack, stack->x);
						ptr++;
						break;
					case 'z':
						push_i(stack, stack->z);
						ptr++;
						break;
					default:
						break;
				}
				ptr++;
				break;
			}

			case ' ':
			case '\t':
				ptr++;
				break;

			case '+':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c = a + b;
				push_i(stack, c);
				ptr++;
				break;
			}
			case '-':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c = a - b;
				push_i(stack, c);
				ptr++;
				break;
			}
			case '*':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c = a * b;
				push_i(stack, c);
				ptr++;
				break;
			}
			case '/':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c = a / b;
				push_i(stack, c);
				ptr++;
				break;
			}
			case '%':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c = a % b;
				push_i(stack, c);
				ptr++;
				break;
			}
			case '<':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c;
				switch(ptr[1])
				{
					case '=':
						c = a <= b;
						ptr++;
						break;
					default:
						c = a < b;
						break;
				}
				push_i(stack, c);
				ptr++;
				break;
			}
			case '>':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c;
				switch(ptr[1])
				{
					case '=':
						c = a >= b;
						ptr++;
						break;
					default:
						c = a > b;
						break;
				}
				push_i(stack, c);
				ptr++;
				break;
			}
			case '=':
			{
				int32_t b = pop_i(stack);
				int32_t a = pop_i(stack);
				int32_t c;
				switch(ptr[1])
				{
					case '=':
						ptr++;
						// fall-thru
					default:
						c = a == b;
						break;
				}
				push_i(stack, c);
				ptr++;
				break;
			}
			default:
			{
				int32_t c = strtol(ptr, &ptr, 10);
				push_i(stack, c);
			}
		}
	}

	return pop_i(stack);
}

static float
eval_f(char *str, size_t len, RPN_Stack *stack)
{
	char *ptr = str;
	char *end = str + len;

	while(ptr < end)
	{
		switch(*ptr)
		{
			case '$':
			{
				switch(ptr[1])
				{
					case 'f':
						push_f(stack, stack->fid);
						ptr++;
						break;
					case 'b':
						push_f(stack, stack->sid);
						ptr++;
						break;
					case 'g':
						push_f(stack, stack->gid);
						ptr++;
						break;
					case 'p':
						push_f(stack, stack->pid);
						ptr++;
						break;
					case 'x':
						push_f(stack, stack->x);
						ptr++;
						break;
					case 'z':
						push_f(stack, stack->z);
						ptr++;
						break;
					default:
						break;
				}
				ptr++;
				break;
			}

			case ' ':
			case '\t':
				ptr++;
				break;

			case '+':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = a + b;
				push_f(stack, c);
				ptr++;
				break;
			}
			case '-':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = a - b;
				push_f(stack, c);
				ptr++;
				break;
			}
			case '*':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = a * b;
				push_f(stack, c);
				ptr++;
				break;
			}
			case '/':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = a / b;
				push_f(stack, c);
				ptr++;
				break;
			}
			case '%':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = fmod(a, b);
				push_f(stack, c);
				ptr++;
				break;
			}
			case '^':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c = pow(a, b);
				push_f(stack, c);
				ptr++;
				break;
			}
			case '<':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c;
				switch(ptr[1])
				{
					case '=':
						c = a <= b;
						ptr++;
						break;
					default:
						c = a < b;
						break;
				}
				push_f(stack, c);
				ptr++;
				break;
			}
			case '>':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c;
				switch(ptr[1])
				{
					case '=':
						c = a >= b;
						ptr++;
						break;
					default:
						c = a > b;
						break;
				}
				push_f(stack, c);
				ptr++;
				break;
			}
			case '=':
			{
				float b = pop_f(stack);
				float a = pop_f(stack);
				float c;
				switch(ptr[1])
				{
					case '=':
						ptr++;
						// fall-thru
					default:
						c = a == b;
						break;
				}
				push_f(stack, c);
				ptr++;
				break;
			}
			default:
			{
				float c = strtod(ptr, &ptr);
				push_f(stack, c);
			}
		}
	}

	return pop_f(stack);
}

uint_fast8_t
rpn_eval(nOSC_Message msg, Custom_Item *itm, RPN_Stack *stack)
{
	char *ptr = itm->args;
	char *end = itm->args + strlen(itm->args);
	char *dst = itm->fmt;

	uint_fast8_t ret = 0;

	while(ptr < end)
	{
		switch(*ptr)
		{
			case ' ':
			case '\t':
			case '\n':
			 ptr++;
			 break;

			case 'i':
			{
				ptr++; // skip 'i'
				ptr++; // skip '('
				size_t size = strchr(ptr, ')') - ptr;
				int32_t i = eval_i(ptr, size, stack);
				ptr += size;
				ptr++; // skip ')'

				nosc_message_set_int32(msg, dst - itm->fmt, i);
				*dst++ = nOSC_INT32;

				ret = 1;
				break;
			}
			case 'f':
			{
				ptr++; // skip 'i'
				ptr++; // skip '('
				size_t size = strchr(ptr, ')') - ptr;
				float f = eval_f(ptr, size, stack);
				ptr += size;
				ptr++; // skip ')'
				
				nosc_message_set_float(msg, dst - itm->fmt, f);
				*dst++ = nOSC_FLOAT;

				ret = 1;
				break;
			}
			case 's':
			{
				// FIXME
				break;
			}
		}
	}
	
	*dst = '\0';

	return ret;
}
