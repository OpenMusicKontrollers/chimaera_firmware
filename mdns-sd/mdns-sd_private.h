/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <mdns-sd.h>

typedef struct _DNS_Query DNS_Query;
typedef struct _DNS_Question DNS_Question;
typedef struct _DNS_Answer DNS_Answer;
typedef struct _DNS_Resolve DNS_Resolve;
typedef struct _DNS_PTR_Method DNS_PTR_Method;
typedef void (*DNS_PTR_Method_Cb)(DNS_Query *query);

#define MDNS_FLAGS_QR_BIT				15
#define MDNS_FLAGS_OPCODE_SHIFT	11
#define MDNS_FLAGS_AA_BIT				10
#define MDNS_FLAGS_TC_BIT				9
#define MDNS_FLAGS_RD_BIT				8
#define MDNS_FLAGS_RA_BIT				7
#define MDNS_FLAGS_Z_SHIFT			4
#define MDNS_FLAGS_RCODE_SHIFT	0

#define MDNS_FLAGS_QR						(0x1 << MDNS_FLAGS_QR_BIT)
#define MDNS_FLAGS_OPCODE 			(0xf << MDNS_FLAGS_OPCODE_SHIFT)
#define MDNS_FLAGS_AA						(0x1 << MDNS_FLAGS_AA_BIT)
#define MDNS_FLAGS_TC						(0x1 << MDNS_FLAGS_TC_BIT)
#define MDNS_FLAGS_RD						(0x1 << MDNS_FLAGS_RD_BIT)
#define MDNS_FLAGS_RA						(0x1 << MDNS_FLAGS_RA_BIT)
#define MDNS_FLAGS_Z						(0x7 << MDNS_FLAGS_Z_SHIFT)
#define MDNS_FLAGS_RCODE				(0xf << MDNS_FLAGS_RCODE_SHIFT)

struct _DNS_PTR_Method {
	char *name;
	DNS_PTR_Method_Cb cb;
};

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
} __attribute((packed,aligned(2)));

#define MDNS_TYPE_A						0x0001
#define MDNS_TYPE_AAAA				0x001c
#define MDNS_TYPE_PTR					0x000c
#define MDNS_TYPE_TXT					0x0010
#define MDNS_TYPE_SRV					0x0021

#define MDNS_CLASS_INET				0x0001
#define MDNS_CLASS_FLUSH			0x8000

#define MDNS_DEFAULT_TTL			0x00001194 // 1h 15m

struct _DNS_Question {
	uint16_t QTYPE;
	uint16_t QCLASS; // 1: internet
} __attribute((packed,aligned(2)));

struct _DNS_Answer {
	uint16_t RTYPE;
	uint16_t RCLASS;
	uint32_t TTL;
	uint16_t RLEN;
} __attribute((packed,aligned(2)));

struct _DNS_Resolve {
	char name [32];
	mDNS_Resolve_Cb cb;
	void *data;
};

#endif
