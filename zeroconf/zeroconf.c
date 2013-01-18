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

#include "zeroconf_private.h"
#include <chimaera.h>
#include <wiz.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

DNS_Query _q;
DNS_Answer _a;

void 
zeroconf_IPv4LL_random (uint8_t *ip)
{
	ip[0] = 169;
	ip[1] = 254;
	ip[2] = 1 + rand () / (RAND_MAX / 253);
	ip[3] = rand () / (RAND_MAX / 255);
}

static void
dns_question (DNS_Query *query, uint8_t *buf, uint16_t len)
{
	uint8_t *buf_ptr = buf;
	char *QNAME;
	DNS_Question *question;

	QNAME = (char *)buf_ptr;

	/*
	for (uint8_t i=0; i<strlen(QNAME); i++)
	{
		char tmp[] = {'a', '\0'};
		tmp[0] = QNAME[i];
		debug_int32 (tmp[0]);
		debug_str (tmp);
	}
	*/

	buf_ptr += strlen (QNAME) + 1;

	question = (DNS_Question *)buf_ptr;
	uint16_t qtype = hton (question->QTYPE);
	uint16_t qclass = hton (question->QCLASS);
	debug_int32 (qtype);
	debug_int32 (qclass);

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

			*host = 0x0; host++;
			*domain = 0x0; domain++; 

			debug_str (host);
			debug_str (domain);

			// only reply when there is a request for our name
			if ( strcmp (host, "chimaera") || strcmp (domain, "local"))
				break;

			debug_str ("this is our name");

			DNS_Query *q = &_q;
			DNS_Answer *a = &_a;

			q->ID = hton (query->ID);
			q->FLAGS = hton ( (1 << QR_BIT) | (1 << AA_BIT) );
			q->ANCOUNT = hton (1); // number of answers

			a->RTYPE = hton (0x0001);
			a->RCLASS = hton (0x8001);
			a->TTL = htonl (0x00000078);
			a->RLEN = hton (0x0004);

			uint8_t *ref = &buf_o[buf_o_ptr][WIZ_SEND_OFFSET];
			uint8_t *ptr = ref;

			memcpy (ptr, q, sizeof (DNS_Query));
			ptr += sizeof (DNS_Query);

			//char *name = "\008chimaera\005local";
			char *name = "\x08""chimaera""\x05""local";
			memcpy (ptr, name, strlen (name) + 1); // copy 0x0 string end, too
			ptr += strlen (name) + 1;

			memcpy (ptr, a, sizeof (DNS_Answer));
			ptr += sizeof (DNS_Answer);

			memcpy (ptr, config.comm.ip, 4);
			ptr += 4;

			udp_send (config.zeroconf.socket.sock, buf_o_ptr, ptr-ref);

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

			debug_int32 (atoi (ip4));
			debug_int32 (atoi (ip3));
			debug_int32 (atoi (ip2));
			debug_int32 (atoi (ip1));
			debug_str (addr);
			debug_str (arpa);

			break;
		}
	}
}

static void
dns_answer (DNS_Query *query, uint8_t *buf, uint16_t len)
{
	//TODO
}

void 
zeroconf_dispatch (uint8_t *buf, uint16_t len)
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
	debug_str (deb);
	buf_ptr += sizeof (DNS_Query);

	switch (qr)
	{
		case 0x0:
			debug_str ("got mDNS Query Question");
			dns_question (query, buf_ptr, len - (buf_ptr-buf));
			break;
		case 0x1:
			debug_str ("got mDNS Query Answer");
			dns_answer (query, buf_ptr, len - (buf_ptr-buf));
			break;
	}
}

void
zeroconf_publish (const char *name, const char *type, uint16_t port)
{
	// TODO
	// zeroconf_publish ("chimaera", "_osc._udp", "_tuio2._sub._osc._udp", 3333);
}

void
zeroconf_discover (const char *name, const char *type, uint16_t port)
{
	// TODO
}
