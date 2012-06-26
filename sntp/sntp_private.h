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

#ifndef _SNTP_PRIVATE_H_
#define _SNTP_PRIVATE_H_

#include <stdint.h>

#include <sntp.h>

typedef union _timestamp32s_t timestamp32s_t;
typedef union _timestamp32u_t timestamp32u_t;
typedef struct _sntp_t sntp_t;

union _timestamp32s_t {
	uint32_t all;
	struct {
		uint16_t sec;
		uint16_t frac;
	} part;
};

union _timestamp32u_t {
	uint32_t all;
	struct {
		int16_t sec;
		uint16_t frac;
	} part;
};

struct _sntp_t {
	uint8_t li_vn_mode;
	uint8_t stratum;
	int8_t poll;
	int8_t precision;

	timestamp32s_t root_delay;
	timestamp32u_t root_dispersion;
	char reference_identifier[4];

	timestamp64u_t reference_timestamp;
	timestamp64u_t originate_timestamp;
	timestamp64u_t receive_timestamp;
	timestamp64u_t transmit_timestamp;
};

#endif
