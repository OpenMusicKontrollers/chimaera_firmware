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

#ifndef RTPMIDI_PRIVATE_H
#define RTPMIDI_PRIVATE_H

#include <rtpmidi.h>

typedef struct _RTP_Header RTP_Header;
typedef struct _RTP_MIDI_Session RTP_MIDI_Session;
typedef struct _RTP_MIDI_Header RTP_MIDI_Header;

struct _RTP_Header {
	uint8_t V_P_X_CC;
	uint8_t M_PT;
	uint16_t sequence_number;
	uint32_t timestamp;
	uint32_t SSRC;
};

struct _RTP_MIDI_Session {
	uint32_t rate;
};

struct _RTP_MIDI_Header {
	/*
	 B: 0: LEN is 4bits, 1: LEN is 12bits
	 J: 0: no journal, 1: journal
	 Z: 0: delta_0 existent, 1: delta_0 absent
	 LEN: number of octets in MIDI list
	 */
	uint8_t B_J_Z_P_LEN1;
	uint8_t LEN2;
};

#define LSV 0x00
#define MSV 0x20

#endif // RTPMIDI_PRIVATE_H
