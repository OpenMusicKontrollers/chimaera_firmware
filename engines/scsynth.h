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

#ifndef _SCSYNTH_H_
#define _SCSYNTH_H_

#include <stdint.h>

#include <netdef.h>

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
extern nOSC_Bundle_Item scsynth_osc;
extern CMC_Engine scsynth_engine;
extern const nOSC_Query_Item scsynth_tree [3];

void scsynth_init();
uint_fast8_t scsynth_group_get(uint_fast8_t gid, char **name, uint16_t *sid, uint16_t *group, uint16_t *out, uint8_t *arg, uint8_t *alloc, uint8_t *gate, uint8_t *add_action, uint8_t *is_group);
uint_fast8_t scsynth_group_set(uint_fast8_t gid, char *name, uint16_t sid, uint16_t group, uint16_t out, uint8_t arg, uint8_t alloc, uint8_t gate, uint8_t add_action, uint8_t is_group);

#endif // _SCSYNTH_H_
