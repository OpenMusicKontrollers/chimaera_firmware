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
midi_add_key(MIDI_Hash *hash, uint32_t sid, uint8_t key, uint8_t cha)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == 0)
		{
			hash[k].sid = sid;
			hash[k].key = key;
			hash[k].cha = cha;

			break;
		}
}

inline uint8_t
midi_get_key(MIDI_Hash *hash, uint32_t sid, uint8_t *key, uint8_t *ch)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == sid)
		{
			*key = hash[k].key;
			*ch = hash[k].cha;

			return 0; // success
		}
	return 1; // not found
}

inline uint8_t
midi_rem_key(MIDI_Hash *hash, uint32_t sid, uint8_t *key, uint8_t *ch)
{
	uint_fast8_t k;
	for(k=0; k<BLOB_MAX; k++)
		if(hash[k].sid == sid)
		{
			hash[k].sid = 0;
			*key = hash[k].key;
			*ch = hash[k].cha;

			return 0; // success
		}
	return 1; // not found
}

inline void
mpe_populate(mpe_t *mpe, uint8_t n_zones)
{
	n_zones %= ZONE_MAX + 1; // wrap around if n_zones > ZONE_MAX
	int8_t rem = CHAN_MAX % n_zones;
	const uint8_t span = (CHAN_MAX - rem) / n_zones - 1;
	uint8_t ptr = 0;

	mpe->n_zones = n_zones;
	zone_t *zones = mpe->zones;
	int8_t *channels = mpe->channels;

	for(uint8_t i=0;
		i<n_zones;
		rem--, ptr += 1 + zones[i++].span)
	{
		zones[i].base = ptr;
		zones[i].ref = 0;
		zones[i].span = span;
		if(rem > 0)
			zones[i].span += 1;
	}

	for(uint8_t i=0; i<CHAN_MAX; i++)
		channels[i] = 0;
}

inline uint8_t
mpe_acquire(mpe_t *mpe, uint8_t zone_idx)
{
	zone_idx %= mpe->n_zones; // wrap around if zone_idx > n_zones
	zone_t *zone = &mpe->zones[zone_idx];
	int8_t *channels = mpe->channels;

	int8_t min = INT8_MAX;
	uint8_t pos = zone->ref; // start search at current channel
	const uint8_t base_1 = zone->base + 1;
	for(uint8_t i = zone->ref; i < zone->ref + zone->span; i++)
	{
		const uint8_t ch = base_1 + (i % zone->span); // wrap to [0..span]
		if(channels[ch] < min) // check for less occupation
		{
			min = channels[ch]; // lower minimum
			pos = i; // set new minimally occupied channel
		}
	}

	const uint8_t ch = base_1 + (pos % zone->span); // wrap to [0..span]
	if(channels[ch] <= 0) // off since long
		channels[ch] = 1;
	else
		channels[ch] += 1; // increase occupation
	zone->ref = (pos + 1) % zone->span; // start next search from next channel

	return ch;
}

inline void
mpe_release(mpe_t *mpe, uint8_t zone_idx, uint8_t ch)
{
	zone_idx %= mpe->n_zones; // wrap around if zone_idx > n_zones
	ch %= CHAN_MAX; // wrap around if ch > CHAN_MAX
	zone_t *zone = &mpe->zones[zone_idx];
	int8_t *channels = mpe->channels;

	const uint8_t base_1 = zone->base + 1;
	for(uint8_t i = base_1; i < base_1 + zone->span; i++)
	{
		if( (i == ch) || (channels[i] <= 0) )
			channels[i] -= 1;
		// do not decrease occupied channels
	}
}
