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

#include "mdns-sd_private.h"

#include <chimaera.h>
#include <chimutil.h>
#include <wiz.h>
#include <config.h>
#include <sntp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libmaple/systick.h>

static const char *localdomain = "local";
static const char *in_addr = "in-addr";
static const char *arpa = "arpa";

// running mDNS resolve request
static DNS_Resolve resolve;

// dns-sd PTR methods prototype definitions
static void _hook_services(DNS_Query *);
static void _hook_osc(DNS_Query *);
static void _hook_chimaera(DNS_Query *);
static void _hook_arpa(DNS_Query *);

enum {HOOK_SERVICES=0, HOOK_OSC=1, HOOK_CHIMAERA=2, HOOK_ARPA=3};

// self query name for DNS lookup
static int len_self;
static char hook_self [32];

// self query name for DNS revers-lookup
static int len_arpa;
static char hook_arpa [32];

// dns-sd PTR methods array
static DNS_PTR_Method hooks [] = {
	[HOOK_SERVICES]	= {"\011_services\007_dns-sd\004_udp\005local", _hook_services},
	[HOOK_OSC]			= {"\004_osc\004_udp\005local", _hook_osc},
	[HOOK_CHIMAERA]	= {"\010chimaera\004_osc\004_udp\005local", _hook_chimaera},
	[HOOK_ARPA]			= {hook_arpa, _hook_arpa},
	{NULL, NULL}
};

// unrolled query name label
static uint8_t qname_unrolled [32]; //TODO big enough?

// unroll query name
static uint8_t *
_unroll_qname(DNS_Query *query, uint8_t *buf, char **qname)
{
	uint8_t *buf_ptr;
	uint8_t *ref = qname_unrolled;

	*qname = ref;

	for(buf_ptr=buf; *buf_ptr; )
		if( (buf_ptr[0] & 0xc0) == 0xc0) // it's a pointer at the beginning/end of the label
		{
			uint16_t offset =((buf_ptr[0] & 0x3f) << 8) | (buf_ptr[1] & 0xff); // get offset of label
			uint8_t *ptr =(uint8_t *)query + offset; // label
			memcpy(ref, ptr, strlen(ptr) + 1); // trailing zero
			buf_ptr += 2; // skip pointer

			return buf_ptr; // that's the end of the label
		}
		else // copy character of label
		{
			*ref = *buf_ptr;
			ref++;
			buf_ptr++;
		}

	*ref = 0x0; // write trailing zero
	buf_ptr++; // skip trailing zero

	return buf_ptr;
}

static uint8_t *
_serialize_query(uint8_t *buf, uint16_t id, uint16_t flags, uint16_t qdcount, uint16_t ancount, uint16_t nscount, uint16_t arcount)
{
	uint8_t *buf_ptr = buf;

	DNS_Query q;
	q.ID = hton(id);
	q.FLAGS = hton(flags);
	q.QDCOUNT = hton(qdcount); // number of questions
	q.ANCOUNT = hton(ancount); // number of answers
	q.NSCOUNT = hton(nscount); // number of authoritative records
	q.ARCOUNT = hton(arcount); // number of resource records

	memcpy(buf_ptr, &q, sizeof(DNS_Query));
	buf_ptr += sizeof(DNS_Query);

	return buf_ptr;
}

static uint8_t *
_serialize_answer(uint8_t *buf, char *qname, uint16_t rtype, uint16_t rclass, uint32_t ttl, uint16_t rlen)
{
	uint8_t *buf_ptr = buf;

	uint16_t len = strlen(qname) + 1;
	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	DNS_Answer a;
	a.RTYPE = hton(rtype);
	a.RCLASS = hton(rclass);
	a.TTL = htonl(ttl);
	a.RLEN = hton(rlen);

	memcpy(buf_ptr, &a, sizeof(DNS_Answer));
	buf_ptr += sizeof(DNS_Answer);

	return buf_ptr;
}

static uint8_t *
_serialize_question(uint8_t *buf, char *qname, uint16_t qtype, uint16_t qclass)
{
	uint8_t *buf_ptr = buf;

	uint16_t len = strlen(qname) + 1;
	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	DNS_Question quest;
	quest.QTYPE = hton(qtype);
	quest.QCLASS = hton(qclass);

	memcpy(buf_ptr, &quest, sizeof(DNS_Question));
	buf_ptr += sizeof(DNS_Question);

	return buf_ptr;
}

static uint8_t *
_serialize_PTR_services(uint8_t *buf)
{
	char *qname;
	uint16_t len;
	uint8_t *buf_ptr = buf;

	qname = hooks[HOOK_OSC].name;
	len = strlen(qname) + 1;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_SERVICES].name, MDNS_TYPE_PTR, MDNS_CLASS_INET, MDNS_DEFAULT_TTL, len);

	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	return buf_ptr;
}

static uint8_t * 
_serialize_PTR_osc(uint8_t *buf)
{
	char *qname;
	uint16_t len;
	uint8_t *buf_ptr = buf;

	qname = hooks[HOOK_CHIMAERA].name;
	len = strlen(qname) + 1;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_OSC].name, MDNS_TYPE_PTR, MDNS_CLASS_INET, MDNS_DEFAULT_TTL, len);

	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	return buf_ptr;
}

static uint8_t *
_serialize_TXT_chimaera(uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_CHIMAERA].name, MDNS_TYPE_TXT, MDNS_CLASS_INET, MDNS_DEFAULT_TTL, 1);

	*buf_ptr++ = 0x0; // text length

	return buf_ptr;
}

static uint8_t *
_serialize_SRV_chimaera(uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_CHIMAERA].name, MDNS_TYPE_SRV, MDNS_CLASS_INET, MDNS_DEFAULT_TTL, 6 + len_self);

	*buf_ptr++ = 0x0; // priority MSB
	*buf_ptr++ = 0x0; // priority LSB

	*buf_ptr++ = 0x0; // weight MSB
	*buf_ptr++ = 0x0; // weight LSB

	uint16_t port = config.config.socket.port[SRC_PORT];
	*buf_ptr++ = port >> 8; // port MSB
	*buf_ptr++ = port & 0xff; // port LSB

	memcpy(buf_ptr, hook_self, len_self);
	buf_ptr += len_self;

	return buf_ptr;
}

static uint8_t *
_serialize_A_chimaera(uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hook_self, MDNS_TYPE_A, MDNS_CLASS_INET, MDNS_DEFAULT_TTL, 4); //TODO FLUSH

	memcpy(buf_ptr, config.comm.ip, 4);
	buf_ptr += 4;

	return buf_ptr;
}

static uint8_t *
_serialize_PTR_arpa(uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hook_arpa, MDNS_TYPE_PTR, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_DEFAULT_TTL, len_self);

	memcpy(buf_ptr, hook_self, len_self);
	buf_ptr += len_self;

	return buf_ptr;
}

static void
_hook_services(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 5, 0, 0);
	// recommended by rfc6763
	tail = _serialize_PTR_services(tail); 
	tail = _serialize_PTR_osc(tail);
	tail = _serialize_TXT_chimaera(tail);
	tail = _serialize_SRV_chimaera(tail);
	tail = _serialize_A_chimaera(tail);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_osc(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 4, 0, 0);
	// recommended by rfc6763
	tail = _serialize_PTR_osc(tail);
	tail = _serialize_TXT_chimaera(tail);
	tail = _serialize_SRV_chimaera(tail);
	tail = _serialize_A_chimaera(tail);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_chimaera(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 3, 0, 0);
	// recommended by rfc6763
	tail = _serialize_TXT_chimaera(tail);
	tail = _serialize_SRV_chimaera(tail);
	tail = _serialize_A_chimaera(tail);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_arpa(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;

	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 1, 0, 0);
	tail = _serialize_PTR_arpa(tail);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static uint16_t
_update_hook_self(char *name)
{
	//TODO check maximal length

	int len;
	char *ptr = hook_self;

	len = strlen(name);
	*ptr++ = len;
	memcpy(ptr, name, len);
	ptr += len;

	len = strlen(localdomain);
	*ptr++ = len;
	memcpy(ptr, localdomain, len);
	ptr += len;

	*ptr = 0x0; // trailing zero
	
	return strlen(hook_self) + 1;
}

static uint16_t
_update_hook_arpa(uint8_t *ip)
{
	int len;
	int i;
	char octet [4];
	char *ptr = hook_arpa;

	// octets 4-1
	for(i=3; i>=0; i--)
	{
		sprintf(octet, "%d", ip[i]);
		len = strlen(octet);
		*ptr++ = len;
		memcpy(ptr, octet, len);
		ptr += len;
	}

	// in-addr
	len = strlen(in_addr);
	*ptr++ = len;
	memcpy(ptr, in_addr, len);
	ptr += len;

	// arpa
	len = strlen(arpa);
	*ptr++ = len;
	memcpy(ptr, arpa, len);
	ptr += len;

	*ptr = 0x0; // trailing zero

	return strlen(hook_arpa) + 1;
}

static uint8_t *
_dns_question(DNS_Query *query, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	char *qname;
	buf_ptr = _unroll_qname(query, buf_ptr, &qname);

	DNS_Question *question =(DNS_Question *)buf_ptr;
	question->QTYPE = hton(question->QTYPE);
	question->QCLASS = hton(question->QCLASS);
	buf_ptr += sizeof(DNS_Question);

	if( (question->QCLASS & 0x7fff) == MDNS_CLASS_INET) // check question class
		switch(question->QTYPE)
		{
			case MDNS_TYPE_A: // for mDNS lookup
			{
				// reply with current IP when there is a request for our name
				if(!strncmp(hook_self, qname, len_self))
				{
					uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
					uint8_t *tail = head;

					tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 1, 0, 0);
					tail = _serialize_answer(tail, hook_self, MDNS_TYPE_A, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_DEFAULT_TTL, 4);

					memcpy(tail, config.comm.ip, 4);
					tail += 4;

					udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
				}
				break;
			}
			case MDNS_TYPE_PTR: // for mDNS reverse-lookup and for mDNS-SD(service discovery)
			{
				DNS_PTR_Method *hook;
				for(hook=hooks; hook->name; hook++)
					if(!strcmp(hook->name, qname)) // is there a hook with this qname registered?
						hook->cb(query);
				break;
			}
			default:
				// ignore remaining question types, e.g. MDNS_TYPE_SRV
				break;
		}

	return buf_ptr;
}

static uint8_t *
_dns_answer(DNS_Query *query, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;

	char *qname;
	buf_ptr = _unroll_qname(query, buf_ptr, &qname);

	DNS_Answer *answer =(DNS_Answer *)buf_ptr;
	answer->RTYPE = hton(answer->RTYPE);
	answer->RCLASS = hton(answer->RCLASS);
	answer->TTL = htonl(answer->TTL);
	answer->RLEN = hton(answer->RLEN);
	buf_ptr += sizeof(DNS_Answer);

	switch(answer->RTYPE)
	{
		case MDNS_TYPE_A:
			// ignore when qname was not requested by us previously
			if(!resolve.cb || strcmp(qname, resolve.name))
				break;

			uint8_t *ip = buf_ptr; 
			if(ip_part_of_subnet(ip)) // check IP for same subnet TODO make this configurable
			{
				timer_pause(mdns_timer); // stop timeout timer

				resolve.cb(ip, resolve.data);

				// reset request
				resolve.name[0] = '\0';
				resolve.cb = NULL;
				resolve.data = NULL;
			}
			break;
		default:
			// ignore remaining answer types, e.g MDNS_TYPE_PTR, MDNS_TYPE_SRV, MDNS_TYPE_TXT
			break;
	}

	buf_ptr += answer->RLEN; // skip rdata

	return buf_ptr;
}

void 
mdns_dispatch(uint8_t *buf, uint16_t len)
{
	// update qname labels corresponding to self
	len_self = _update_hook_self(config.name);
	len_arpa = _update_hook_arpa(config.comm.ip);

	uint8_t *buf_ptr = buf;
	DNS_Query *query =(DNS_Query *)buf_ptr;

	// convert from network byteorder
	query->ID = hton(query->ID);
	query->FLAGS = hton(query->FLAGS);
	query->QDCOUNT = hton(query->QDCOUNT);
	query->ANCOUNT = hton(query->ANCOUNT);
	query->NSCOUNT = hton(query->NSCOUNT);
	query->ARCOUNT = hton(query->ARCOUNT);

	uint8_t qr =(query->FLAGS & MDNS_FLAGS_QR) >> MDNS_FLAGS_QR_BIT;
	//uint8_t opcode =(query->FLAGS & MDNS_FLAGS_OPCODE) >> MDNS_FLAGS_OPCODE_SHIFT;
	//uint8_t aa =(query->FLAGS & MDNS_FLAGS_AA) >> MDNS_FLAGS_AA_BIT;
	//uint8_t tc =(query->FLAGS & MDNS_FLAGS_TC) >> MDNS_FLAGS_TC_BIT;
	//uint8_t rd =(query->FLAGS & MDNS_FLAGS_RD) >> MDNS_FLAGS_RD_BIT;
	//uint8_t ra =(query->FLAGS & MDNS_FLAGS_RA) >> MDNS_FLAGS_RA_BIT;
	//uint8_t z =(query->FLAGS & MDNS_FLAGS_Z) >> MDNS_FLAGS_Z_SHIFT;
	//uint8_t rcode =(query->FLAGS & MDNS_FLAGS_RCODE) >> MDNS_FLAGS_RCODE_SHIFT;

	buf_ptr += sizeof(DNS_Query);

	uint_fast8_t i;
	switch(qr)
	{
		case 0x0: // mDNS Query Question
			for(i=0; i<query->QDCOUNT; i++) // walk all questions
				buf_ptr = _dns_question(query, buf_ptr);

			// ignore answers
			break;
		case 0x1: // mDNS Query Answer
			for(i=0; i<query->QDCOUNT; i++) // skip all questions
				buf_ptr += strlen(buf_ptr) + 1 + sizeof(DNS_Question);

			for(i=0; i<query->ANCOUNT; i++) // walk all answers
				buf_ptr = _dns_answer(query, buf_ptr);
			break;
	}
}

void mdns_announce()
{
	len_self = _update_hook_self(config.name);

	int i;
	for(i=0; i<2; i++)
	{
		uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
		uint8_t *tail = head;
		
		uint16_t id = rand() & 0xffff;
		tail = _serialize_query(tail, id, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 1, 0, 0);
		tail = _serialize_answer(tail, hook_self, MDNS_TYPE_A, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_DEFAULT_TTL, 4);
		//TODO append dns-sd services here, too?
		
		memcpy(tail, config.comm.ip, 4);
		tail += 4;
		
		udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);

		// wait a second
		uint32_t tick = systick_uptime();
		while(systick_uptime() - tick < SNTP_SYSTICK_RATE)
			;
	}
}

void
mdns_resolve_timeout()
{
	if(resolve.cb)
		resolve.cb(NULL, resolve.data);

	resolve.name[0] = '\0';
	resolve.cb = NULL;
	resolve.data = NULL;
}

//TODO implement timeout
uint_fast8_t
mdns_resolve(char *name, mDNS_Resolve_Cb cb, void *data)
{
	if(resolve.cb) // is there a mDNS resolve request already ongoing?
		return 0;

	// construct DNS label from mDNS name
	uint8_t *ref = resolve.name;
	uint8_t *s0 = name;
	uint8_t *s1;
	uint8_t len;
	while( (s1 = strchr(s0, '.')) )
	{
		len = s1 - s0;
		*ref++ = len;
		memcpy(ref, s0, len);
		ref += len;

		s0 = s1 + 1; // skip '.'
	}
	len = strlen(s0);
	*ref++ = len;
	memcpy(ref, s0, len);
	ref += len;
	*ref++ = 0x0; // trailing zero

	resolve.cb = cb;
	resolve.data = data;

	// serialize and send mDNS name request
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;

	uint16_t id = rand() & 0xffff;
	tail = _serialize_query(tail, id, 0, 1, 0, 0, 0);
	tail = _serialize_question(tail, resolve.name, MDNS_TYPE_A, MDNS_CLASS_INET);
	
	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);

	// start timer for timeout
	timer_pause(mdns_timer);
	mdns_timer_reconfigure();
	timer_resume(mdns_timer);

	return 1;
}

/*
 * Config
 */

static uint_fast8_t
_mdns_enabled(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	// needs a config save and reboot to take action
	return config_socket_enabled(&config.mdns.socket, path, fmt, argc, args);
}

/*
 * Query
 */

const nOSC_Query_Item mdns_tree [] = {
	nOSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _mdns_enabled, config_boolean_args),
};
