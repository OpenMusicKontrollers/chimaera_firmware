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

typedef union _timestamp32u_t timestamp32u_t;
typedef union _timestamp32s_t timestamp32s_t;
typedef union _timestamp64u_t timestamp64u_t;
typedef union _timestamp64s_t timestamp64s_t;

union _timestamp32u_t {
	uint32_t all;
	struct {
		uint16_t sec;
		uint16_t frac;
	} part;
};

union _timestamp32s_t {
	int32_t all;
	struct {
		int16_t sec;
		uint16_t frac;
	} part;
};

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

/*
 * Endian stuff
 */

#define memcpy_hton(dst,x) \
({ \
	uint8_t *__dst = (dst); \
	__dst[0] = ((uint16_t)x & 0xff00U) >> 8; \
	__dst[1] = ((uint16_t)x & 0x00ffU); \
})

#define memcpy_htonl(dst,x) \
({ \
	uint8_t *__dst = (dst); \
	__dst[0] = ((uint32_t)x & 0xff000000UL) >> 24; \
	__dst[1] = ((uint32_t)x & 0x00ff0000UL) >> 16; \
	__dst[2] = ((uint32_t)x & 0x0000ff00UL) >> 8; \
	__dst[3] = ((uint32_t)x & 0x000000ffUL); \
})

#define memcpy_htonll(dst,x) \
({ \
	uint8_t *__dst = (dst); \
	__dst[0] = ((uint64_t)x & 0xff00000000000000ULL) >> 56; \
	__dst[1] = ((uint64_t)x & 0x00ff000000000000ULL) >> 48; \
	__dst[2] = ((uint64_t)x & 0x0000ff0000000000ULL) >> 40; \
	__dst[3] = ((uint64_t)x & 0x000000ff00000000ULL) >> 32; \
	__dst[4] = ((uint64_t)x & 0x00000000ff000000ULL) >> 24; \
	__dst[5] = ((uint64_t)x & 0x0000000000ff0000ULL) >> 16; \
	__dst[6] = ((uint64_t)x & 0x000000000000ff00ULL) >> 8; \
	__dst[7] = ((uint64_t)x & 0x00000000000000ffULL); \
})

#define memcpy_ntoh(dst) \
({ \
	uint8_t *__dst = (dst); \
	((uint16_t)( \
	((uint16_t)__dst[0] << 8) | \
	((uint16_t)__dst[1]) )); \
})

#define memcpy_ntohl(dst) \
({ \
	uint8_t *__dst = (dst); \
	((uint32_t)( \
	((uint32_t)__dst[0] << 24) | \
	((uint32_t)__dst[1] << 16) | \
	((uint32_t)__dst[2] << 8) | \
	((uint32_t)__dst[3]) )); \
})

#define memcpy_ntohll(dst) \
({ \
	uint8_t *__dst = (dst); \
	((uint64_t)( \
	((uint64_t)__dst[0] << 56) | \
	((uint64_t)__dst[1] << 48) | \
	((uint64_t)__dst[2] << 40) | \
	((uint64_t)__dst[3] << 32) | \
	((uint64_t)__dst[4] << 24) | \
	((uint64_t)__dst[5] << 16) | \
	((uint64_t)__dst[6] << 8) | \
	((uint64_t)__dst[7]) )); \
})

#define memcpy_ntotll(dst) \
({ \
	uint8_t *__dst = (dst); \
	((uint64_t)( \
	((uint64_t)__dst[4] << 24) | \
	((uint64_t)__dst[5] << 16) | \
	((uint64_t)__dst[6] << 8) | \
	((uint64_t)__dst[7]) | \
	((uint64_t)__dst[0] << 56) | \
	((uint64_t)__dst[1] << 48) | \
	((uint64_t)__dst[2] << 40) | \
	((uint64_t)__dst[3] << 32) )); \
})

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
