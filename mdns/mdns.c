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

#include "mdns_private.h"

#include <chimaera.h>
#include <wiz.h>
#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

DNS_Query _q;
DNS_Answer _a;
DNS_Resolve resolve;
const char *localdomain = "local";

static uint8_t *
dns_question (DNS_Query *query, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;
	char *QNAME;
	DNS_Question *question;

	if ( (buf_ptr[0] & 0xc0) == 0xc0) // it's a poiner
	{
		uint16_t offset = ((buf_ptr[0] & 0x3f) << 8) | (buf_ptr[1] & 0xff);
		QNAME = (char *)query + offset;
		buf_ptr += 2;
	}
	else // its actually a name
	{
		QNAME = (char *)buf_ptr;
		buf_ptr += strlen (QNAME) + 1;
	}

	question = (DNS_Question *)buf_ptr;
	uint16_t qtype = hton (question->QTYPE);
	uint16_t qclass = hton (question->QCLASS);
	buf_ptr += sizeof (DNS_Question);

	switch (qtype)
	{
		case 0x0001: // A: name request -> return 32 bit IPv4 address
		{
			debug_str ("got a name request");
			uint8_t len;

			char *host = QNAME;
			len = host[0];

			char *domain = host + len+1;
			len = host[len+1];

			// only reply when there is a request for our name
			if ( strncmp (host+1, config.name, *host) || strncmp (domain+1, localdomain, *domain))
				break;

			debug_str ("this is our name");

			DNS_Query *q = &_q;
			DNS_Answer *a = &_a;

			q->ID = hton (query->ID);
			q->FLAGS = hton ( (1 << QR_BIT) | (1 << AA_BIT) );
			q->QDCOUNT = hton (0); // number of questions
			q->ANCOUNT = hton (1); // number of answers
			q->NSCOUNT = hton (0); // number of authoritative records
			q->ARCOUNT = hton (0); // number of resource records

			a->RTYPE = hton (0x0001);
			a->RCLASS = hton (0x8001);
			a->TTL = htonl (0x00000078);
			a->RLEN = hton (0x0004);

			uint8_t *ref = &buf_o[buf_o_ptr][WIZ_SEND_OFFSET];
			uint8_t *ptr = ref;

			memcpy (ptr, q, sizeof (DNS_Query));
			ptr += sizeof (DNS_Query);

			*ptr++ = strlen (config.name);
			memcpy (ptr, config.name, strlen (config.name));
			ptr += strlen (config.name);
			*ptr++ = strlen (localdomain);
			memcpy (ptr, localdomain, strlen (localdomain) + 1);
			ptr += strlen (localdomain) + 1; // inclusive string terminating '\0'

			memcpy (ptr, a, sizeof (DNS_Answer));
			ptr += sizeof (DNS_Answer);

			memcpy (ptr, config.comm.ip, 4);
			ptr += 4;

			udp_send (config.mdns.socket.sock, buf_o_ptr, ptr-ref);

			break;
		}
		case 0x000c: // PTR: canonical name request -> return name
		{
			//TODO
			debug_str ("got a pointer request");
			uint8_t len;

			char *ip4 = QNAME;
			len = ip4[0];

			char *ip3 = ip4 + len+1;
			len = ip4[len+1];

			char *ip2 = ip3 + len+1;
			len = ip3[len+1];

			char *ip1 = ip2 + len+1;
			len = ip2[len+1];

			char *addr = ip1 + len+1;
			len = ip1[len+1];

			char *arpa = addr + len+1;
			len = addr[len+1];

			*ip4 = 0x0; ip4++;
			*ip3 = 0x0; ip3++;
			*ip2 = 0x0; ip2++;
			*ip1 = 0x0; ip1++;
			*addr = 0x0; addr++;
			*arpa = 0x0; arpa++;

			//debug_int32 (atoi (ip4));
			//debug_int32 (atoi (ip3));
			//debug_int32 (atoi (ip2));
			//debug_int32 (atoi (ip1));
			//debug_str (addr);
			//debug_str (arpa);

			break;
		}
	}

	return buf_ptr;
}

static uint8_t *
dns_answer (DNS_Query *query, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;
	char *QNAME;
	DNS_Answer *answer;

	if ( (buf_ptr[0] & 0xc0) == 0xc0) // it's a poiner
	{
		uint16_t offset = ((buf_ptr[0] & 0x3f) << 8) | (buf_ptr[1] & 0xff);
		QNAME = (char *)query + offset;
		buf_ptr += 2;
		debug_str ("is a pointer");
	}
	else // its actually a name
	{
		QNAME = (char *)buf_ptr;
		buf_ptr += strlen (QNAME) + 1;
		debug_str ("is a new name");
	}

	answer = (DNS_Answer *)buf_ptr;
	uint16_t rtype = hton (answer->RTYPE);
	uint16_t rclass = hton (answer->RCLASS);
	uint32_t ttl = htonl (answer->TTL);
	uint16_t rlen = hton (answer->RLEN);
	buf_ptr += sizeof(DNS_Answer);

	switch (rtype)
	{
		case 0x0001: // A: host address
			debug_str ("got a host address answer");
			uint8_t len;

			char *host = QNAME;
			len = host[0];

			char *domain = host + len+1;
			len = host[len+1];

			// ignore when hostname was not requested by us
			if ( strncmp (host+1, resolve.name, *host) || strncmp (domain+1, localdomain, *domain))
				break;

			uint8_t *ip = buf_ptr; 

			// check IP for same subnet
			if (ip_part_of_subnet (ip)) // TODO make this configurable
				if (resolve.cb)
					resolve.cb (ip, resolve.data);

			// reset resolve struct
			/* //TODO
			resolve.name[0] = '\0';
			resolve.cb = NULL;
			resolve.data = NULL;
			*/

			break;
	}

	buf_ptr += rlen;

	return buf_ptr;
}

void 
mdns_dispatch (uint8_t *buf, uint16_t len)
{
	uint8_t *buf_ptr = buf;
	DNS_Query *query = (DNS_Query *)buf_ptr;

	// convert from network byteorder
	query->ID = hton (query->ID);
	query->FLAGS = hton (query->FLAGS);
	query->QDCOUNT = hton (query->QDCOUNT);
	query->ANCOUNT = hton (query->ANCOUNT);
	query->NSCOUNT = hton (query->NSCOUNT);
	query->ARCOUNT = hton (query->ARCOUNT);

	uint8_t qr = (query->FLAGS & QR) >> QR_BIT;
	uint8_t opcode = (query->FLAGS & OPCODE) >> OPCODE_BIT;
	uint8_t aa = (query->FLAGS & AA) >> AA_BIT;
	uint8_t tc = (query->FLAGS & TC) >> TC_BIT;
	uint8_t rd = (query->FLAGS & RD) >> RD_BIT;
	uint8_t ra = (query->FLAGS & RA) >> RA_BIT;
	uint8_t z = (query->FLAGS & Z) >> Z_BIT;
	uint8_t rcode = (query->FLAGS & RCODE) >> RCODE_BIT;

	char deb[256];
	sprintf (deb, "got mDNS Query: %i %i %i %i %i %i %i %i %i %i %i %i %i",
		query->ID,
		qr, opcode, aa, tc, rd, ra, z, rcode,
		query->QDCOUNT,
		query->ANCOUNT,
		query->NSCOUNT,
		query->ARCOUNT);
	//debug_str (deb);
	buf_ptr += sizeof (DNS_Query);

	uint8_t i;
	switch (qr)
	{
		case 0x0:
			debug_str ("got mDNS Query Question");
			debug_int32 (query->QDCOUNT);

			for (i=0; i<query->QDCOUNT; i++) // parse any question
				buf_ptr = dns_question (query, buf_ptr);
			break;
		case 0x1:
			debug_str ("got mDNS Query Answer");
			debug_int32 (query->QDCOUNT);
			debug_int32 (query->ANCOUNT);

			for (i=0; i<query->QDCOUNT; i++) // skip any repeated questions
				buf_ptr += strlen(buf_ptr) + 1 + sizeof(DNS_Question);

			for (i=0; i<query->ANCOUNT; i++) // parse any answer
				buf_ptr = dns_answer (query, buf_ptr);
			break;
	}
}

void
mdns_resolve (const char *name, mDNS_Resolve_Cb cb, void *data)
{
	uint8_t len = strstr (name, localdomain) - name - 1;
	strncpy (resolve.name, name, len);
	resolve.name[len] = '\0';

	resolve.cb = cb;
	resolve.data = data;

	DNS_Query query;
	DNS_Question quest;

	query.ID = hton (rand() & 0xffff);
	query.FLAGS = hton (0); // standard query
	query.QDCOUNT = hton (1); // number of questions
	query.ANCOUNT = hton (0); // number of answers
	query.NSCOUNT = hton (0); // number of authoritative records
	query.ARCOUNT = hton (0); // number of resource records

	quest.QTYPE = hton (0x0001); // A (host address)
	quest.QCLASS = hton (0x0001); // IN

	uint8_t *ref = &buf_o[buf_o_ptr][WIZ_SEND_OFFSET];
	uint8_t *ptr = ref;

	memcpy (ptr, &query, sizeof (DNS_Query));
	ptr += sizeof (DNS_Query);

	*ptr++ = strlen (resolve.name);
	memcpy (ptr, resolve.name, strlen (resolve.name));
	ptr += strlen (resolve.name);

	*ptr++ = strlen (localdomain);
	memcpy (ptr, localdomain, strlen (localdomain) + 1);
	ptr += strlen (localdomain) + 1; // inclusive string terminating '\0'

	memcpy (ptr, &quest, sizeof (DNS_Question));
	ptr += sizeof (DNS_Question);

	udp_send (config.mdns.socket.sock, buf_o_ptr, ptr-ref);
}
