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

#ifndef _ARMFIX_H_
#define _ARMFIX_H_

#include <stdfix.h>

#include <netdef.h>

// unsaturated fixed point types
typedef unsigned short fract					fix_0_8_t;		// size: 1
typedef unsigned fract								fix_0_16_t;		// size: 2
typedef unsigned long fract						fix_0_32_t;		// size: 4
typedef unsigned long long fract			fix_0_64_t;		// size: 8

typedef short fract										fix_s_7_t;		// size: 1
typedef fract													fix_s_15_t;		// size: 2
typedef long fract										fix_s_31_t;		// size: 4
typedef long long fract								fix_s_63_t;		// size: 8

typedef unsigned short accum					fix_8_8_t;		// size: 2
typedef unsigned accum								fix_16_16_t;	// size: 4
typedef unsigned long accum						fix_32_32_t;	// size: 8

typedef short accum										fix_s7_8_t;		// size: 2
typedef accum													fix_s15_16_t;	// size: 4
typedef long accum										fix_s31_32_t;	// size: 8

// saturated fixed point types
typedef sat unsigned short fract			sat_fix_0_8_t;		// size: 1
typedef sat unsigned fract						sat_fix_0_16_t;		// size: 2
typedef sat unsigned long fract				sat_fix_0_32_t;		// size: 4
typedef sat unsigned long long fract	sat_fix_0_64_t;		// size: 8

typedef sat short fract								sat_fix_s_7_t;		// size: 1
typedef sat fract											sat_fix_s_15_t;		// size: 2
typedef sat long fract								sat_fix_s_31_t;		// size: 4
typedef sat long long fract						sat_fix_s_63_t;		// size: 8

typedef sat unsigned short accum			sat_fix_8_8_t;		// size: 2
typedef sat unsigned accum						sat_fix_16_16_t;	// size: 4
typedef sat unsigned long accum				sat_fix_32_32_t;	// size: 8

typedef sat short accum								sat_fix_s7_8_t;		// size: 2
typedef sat accum											sat_fix_s15_16_t;	// size: 4
typedef sat long accum								sat_fix_s31_32_t;	// size: 8

#endif // _ARMFIX_H_
