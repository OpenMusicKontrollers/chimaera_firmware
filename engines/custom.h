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

#ifndef _CUSTOM_H_
#define _CUSTOM_H_

#include <cmc.h>

#define CUSTOM_PATH_LEN		16
#define CUSTOM_FMT_LEN		16
#define CUSTOM_ARGS_LEN		64

typedef struct _Custom_Item  Custom_Item;

struct _Custom_Item {
	char path [CUSTOM_PATH_LEN];
	char fmt [CUSTOM_FMT_LEN];
	char args [CUSTOM_ARGS_LEN];
};

extern nOSC_Bundle_Item custom_osc;
extern CMC_Engine custom_engine;
extern const nOSC_Query_Item custom_tree [5];

void custom_init ();

#endif // _CUSTOM_H_
