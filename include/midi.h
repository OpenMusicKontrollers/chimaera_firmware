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

#ifndef _MIDI_H_
#define _MIDI_H_

#include <stdint.h>

#include <chimaera.h>

enum _MIDI_COMMAND {
	MIDI_STATUS_NOTE_OFF 							= 0x80,
	MIDI_STATUS_NOTE_ON								= 0x90,
	MIDI_STATUS_NOTE_PRESSURE					= 0xa0,
	MIDI_STATUS_CONTROL_CHANGE				= 0xb0,

	MIDI_STATUS_CHANNEL_PRESSURE			= 0xd0,
	MIDI_STATUS_PITCH_BEND						= 0xe0,
	
	MIDI_CONTROLLER_MODULATION				= 0x01,
	MIDI_CONTROLLER_BREATH						= 0x02,
	MIDI_CONTROLLER_VOLUME						= 0x07,
	MIDI_CONTROLLER_PAN								= 0x0a,
	MIDI_CONTROLLER_EXPRESSION				= 0x0b,
	MIDI_CONTROLLER_EFFECT_CONTROL_1	= 0x0c,
	MIDI_CONTROLLER_EFFECT_CONTROL_2	= 0x0d,

	MIDI_CONTROLLER_ALL_NOTES_OFF			= 0x7b,
};

#define MIDI_MSV 0x00
#define MIDI_LSV 0x20

typedef struct _MIDI_Hash MIDI_Hash;

struct _MIDI_Hash {
	uint32_t sid;
	uint8_t key;
	uint8_t pos;
};

void midi_add_key(MIDI_Hash *hash, uint32_t sid, uint8_t key);
uint8_t midi_get_key(MIDI_Hash *hash, uint32_t sid);
uint8_t midi_rem_key(MIDI_Hash *hash, uint32_t sid);

//TODO create a MIDI meta engine, both OSC-MIDI and RTP-MIDI can refer to

#define MIDI_BOT (3.f*12.f - 0.5f - (SENSOR_N % 18 / 6.f))
#define MIDI_RANGE (SENSOR_N/3.f)

#endif // _MIDI_H_ 
