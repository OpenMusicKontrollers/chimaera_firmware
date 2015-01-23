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
static void _hook_instance(DNS_Query *);
static void _hook_arpa(DNS_Query *);

enum {HOOK_SERVICES=0, HOOK_OSC=1, HOOK_INSTANCE=2, HOOK_ARPA=3};

// self query name for DNS lookup
static int len_self;
static char hook_self [NAME_LENGTH+8];

// self query name for DNS revers-lookup
static int len_arpa;
static char hook_arpa [32];

// self instance name for DNS-SD
static int len_instance;
static char hook_instance_udp [NAME_LENGTH+20];
static char hook_instance_tcp [NAME_LENGTH+20];

// dns-sd PTR methods array
static DNS_PTR_Method hooks_udp [] = {
	[HOOK_SERVICES]	= {"\011_services\007_dns-sd\004_udp\005local", _hook_services},
	[HOOK_OSC]			= {"\004_osc\004_udp\005local", _hook_osc},
	[HOOK_INSTANCE]	= {hook_instance_udp, _hook_instance},
	[HOOK_ARPA]			= {hook_arpa, _hook_arpa},
	{NULL, NULL}
};

static DNS_PTR_Method hooks_tcp [] = {
	[HOOK_SERVICES]	= {"\011_services\007_dns-sd\004_udp\005local", _hook_services},
	[HOOK_OSC]			= {"\004_osc\004_tcp\005local", _hook_osc},
	[HOOK_INSTANCE]	= {hook_instance_tcp, _hook_instance},
	[HOOK_ARPA]			= {hook_arpa, _hook_arpa},
	{NULL, NULL}
};

// unrolled query name label
static char qname_unrolled [32]; //TODO big enough?

// unroll query name
static uint8_t *
_unroll_qname(DNS_Query *query, uint8_t *buf, char **qname)
{
	uint8_t *buf_ptr;
	char *ref = qname_unrolled;

	*qname = ref;

	for(buf_ptr=buf; *buf_ptr; )
		if( (buf_ptr[0] & 0xc0) == 0xc0) // it's a pointer at the beginning/end of the label
		{
			uint16_t offset =((buf_ptr[0] & 0x3f) << 8) | (buf_ptr[1] & 0xff); // get offset of label
			char *ptr = (char *)query + offset; // label
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
_serialize_answer_inline(uint8_t *buf, char *qname, uint16_t rtype, uint16_t rclass, uint32_t ttl, uint16_t **rlen)
{
	uint8_t *buf_ptr = buf;

	uint16_t len = strlen(qname) + 1;
	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	DNS_Answer a;
	a.RTYPE = hton(rtype);
	a.RCLASS = hton(rclass);
	a.TTL = htonl(ttl);
	a.RLEN = 0;
	
	*rlen = (uint16_t *)(buf_ptr + offsetof(DNS_Answer, RLEN));

	memcpy(buf_ptr, &a, sizeof(DNS_Answer));
	buf_ptr += sizeof(DNS_Answer);

	return buf_ptr;
}

static uint8_t *
_serialize_answer(uint8_t *buf, char *qname, uint16_t rtype, uint16_t rclass, uint32_t ttl, uint16_t rlen)
{
	uint16_t *len = NULL;
	uint8_t *buf_ptr = _serialize_answer_inline(buf, qname, rtype, rclass, ttl, &len);
	*len = hton(rlen);

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
_serialize_PTR_services(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	char *qname;
	uint16_t len;
	uint8_t *buf_ptr = buf;

	DNS_PTR_Method *hooks = NULL;
	if(config.config.osc.mode == OSC_MODE_UDP)
		hooks = hooks_udp;
	else
		hooks = hooks_tcp;

	qname = hooks[HOOK_OSC].name;
	len = strlen(qname) + 1;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_SERVICES].name, MDNS_TYPE_PTR, rclass, ttl, len);

	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	return buf_ptr;
}

static uint8_t * 
_serialize_PTR_osc(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	char *qname;
	uint16_t len;
	uint8_t *buf_ptr = buf;

	DNS_PTR_Method *hooks = NULL;
	if(config.config.osc.mode == OSC_MODE_UDP)
		hooks = hooks_udp;
	else
		hooks = hooks_tcp;

	qname = hooks[HOOK_INSTANCE].name;
	len = strlen(qname) + 1;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_OSC].name, MDNS_TYPE_PTR, rclass, ttl, len);

	memcpy(buf_ptr, qname, len);
	buf_ptr += len;

	return buf_ptr;
}

const char *TXT_TXTVERSION = "txtvers=1";
const char *TXT_VERSION = "version=1.1";
const char *TXT_URI = "uri=http://open-music-kontrollers.ch/chimaera";
const char *TXT_TYPES = "type=ifsbhdtTFNISmc";
const char *TXT_FRAMING = "framing=slip";

const char *TXT_RESET [] = {
	[RESET_MODE_FLASH_SOFT] = "reset=soft",
	[RESET_MODE_FLASH_HARD] = "reset=hard",
};
const char *TXT_STATIC = "static=1";
const char *TXT_DHCP = "dhcp=1";
const char *TXT_IPV4LL = "ipv4ll=1";

static uint8_t *
_serialize_TXT_record(uint8_t *buf, const char *record)
{
	uint8_t *buf_ptr = buf;

	uint8_t len = strlen(record);

	if(len > 0)
	{
		*buf_ptr++ = len;
		memcpy(buf_ptr, record, len);
		buf_ptr += len;
	}

	return buf_ptr;
}

static uint8_t *
_serialize_TXT_instance(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	uint8_t *buf_ptr = buf;
	uint16_t *len = NULL;

	DNS_PTR_Method *hooks = NULL;
	if(config.config.osc.mode == OSC_MODE_UDP)
		hooks = hooks_udp;
	else
		hooks = hooks_tcp;

	buf_ptr = _serialize_answer_inline(buf_ptr, hooks[HOOK_INSTANCE].name, MDNS_TYPE_TXT, rclass, ttl, &len);

	uint8_t *txt = buf_ptr;
	buf_ptr = _serialize_TXT_record(buf_ptr, TXT_TXTVERSION);
	buf_ptr = _serialize_TXT_record(buf_ptr, TXT_VERSION);
	buf_ptr = _serialize_TXT_record(buf_ptr, TXT_URI);
	buf_ptr = _serialize_TXT_record(buf_ptr, TXT_TYPES);
	if(config.config.osc.mode == OSC_MODE_SLIP)
		buf_ptr = _serialize_TXT_record(buf_ptr, TXT_FRAMING);
	buf_ptr = _serialize_TXT_record(buf_ptr, TXT_RESET[reset_mode]);
	if(config.dhcpc.enabled || config.ipv4ll.enabled)
	{
		if(config.dhcpc.enabled)
			buf_ptr = _serialize_TXT_record(buf_ptr, TXT_DHCP);
		if(config.ipv4ll.enabled)
			buf_ptr = _serialize_TXT_record(buf_ptr, TXT_IPV4LL);
	}
	else
		buf_ptr = _serialize_TXT_record(buf_ptr, TXT_STATIC);
	//TODO send UID?

	*len = hton(buf_ptr - txt);

	return buf_ptr;
}

static uint8_t *
_serialize_SRV_instance(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	uint8_t *buf_ptr = buf;

	DNS_PTR_Method *hooks = NULL;
	if(config.config.osc.mode == OSC_MODE_UDP)
		hooks = hooks_udp;
	else
		hooks = hooks_tcp;

	buf_ptr = _serialize_answer(buf_ptr, hooks[HOOK_INSTANCE].name, MDNS_TYPE_SRV, rclass, ttl, 6 + len_self);

	*buf_ptr++ = 0x0; // priority MSB
	*buf_ptr++ = 0x0; // priority LSB

	*buf_ptr++ = 0x0; // weight MSB
	*buf_ptr++ = 0x0; // weight LSB

	uint16_t port = config.config.osc.socket.port[SRC_PORT];
	*buf_ptr++ = port >> 8; // port MSB
	*buf_ptr++ = port & 0xff; // port LSB

	memcpy(buf_ptr, hook_self, len_self);
	buf_ptr += len_self;

	return buf_ptr;
}

static uint8_t *
_serialize_A_instance(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hook_self, MDNS_TYPE_A, rclass, ttl, 4);

	memcpy(buf_ptr, config.comm.ip, 4);
	buf_ptr += 4;

	return buf_ptr;
}

static uint8_t *
_serialize_PTR_arpa(uint8_t *buf, uint16_t rclass, uint32_t ttl)
{
	uint8_t *buf_ptr = buf;

	buf_ptr = _serialize_answer(buf_ptr, hook_arpa, MDNS_TYPE_PTR, rclass, ttl, len_self);

	memcpy(buf_ptr, hook_self, len_self);
	buf_ptr += len_self;

	return buf_ptr;
}

static void
_hook_services(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;
	uint16_t rclass = MDNS_CLASS_FLUSH | MDNS_CLASS_INET;
	uint32_t ttl = MDNS_TTL_75MIN;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 5, 0, 0);
	// recommended by rfc6763
	tail = _serialize_PTR_services(tail, rclass, ttl);
	tail = _serialize_PTR_osc(tail, rclass, ttl);
	tail = _serialize_TXT_instance(tail, rclass, ttl);
	tail = _serialize_SRV_instance(tail, rclass, ttl);
	tail = _serialize_A_instance(tail, rclass, ttl);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_osc(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;
	uint16_t rclass = MDNS_CLASS_FLUSH | MDNS_CLASS_INET;
	uint32_t ttl = MDNS_TTL_75MIN;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 4, 0, 0);
	// recommended by rfc6763
	tail = _serialize_PTR_osc(tail, rclass, ttl);
	tail = _serialize_TXT_instance(tail, rclass, ttl);
	tail = _serialize_SRV_instance(tail, rclass, ttl);
	tail = _serialize_A_instance(tail, rclass, ttl);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_instance(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;
	uint16_t rclass = MDNS_CLASS_FLUSH | MDNS_CLASS_INET;
	uint32_t ttl = MDNS_TTL_75MIN;

	// mandatory
	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 3, 0, 0);
	// recommended by rfc6763
	tail = _serialize_TXT_instance(tail, rclass, ttl);
	tail = _serialize_SRV_instance(tail, rclass, ttl);
	tail = _serialize_A_instance(tail, rclass, ttl);

	udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
}

static void
_hook_arpa(DNS_Query *query)
{
	uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
	uint8_t *tail = head;
	uint16_t rclass = MDNS_CLASS_FLUSH | MDNS_CLASS_INET;
	uint32_t ttl = MDNS_TTL_75MIN;

	tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 1, 0, 0);
	tail = _serialize_PTR_arpa(tail, rclass, ttl);

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
_update_hook_instance(char *name)
{
	int len;

	// UDP
	char *ptr = hook_instance_udp;
	const char *instance_postfix = hooks_udp[HOOK_OSC].name;

	len = strlen(name);
	*ptr++ = len;
	memcpy(ptr, name, len);
	ptr += len;

	len = strlen(instance_postfix);
	memcpy(ptr, instance_postfix, len);
	ptr += len;

	*ptr = 0x0; // trailing zero

	// TCP
	ptr = hook_instance_tcp;
	instance_postfix = hooks_tcp[HOOK_OSC].name;

	len = strlen(name);
	*ptr++ = len;
	memcpy(ptr, name, len);
	ptr += len;

	len = strlen(instance_postfix);
	memcpy(ptr, instance_postfix, len);
	ptr += len;

	*ptr = 0x0; // trailing zero
	
	return strlen(hook_instance_tcp) + 1;
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

	DNS_PTR_Method *hooks = NULL;
	if(config.config.osc.mode == OSC_MODE_UDP)
		hooks = hooks_udp;
	else
		hooks = hooks_tcp;

	char *qname;
	buf_ptr = _unroll_qname(query, buf_ptr, &qname);

	DNS_Question *question =(DNS_Question *)buf_ptr;
	question->QTYPE = hton(question->QTYPE);
	question->QCLASS = hton(question->QCLASS);
	buf_ptr += sizeof(DNS_Question);

	// TODO answer as unicast
	//uint_fast8_t unicast = question->QCLASS & MDNS_CLASS_UNICAST;

	if( (question->QCLASS & 0x7fff) == MDNS_CLASS_INET) // check question class
	{
		// for IP mDNS lookup
		if(  (question->QTYPE == MDNS_TYPE_A)
			|| (question->QTYPE == MDNS_TYPE_AAAA)
			|| (question->QTYPE == MDNS_TYPE_ANY) )
		{
			// reply with current IP when there is a request for our name
			if(!strncmp(hook_self, qname, len_self))
			{
				uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
				uint8_t *tail = head;

				tail = _serialize_query(tail, query->ID, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 1, 0, 0);
				//FIXME append negative response for IPv6 via NSEC record
				tail = _serialize_answer(tail, hook_self, MDNS_TYPE_A, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_TTL_75MIN, 4);

				memcpy(tail, config.comm.ip, 4);
				tail += 4;

				udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);
			}
		}

		// for mDNS reverse-lookup and for mDNS-SD(service discovery)
		if( (question->QTYPE == MDNS_TYPE_PTR) || (question->QTYPE == MDNS_TYPE_ANY) )
		{
			DNS_PTR_Method *hook;
			for(hook=hooks; hook->name; hook++)
				if(!strcmp(hook->name, qname)) // is there a hook with this qname registered?
					hook->cb(query);
		}
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
		
			//TODO handle conflicts when probing

			uint8_t *ip = (uint8_t *)buf_ptr; 
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
	(void)len;
	// update qname labels corresponding to self
	len_self = _update_hook_self(config.name);
	len_instance = _update_hook_instance(config.name);
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
				buf_ptr += strlen((char *)buf_ptr) + 1 + sizeof(DNS_Question);

			for(i=0; i<query->ANCOUNT; i++) // walk all answers
				buf_ptr = _dns_answer(query, buf_ptr);
			break;
	}
}

#if 0
void mdns_probe()
{
	len_self = _update_hook_self(config.name);
	len_instance = _update_hook_instance(config.name);
	len_arpa = _update_hook_arpa(config.comm.ip);

	int i;
	for(i=0; i<3; i++)
	{
		uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
		uint8_t *tail = head;
		
		uint16_t id = rand() & 0xffff;
		tail = _serialize_query(tail, id, MDNS_FLAGS_AA, 0, 1, 0, 0);
		tail = _serialize_question(tail, hook_self, MDNS_TYPE_ANY, MDNS_CLASS_INET);
		
		udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);

		// wait 250ms second
		uint32_t tick = systick_uptime();
		while(systick_uptime() - tick < SNTP_SYSTICK_RATE / 4)
			;

		//FIXME listen for collisions with hook_self
	}
}
#endif

static void
_mdns_tell(int count, uint16_t rclass, uint32_t ttl)
{
	len_self = _update_hook_self(config.name);
	len_instance = _update_hook_instance(config.name);
	len_arpa = _update_hook_arpa(config.comm.ip);

	int i;
	for(i=0; i<count; i++)
	{
		uint8_t *head = BUF_O_OFFSET(buf_o_ptr);
		uint8_t *tail = head;
		
		uint16_t id = rand() & 0xffff;
		tail = _serialize_query(tail, id, MDNS_FLAGS_QR | MDNS_FLAGS_AA, 0, 5, 0, 0);
		tail = _serialize_answer(tail, hook_self, MDNS_TYPE_A, rclass, ttl, 4);
		
		memcpy(tail, config.comm.ip, 4);
		tail += 4;

		// append dns-sd services here, too
		tail = _serialize_PTR_services(tail, rclass, ttl);
		tail = _serialize_PTR_osc(tail, rclass, ttl);
		tail = _serialize_TXT_instance(tail, rclass, ttl);
		tail = _serialize_SRV_instance(tail, rclass, ttl);
		tail = _serialize_A_instance(tail, rclass, ttl);
		
		udp_send(config.mdns.socket.sock, BUF_O_BASE(buf_o_ptr), tail-head);

		if(i == count-1)
			break;

		// wait a second
		uint32_t tick = systick_uptime();
		while(systick_uptime() - tick < SNTP_SYSTICK_RATE)
			;
	}
}

void
mdns_announce()
{
	_mdns_tell(2, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_TTL_75MIN);
}

void
mdns_update()
{
	_mdns_tell(1, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_TTL_75MIN);
}

void
mdns_goodbye()
{
	_mdns_tell(1, MDNS_CLASS_FLUSH | MDNS_CLASS_INET, MDNS_TTL_NULL);
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
mdns_resolve(const char *name, mDNS_Resolve_Cb cb, void *data)
{
	if(resolve.cb) // is there a mDNS resolve request already ongoing?
		return 0;

	// construct DNS label from mDNS name
	char *ref = resolve.name;
	const char *s0 = name;
	char *s1;
	char len;
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
_mdns_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	// needs a config save and reboot to take action
	return config_socket_enabled(&config.mdns.socket, path, fmt, argc, buf);
}

/*
 * Query
 */

const OSC_Query_Item mdns_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _mdns_enabled, config_boolean_args),
};
