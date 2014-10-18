/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#ifndef _OSCMIDI_H_
#define _OSCMIDI_H_

#include <cmc.h>
#include <oscquery.h>

typedef enum _OSC_MIDI_Mapping OSC_MIDI_Mapping;
typedef enum _OSC_MIDI_Format OSC_MIDI_Format;
typedef struct _OSC_MIDI_Group OSC_MIDI_Group;

enum _OSC_MIDI_Mapping {
	OSC_MIDI_MAPPING_NOTE_PRESSURE,
	OSC_MIDI_MAPPING_CHANNEL_PRESSURE,
	OSC_MIDI_MAPPING_CONTROL_CHANGE
};

enum _OSC_MIDI_Format {
	OSC_MIDI_FORMAT_MIDI,
	OSC_MIDI_FORMAT_INT32
};

struct _OSC_MIDI_Group {
	uint8_t mapping;
	uint8_t control;
	float offset;
	float range;
};

extern OSC_MIDI_Group *oscmidi_groups;
extern CMC_Engine oscmidi_engine;
extern const OSC_Query_Item oscmidi_tree [6];

#endif // _OSCMIDI_H_
