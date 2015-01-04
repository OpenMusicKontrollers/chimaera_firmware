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
