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

#ifndef _NETDEF_H_
#define _NETDEF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union _timestamp64u_t timestamp64u_t;
typedef union _timestamp64s_t timestamp64s_t;

union _timestamp64u_t {
	uint64_t all;
	struct {
		uint32_t sec;
		uint32_t frac;
	} part;
};

union _timestamp64s_t {
	int64_t all;
	struct {
		int32_t sec;
		uint32_t frac;
	} part;
};

#define timestamp_to_int64(t) \
({ \
		timestamp64s_t __t = (t); \
		((int64_t)(__t).part.sec<<32) + (__t).part.frac; \
})

#define timestamp_to_uint64(t) \
({ \
		timestamp64u_t __t = (t); \
		((uint64_t)(__t).part.sec<<32) + (__t).part.frac; \
})

#define int64_to_timestamp(x) \
({ \
		int64_t __x = (x); \
    timestamp64s_t __t; \
		__t.part.sec = (__x)>>32; \
		__t.part.frac = (__x)&0xffffffff; \
		__t; \
})

#define uint64_to_timestamp(x) \
({ \
		uint64_t __x = (x); \
    timestamp64u_t __t; \
		__t.part.sec = (__x)>>32; \
		__t.part.frac = (__x)&0xffffffff; \
		__t; \
})

/*
 * Endian stuff
 */

#define hton(x) \
({ \
    uint16_t __x = (x); \
    ((uint16_t)( \
	(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
	(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})

#define htonl(x) \
({ \
    uint32_t __x = (x); \
    ((uint32_t)( \
	(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
	(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
	(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
	(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})

#define htonll(x) \
({ \
    uint64_t __x = (x); \
    ((uint64_t)( \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
	(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})

#ifdef __cplusplus
}
#endif

#endif
