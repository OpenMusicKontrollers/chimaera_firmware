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

#ifndef _SCSYNTH_H_
#define _SCSYNTH_H_

#include <cmc.h>
#include <oscquery.h>

typedef enum _SCSynth_Add_Action SCSynth_Add_Action;
typedef struct _SCSynth_Group SCSynth_Group;

enum _SCSynth_Add_Action {
	SCSYNTH_ADD_TO_HEAD = 0,
	SCSYNTH_ADD_TO_TAIL = 1
};

struct _SCSynth_Group {
	char name[8];
	uint16_t sid;
	uint16_t group;
	uint16_t out;
	uint8_t arg;
	uint8_t alloc;
	uint8_t gate;
	uint8_t add_action;
	uint8_t is_group;
};

extern SCSynth_Group *scsynth_groups;
extern CMC_Engine scsynth_engine;
extern const OSC_Query_Item scsynth_tree [4];

#endif // _SCSYNTH_H_
