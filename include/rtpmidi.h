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

#ifndef RTPMIDI_H
#define RTPMIDI_H

#include <chimaera.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _RTP_MIDI_List RTP_MIDI_List;
struct _RTP_MIDI_List {
	uint8_t delta_time; // 0b0ddddddd
	uint8_t midi [3]; // 0x80 0x7f 0x7f
};

typedef enum _MIDI_COMMAND {
	NOTE_ON						= 0x80,
	NOTE_OFF 					= 0x90,
	AFTER_TOUCH				= 0xa0,
	CONTROL_CHANGE		= 0xb0,
	PITCH_BEND				= 0xe0,
	
	MODULATION				= 0x01,
	BREATH						= 0x02,
	VOLUME						= 0x07,
	PAN								= 0x0a,
	EXPRESSION				= 0x0b,
	EFFECT_CONTROL_1	= 0x0c,
	EFFECT_CONTROL_2	= 0x0d
} MIDI_COMMAND; //TODO check whether ((aligned(1)))

uint32_t rtpmidi_timestamp (uint32_t rate);

uint16_t rtpmidi_serialize (uint8_t *buf, RTP_MIDI_List *list, uint16_t len, uint32_t timestamp);
uint16_t rtpmidi_deserialize (uint8_t *buf, uint8_t *midi);

#ifdef __cplusplus
}
#endif

#endif // RTPMIDI_H
