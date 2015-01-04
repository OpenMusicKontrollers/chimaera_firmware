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

#ifndef _DHCPC_H_
#define _DHCPC_H_

#include <oscquery.h>

typedef enum _DHCPC_State {
	DISCOVER, OFFER, REQUEST, ACK, LEASE, TIMEOUT, CLAIMED, DECLINE
} DHCPC_State;

typedef struct _DHCPC DHCPC;

struct _DHCPC {
	DHCPC_State state;
	uint8_t delay;
	uint32_t timeout;
	uint32_t leastime;

	uint8_t ip [4];
	uint8_t server_ip [4];
	uint8_t router_ip [4];
	uint8_t subnet_mask [4];
};

uint_fast8_t dhcpc_claim(uint8_t *ip, uint8_t *gateway, uint8_t *subnet);
uint_fast8_t dhcpc_refresh(void);

extern DHCPC dhcpc;
extern const OSC_Query_Item dhcpc_tree [1];

#endif // _DHCPC_H_
