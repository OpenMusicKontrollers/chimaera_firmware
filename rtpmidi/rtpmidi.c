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

#include <string.h>
#include <math.h> // floor

#include "rtpmidi_private.h"

//TODO implement RTCP (Real Time Control Protocol)

uint16_t sequence_number = 0;

RTP_Header rtp_header = {
	.V_P_X_CC = 0b10000000, // version=0b10(2), padding=0b0, extension=0b0, csrc_count=0b0000
	.M_PT = 0b01100000, // marker=0b0, payload_type=0b1100000(96) RTP_MIDI
	.sequence_number = 0x0000,
	.timestamp = 0x00000000,
	.SSRC = 0x00000000
};

RTP_MIDI_Session rtp_midi_session = {
	.rate = 48000.0 //TODO make this configurable via RTCP
};

RTP_MIDI_Header rtp_midi_header = {
	.B_J_Z_P_LEN1 = 0b10000000, // B=0b1(len=12bits), J=0b0, Z=0b0, P = 0b0
	.LEN2 = 0b00000000
};

RTP_MIDI_List rtp_midi_list [BLOB_MAX*3];
uint_fast8_t nlist = 0;

uint16_t seq_offset;
uint16_t seq_num;
uint32_t timestamp;
nOSC_Timestamp last_tt;

uint8_t rtpmidi_keys [BLOB_MAX]; // FIXME we should use something bigger or a hash instead

void
rtpmidi_init ()
{
	seq_offset = rand ();
	seq_num = seq_offset;
	timestamp = rand ();
	last_tt = 0ULLK;
	rtp_header.SSRC = htonl (rand ());
}

uint16_t
rtpmidi_serialize (uint8_t *buf)
{
	if (!nlist)
		return 0;

	rtp_header.sequence_number = hton (seq_num);
	rtp_header.timestamp = htonl (timestamp);

	uint_fast8_t len12bit;
	uint16_t len = nlist * sizeof(RTP_MIDI_List);
	if (len > 0b00001111)
	{
		len12bit = 1;
		rtp_midi_header.B_J_Z_P_LEN1 = 0b10000000 | (len & 0b00001111);
		rtp_midi_header.LEN2 = len >> 4;
	}
	else // len <= 0b00001111
	{
		len12bit = 0;
		rtp_midi_header.B_J_Z_P_LEN1 = 0b00000000 | (len & 0b00001111);
		rtp_midi_header.LEN2 = 0b00000000;
	}

	uint8_t *buf_ptr = buf;
	memcpy (buf_ptr, &rtp_header, sizeof(RTP_Header));
	buf_ptr += sizeof(RTP_Header);

	memcpy (buf_ptr, &rtp_midi_header, len12bit ? 2 : 1);
	buf_ptr += len12bit ? 2 : 1;

	memcpy (buf_ptr, rtp_midi_list, len);
	buf_ptr += len;

	return buf_ptr - buf;
}

void
rtpmidi_engine_frame_cb (uint32_t fid, nOSC_Timestamp now, nOSC_Timestamp offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	seq_num = seq_offset + fid;

	nOSC_Timestamp dt;
	uint32_t df;

	if (last_tt > 0ULLK)
	{
		dt = now - last_tt;
		df = dt * rtp_midi_session.rate;
		timestamp += df;
	}
	last_tt = now;

	nlist = 0;
}

void
rtpmidi_engine_on_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	RTP_MIDI_List *itm;
	uint8_t ch = gid % 0xf;
	uint_fast8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = floor (X);
	rtpmidi_keys[pos] = key;
	
	itm = &rtp_midi_list[nlist];
	itm->delta_time = 0b00000000;
	itm->midi[0] = NOTE_ON | ch;
	itm->midi[1] = key;
	itm->midi[2] = 0x7f;
	nlist++;
}

void
rtpmidi_engine_off_cb (uint32_t sid, uint16_t gid, uint16_t pid)
{
	RTP_MIDI_List *itm;
	uint8_t ch = gid % 0xf;
	uint_fast8_t pos = sid % BLOB_MAX;
	uint8_t key = rtpmidi_keys[pos];
	
	itm = &rtp_midi_list[nlist];
	itm->delta_time = 0b00000000;
	itm->midi[0] = NOTE_OFF | ch;
	itm->midi[1] = key;
	itm->midi[2] = 0x00;
	nlist++;
}

void
rtpmidi_engine_set_cb (uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	RTP_MIDI_List *itm;
	uint8_t ch = gid % 0xf;
	uint_fast8_t pos = sid % BLOB_MAX;
	float X = config.oscmidi.offset - 0.5 + x * 48.0;
	uint8_t key = rtpmidi_keys[pos];
	uint16_t bend = (X - key) * 170.65 + 0x2000; // := (X - key) / 48.0 * 0x1fff + 0x2000;

	uint16_t eff = y * 0x3fff;

	itm = &rtp_midi_list[nlist];
	itm->delta_time = 0b00000000;
	itm->midi[0] = PITCH_BEND | ch;
	itm->midi[1] = bend & 0x7f;
	itm->midi[2] = bend >> 7;
	nlist++;

	itm = &rtp_midi_list[nlist];
	itm->delta_time = 0b00000000;
	itm->midi[0] = CONTROL_CHANGE | ch;
	itm->midi[1] = config.oscmidi.effect | LSV;
	itm->midi[2] = eff & 0x7f;
	nlist++;

	itm = &rtp_midi_list[nlist];
	itm->delta_time = 0b00000000;
	itm->midi[0] = CONTROL_CHANGE | ch;
	itm->midi[1] = config.oscmidi.effect | MSV;
	itm->midi[2] = eff >> 7;
	nlist++;
}

CMC_Engine rtpmidi_engine = {
	rtpmidi_engine_frame_cb,
	rtpmidi_engine_on_cb,
	rtpmidi_engine_off_cb,
	rtpmidi_engine_set_cb
};
