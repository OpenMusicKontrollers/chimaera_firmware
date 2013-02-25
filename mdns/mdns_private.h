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

#ifndef _MDNS_PRIVATE_H
#define _MDNS_PRIVATE_H

#include <mdns.h>

typedef struct _DNS_Query DNS_Query;
typedef struct _DNS_Question DNS_Question;
typedef struct _DNS_Answer DNS_Answer;

#define QR_BIT			15
#define OPCODE_BIT	11
#define AA_BIT			10
#define TC_BIT			9
#define RD_BIT			8
#define RA_BIT			7
#define Z_BIT				4
#define RCODE_BIT		0

#define QR			(1 << QR_BIT)
#define OPCODE 	(1 << OPCODE_BIT)
#define AA			(1 << AA_BIT)
#define TC			(1 << TC_BIT)
#define RD			(1 << RD_BIT)
#define RA			(1 << RA_BIT)
#define Z				(1 << Z_BIT)
#define RCODE		(1 << RCODE_BIT)

struct _DNS_Query {
	uint16_t ID;
	uint16_t FLAGS;
	/*
	uint8_t QR : 1; // 0: query, 1: response
	uint8_t Opcode : 4; // 0: standard query
	uint8_t AA : 1; // authoritative answer?
	uint8_t TC : 1; // truncated message?
	uint8_t RD : 1; // recursion desired
	uint8_t RA : 1; // recursion available
	uint8_t Z : 3; // reserved
	uint8_t RCODE : 4; // 0: no error, 1: format error, 2: server failure, 3: name error, 4: not implemented, 5: refused
	*/
	uint16_t QDCOUNT; // number of entries in question section
	uint16_t ANCOUNT; // number of entries in answer section
	uint16_t NSCOUNT; // number of name server resource records in autoritative records section
	uint16_t ARCOUNT; // number of resource records in additional records section
};

struct _DNS_Question {
	uint16_t QTYPE;
	uint16_t QCLASS; // 1: internet
};

struct _DNS_Answer {
	uint16_t RTYPE;
	uint16_t RCLASS;
	uint32_t TTL;
	uint16_t RLEN;
} __attribute((packed,aligned(2)));

#endif
