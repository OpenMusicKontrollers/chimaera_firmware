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

#ifndef _SNTP_PRIVATE_H_
#define _SNTP_PRIVATE_H_

#include <stdint.h>

#include <sntp.h>
#include <armfix.h>

typedef union _timestamp64_t timestamp64_t;
typedef struct _sntp_t sntp_t;

union _timestamp64_t {
	fix_32_32_t fix;
	struct {
		uint32_t frac;
		uint32_t sec;
	} osc;
	struct {
		uint32_t sec;
		uint32_t frac;
	} ntp;
};

#define SNTP_LEAP_INDICATOR_NO_WARNING	(0 << 6)
#define SNTP_VERSION_4									(4 << 3)
#define SNTP_MODE_CLIENT								(3 << 0)
#define SNTP_MODE_SERVER								(4 << 0)

struct _sntp_t {
	uint8_t li_vn_mode;
	uint8_t stratum;
	int8_t poll;
	int8_t precision;

	int32_t root_delay;
	uint32_t root_dispersion;
	char reference_identifier[4];

	timestamp64_t reference_timestamp;
	timestamp64_t originate_timestamp;
	timestamp64_t receive_timestamp;
	timestamp64_t transmit_timestamp;
};

#endif // _SNTP_PRIVATE_H_
