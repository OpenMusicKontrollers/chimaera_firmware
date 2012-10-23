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

#ifdef __cplusplus
extern "C" {
#endif

typedef sat unsigned long long fract fix_0_64_t;
typedef sat fract long long fix_s_63_t;

typedef sat unsigned long fract fix_0_32_t;
typedef sat fract long fix_s_31_t;

typedef sat unsigned short fract fix_0_16_t;
typedef sat fract short fix_s_15_t;

typedef sat unsigned long long accum fix_32_32_t;
typedef sat accum long long fix_s31_32_t;

typedef sat unsigned long accum fix_16_16_t;
typedef sat accum long fix_s15_16_t;

typedef sat unsigned short accum fix_8_8_t;
typedef sat accum short fix_s7_8_t;

fix_32_32_t utime2fix (timestamp64u_t x);
fix_s31_32_t stime2fix (timestamp64s_t x);

timestamp64u_t ufix2time (fix_32_32_t x);
timestamp64s_t sfix2time (fix_s31_32_t x);

#ifdef __cplusplus
}
#endif

#endif
