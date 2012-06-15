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

#ifndef NOSC_PRIVATE_H
#define NOSC_PRIVATE_H

#include <nosc.h>

/*
 * Structs 
 */

struct _nOSC_Bundle {
	nOSC_Message *msg;
	char *path;
	char *fmt;
	nOSC_Bundle *prev, *next;
};

struct _nOSC_Message {
	nOSC_Arg arg;
	nOSC_Type type;
	nOSC_Message *prev, *next;
};

struct _nOSC_Server {
	char *path;
	char *fmt;
	nOSC_Method_Cb cb;
	void *data;
	nOSC_Server *next;
};

/*
 * Internal functions
 */

nOSC_Bundle *_nosc_bundle_deserialize (uint8_t *buf, uint16_t size);
nOSC_Message *_nosc_message_deserialize (uint8_t *buf, uint16_t size, char **path, char**fmt);

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


#endif // NOSC_PRIVATE_H
