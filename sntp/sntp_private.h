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
