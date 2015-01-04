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

#include <chimaera.h>
#include <midi.h>

inline void
midi_add_key(MIDI_Hash *hash, uint32_t sid, uint8_t key)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == 0)
		{
			hash[k].sid = sid;
			hash[k].key = key;
			hash[k].pos = 0; //FIXME
			break;
		}
}

inline uint8_t
midi_get_key(MIDI_Hash *hash, uint32_t sid)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == sid)
			return hash[k].key;
	return 0; // not found
}

inline uint8_t
midi_rem_key(MIDI_Hash *hash, uint32_t sid)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == sid)
		{
			hash[k].sid = 0;
			return hash[k].key;
		}
	return 0; // not found
}
