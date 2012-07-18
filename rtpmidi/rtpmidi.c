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

#include "rtpmidi_private.h"

#include <netdef.h>

#include <stdlib.h>
#include <string.h>

RTP_MIDI_Packet *
rtpmidi_new ()
{
	RTP_MIDI_Packet *packet = calloc (1, sizeof (RTP_MIDI_Packet));

	packet->header.V = 2;
	packet->header.P = 0;
	packet->header.X = 0;
	packet->header.CC = 0;
	packet->header.M = 0;
	packet->header.PT = 69;

	packet->payload.midi_header.B = 0;
	packet->payload.midi_header.J = 0;
	packet->payload.midi_header.Z = 0;
	packet->payload.midi_header.P = 0;
	packet->payload.midi_header.LENa = 12;
	packet->payload.midi_header.LENb = 0;

	return packet;
}

void
rtpmidi_free (RTP_MIDI_Packet *packet)
{
	free (packet);
}

void 
rtpmidi_header_set (RTP_MIDI_Packet *packet, uint32_t sequence_number, uint32_t timestamp)
{
	packet->header.sequence_number = sequence_number;
	packet->header.timestamp = timestamp;
}

void 
rtpmidi_list_set (RTP_MIDI_Packet *packet, RTP_MIDI_List *list, uint16_t len)
{
	if (len > 0xf)
	{
		packet->payload.midi_header.B = 1;
		packet->payload.midi_header.LENa = (len & 0xf00) >> 8;
		packet->payload.midi_header.LENb = len & 0x0ff;
	}
	else
	{
		packet->payload.midi_header.B = 0;
		packet->payload.midi_header.LENa = len & 0xf;
	}

	packet->payload.midi_list = list;
}

uint16_t 
rtpmidi_serialize (RTP_MIDI_Packet *packet, uint8_t *buf)
{
	uint8_t *buf_ptr = buf;
	uint8_t b;
	uint32_t n;

	// RTP header
	b = (packet->header.V << 6) 
		| (packet->header.P << 5) 
		| (packet->header.X << 4) 
		| (packet->header.CC);
	*buf_ptr++ = b;

	b = (packet->header.M << 7) | (packet->header.PT);
	*buf_ptr++ = b;

	n = htonl (packet->header.sequence_number);
	memcpy (buf_ptr, &n, 4);
	buf_ptr += 4;

	n = htonl (packet->header.timestamp);
	memcpy (buf_ptr, &n, 4);
	buf_ptr += 4;

	// MIDI header
	b = (packet->payload.midi_header.B << 7)
		| (packet->payload.midi_header.J << 6) 
		| (packet->payload.midi_header.Z << 5)
		| (packet->payload.midi_header.P << 4)
		| (packet->payload.midi_header.LENa);
	*buf_ptr++ = b;

	if (packet->payload.midi_header.B)
		*buf_ptr++ = packet->payload.midi_header.LENb;

	// MIDI List
	uint16_t len;
	uint16_t i;

	len = packet->payload.midi_header.LENa;
	if (packet->payload.midi_header.B)
		len = (len << 8) | packet->payload.midi_header.LENb;

	memcpy (buf_ptr, packet->payload.midi_list, len);
	buf_ptr += len;

	return buf_ptr - buf;
}
