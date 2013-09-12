/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <chimaera.h>
#include <midi.h>

inline void
midi_add_key (MIDI_Hash *hash, uint32_t sid, uint8_t key)
{
	uint_fast8_t k;
	for (k=0; k<BLOB_MAX; k++)
		if (hash[k].sid == 0)
		{
			hash[k].sid = sid;
			hash[k].key = key;
			break;
		}
}

inline uint8_t
midi_get_key (MIDI_Hash *hash, uint32_t sid)
{
	uint_fast8_t k;
	for (k=0; k<BLOB_MAX; k++)
		if (hash[k].sid == sid)
			return hash[k].key;
	return 0; // not found
}

inline uint8_t
midi_rem_key (MIDI_Hash *hash, uint32_t sid)
{
	uint_fast8_t k;
	for (k=0; k<BLOB_MAX; k++)
		if (hash[k].sid == sid)
		{
			hash[k].sid = 0;
			return hash[k].key;
		}
	return 0; // not found
}
