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

#ifndef _RTPMIDI_H
#define _RTPMIDI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NOTE_OFF 0x80
#define NOTE_ON 0x90
#define AFTER_TOUCH 0xa0
#define CONTROL_CHANGE 0xb0
#define PITCH_BEND 0xe0

#define MOD_WHEEL 0x01
#define BREATH_CONTROL 0x02
#define VOLUME 0x07
#define EFFECT_CONTROL_1 0x0c
#define EFFECT_CONTROL_2 0x0d

typedef struct _RTP_MIDI_List RTP_MIDI_List;
typedef struct _RTP_MIDI_Packet RTP_MIDI_Packet;

struct _RTP_MIDI_List {
	uint8_t delta_time;
	uint8_t midi_command;
};

RTP_MIDI_Packet *rtpmidi_new ();
void rtpmidi_free (RTP_MIDI_Packet *packet);

void rtpmidi_header_set (RTP_MIDI_Packet *packet, uint32_t sequence_number, uint32_t timestamp);
void rtpmidi_list_set (RTP_MIDI_Packet *packet, RTP_MIDI_List *list, uint16_t len);

uint16_t rtpmidi_serialize (RTP_MIDI_Packet *packet, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif // _RTPMIDI_H_
