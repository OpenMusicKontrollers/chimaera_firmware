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

#ifndef _MDNS_H_
#define _MDNS_H_

#include <stdint.h>

#include <oscquery.h>

extern const OSC_Query_Item mdns_tree [1];

typedef void(*mDNS_Resolve_Cb)(uint8_t *ip, void *data);

void mdns_dispatch(uint8_t *buf, uint16_t len);

void mdns_announce(void);
void mdns_update(void);
void mdns_goodbye(void);

//TODO allow multiple concurrent resolvings
void mdns_resolve_timeout(void);
uint_fast8_t mdns_resolve(const char *name, mDNS_Resolve_Cb cb, void *data);

#endif // _MDNS_H_
