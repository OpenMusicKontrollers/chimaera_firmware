/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

#ifndef _ARMFIX_H_
#define _ARMFIX_H_

#include <stdfix.h>

#include <netdef.h>

typedef sat unsigned short fract			fix_0_8_t;		// size: 1
typedef sat unsigned fract						fix_0_16_t;		// size: 2
typedef sat unsigned long fract				fix_0_32_t;		// size: 4
typedef sat unsigned long long fract	fix_0_64_t;		// size: 8

typedef sat short fract								fix_s_7_t;		// size: 1
typedef sat fract											fix_s_15_t;		// size: 2
typedef sat long fract								fix_s_31_t;		// size: 4
typedef sat long long fract						fix_s_63_t;		// size: 8

typedef sat unsigned short accum			fix_8_8_t;		// size: 2
typedef sat unsigned accum						fix_16_16_t;	// size: 4
typedef sat unsigned long accum				fix_32_32_t;	// size: 8
//typedef sat unsigned long long accum	fix_32_32_t;	// size: 8

typedef sat short accum								fix_s7_8_t;		// size: 2
typedef sat accum											fix_s15_16_t;	// size: 4
typedef sat long accum								fix_s31_32_t;	// size: 8
//typedef sat long long accum						fix_s31_32_t; // size: 8

#endif
