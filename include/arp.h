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

#ifndef _ARP_H_
#define _ARP_H_

#include <stdint.h>

/*
 * send an ARP probe: investigate whether another hardware uses ip,
 * e.g. whether there is an IP collision.
 * returns 1 on collision, 0 otherwise
 */
uint_fast8_t arp_probe(uint8_t sock, uint8_t *ip);

/*
 * announce ip via ARP
 */
void arp_announce(uint8_t sock, uint8_t *ip);

#endif // _ARP_H_
