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

#include <string.h>

#include <libmaple/systick.h>

#include "rtpmidi_private.h"

uint16_t sequence_number = 0;

RTP_Header rtp_header = {
	.V_P_X_CC = 0b00000000,
	.M_PT = 0b00000000,
	.sequence_number = 0x0000,
	.timestamp = 0x00000000,
	.SSRC = 0x00000000
};

RTP_MIDI_Session rtp_midi_session = {
	.rate = 48000
};

RTP_MIDI_Header rtp_midi_header = {
	.B_J_Z_P_LEN1 = 0b00000000,
	.LEN2 = 0b00000000
};

uint32_t
rtpmidi_timestamp (uint32_t rate)
{
	// rate: Hz (samp/s)
	// systick_resolution: 0.1ms = 1e-4s
	// timestamp = systick * (rate / systick_resolution), gives a rounding error with 44.1kHz, but not with 48kHz, 96kHz

	uint32_t ms0_1 = systick ();
	return ms0_1 * (rate * 1e-4); // TODO what when systick overflows after 119h?
	//TODO we need to base this on ntp corrected time!
}

uint16_t
rtpmidi_serialize (uint8_t *buf, RTP_MIDI_List *list, uint16_t len, uint32_t timestamp)
{
	rtp_header.sequence_number = hton (sequence_number++);
	rtp_header.timestamp = htonl (timestamp);
	//rtp_header.SSRC = htonl (); TODO

	uint8_t len12bit;
	if (len > 0b00001111)
	{
		len12bit = 1;
		rtp_midi_header.B_J_Z_P_LEN1 = 0b10000000 | (len & 0b00001111);
		rtp_midi_header.LEN2 = (len & 0b111111110000) >> 4;
	}
	else
	{
		len12bit = 0;
		rtp_midi_header.B_J_Z_P_LEN1 = 0b00000000 | (len & 0b00001111);
		rtp_midi_header.LEN2 = 0b00000000;
	}

	uint8_t *buf_ptr = buf;

	memcpy (buf_ptr, &rtp_header, sizeof (RTP_Header));
	buf_ptr += sizeof (RTP_Header);

	memcpy (buf_ptr, &rtp_midi_header, len12bit ? 16 : 8);
	buf_ptr += len12bit ? 16 : 8;

	memcpy (buf_ptr, list, len);
	buf_ptr += len;

	return buf_ptr - buf;
}

uint16_t
rtpmidi_deserialize (uint8_t *buf, uint8_t *midi)
{
	// TODO
}
