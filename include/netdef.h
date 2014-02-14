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

#ifndef _NETDEF_H_
#define _NETDEF_H_

#include <stdint.h>

/*
 * Endian stuff
 */

#define swap16(x) \
({ \
	uint16_t y; \
	asm volatile("\trev16	%[Y], %[X]\n" \
		: [Y]"=r"(y) \
		: [X]"r"(x) \
	); \
	(uint16_t)y; \
})
#define swap32		__builtin_bswap32
#define swap64		__builtin_bswap64

#define hton		swap16
#define htonl		swap32
#define htonll	swap64

#define ntoh		swap16
#define ntohl		swap32
#define ntohll	swap64

typedef union _Con {
	uint64_t all;
	struct {
		uint32_t upper;
		uint32_t lower;
	} part;
} Con;

#define ref_hton(dst,x)		(*((uint16_t *)(dst)) = hton(x))
#define ref_htonl(dst,x)	(*((uint32_t *)(dst)) = htonl(x))
//#define ref_htonll(dst,x)	(*((uint64_t *)(dst)) = htonll(x)) // has issues with GCC 4.8
#define ref_htonll(dst,x)	\
({ \
	Con a, *b; \
	a.all =(uint64_t)(x); \
	b =(Con *)(dst); \
	b->part.upper = htonl(a.part.lower); \
	b->part.lower = htonl(a.part.upper); \
})

#define ref_ntoh(ptr)		(ntoh(*((uint16_t *)(ptr))))
#define ref_ntohl(ptr)	(ntohl(*((uint32_t *)(ptr))))
//#define ref_ntohll(ptr)	(ntohll(*((uint64_t *)(ptr)))) // has issues with GCC 4.8
#define ref_ntohll(ptr)	\
({ \
	Con a, *b; \
	b =(Con *)(ptr); \
	a.part.upper = ntohl(b->part.lower); \
	a.part.lower = ntohl(b->part.upper); \
	(uint64_t)a.all; \
})

#endif
