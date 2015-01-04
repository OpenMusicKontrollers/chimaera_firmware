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

#define swap32(x) \
({ \
	uint32_t y; \
	asm volatile("\trev	%[Y], %[X]\n" \
		: [Y]"=r"(y) \
		: [X]"r"(x) \
	); \
	(uint32_t)y; \
})

#define swap64(x) (((uint64_t)swap32((uint32_t)(x)))<<32 | swap32((uint32_t)((x)>>32)))

#define hton		swap16
#define htonl		swap32
#define htonll	swap64

#define ntoh		swap16
#define ntohl		swap32
#define ntohll	swap64

#define ref_hton(dst,x)		(*((uint16_t *)(dst)) = hton(x))
#define ref_htonl(dst,x)	(*((uint32_t *)(dst)) = htonl(x))
#define ref_htonll(dst,x)	(*((uint64_t *)(dst)) = htonll(x))

#define ref_ntoh(ptr)		(ntoh(*((uint16_t *)(ptr))))
#define ref_ntohl(ptr)	(ntohl(*((uint32_t *)(ptr))))
#define ref_ntohll(ptr)	(ntohll(*((uint64_t *)(ptr))))

#endif // _NETDEF_H_
