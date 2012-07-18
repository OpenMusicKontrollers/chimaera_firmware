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

#ifndef _RTPMIDI_PRIVATE_H
#define _RTPMIDI_PRIVATE_H_

#include "rtpmidi.h"

struct _RTP_MIDI_Packet {
	// RTP header
	struct _header {
		uint8_t V : 2;
		uint8_t P : 1;
		uint8_t X : 1;
		uint8_t CC : 4;
		uint8_t M : 1;
		uint8_t PT : 7;
		uint16_t sequence_number;
		uint32_t timestamp;
	} header;

	struct _payload {
		// MIDI payload header
		struct _midi_header {
			uint8_t B : 1; // B == 0 LEN=4bit, B == 1 LEN=12bit
			uint8_t J : 1; // J == 0 no recovery journal
			uint8_t Z : 1; // Z == 1 no first delta time
			uint8_t P : 1;
			uint8_t LENa : 4;
			uint8_t LENb;
		} midi_header;

		RTP_MIDI_List *midi_list;
	} payload;
};

#endif // _RTPMIDI_PRIVATE_H_
