/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

#ifndef DHCPC_H
#define DHCPC_H

#include <chimaera.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _DHCPC_State {
	DISCOVER, OFFER, REQUEST, ACK, LEASE
} DHCPC_State;

typedef struct _DHCPC DHCPC;

struct _DHCPC {
	DHCPC_State state;

	uint8_t ip [4];
	uint8_t server_ip [4];
	uint8_t gateway_ip [4];
	uint8_t subnet_mask [4];
	uint8_t router_ip [4];
	uint8_t server_id [4];
};

uint16_t dhcpc_discover (uint8_t *buf, uint16_t secs);
uint16_t dhcpc_request (uint8_t *buf, uint16_t secs);

void dhcpc_dispatch (uint8_t *buf, uint16_t size);

extern DHCPC dhcpc;

#ifdef __cplusplus
}
#endif

#endif // DHCPC_H
