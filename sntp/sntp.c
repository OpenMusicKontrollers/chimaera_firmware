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

#include <string.h>

#include <chimutil.h>

#include "sntp_private.h"

uint16_t 
sntp_request (uint8_t *buf, timestamp64u_t now)
{
	uint16_t len = sizeof (sntp_t);

	memset (buf, 0, len);

	sntp_t *request = (sntp_t *) buf;

	request->li_vn_mode = (0x0<<6) + (0x4<<3) + 0x3;
	request->transmit_timestamp = htonll (now);
	
	return len;
}

timestamp64u_t *
sntp_dispatch (uint8_t *buf, timestamp64u_t T4, timestamp64u_t *roundtrip_delay, timestamp64s_t *clock_offset)
{
	sntp_t *answer = (sntp_t *) buf;

	fix_32_32_t *t1 = &answer->originate_timestamp;
	fix_32_32_t *t2 = &answer->receive_timestamp;
	fix_32_32_t *t3 = &answer->transmit_timestamp;
	fix_32_32_t *t4 = (fix_32_32_t *)&T4;

	*t1 = htonll (*t1);
	*t2 = htonll (*t2);
	*t3 = htonll (*t3);

	//Originate Timestamp     T1   time request sent by client
  //Receive Timestamp       T2   time request received by server
  //Transmit Timestamp      T3   time reply sent by server
  //Destination Timestamp   T4   time reply received by client

	//The roundtrip delay d and local clock offset t are defined as
	//d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.

	fix_32_32_t delay = (*t4 - *t1) - (*t3 - *t2);
	memcpy (roundtrip_delay, &delay, 8);

	fix_s31_32_t offset = (fix_s31_32_t)(*t2 - *t1) - (fix_s31_32_t)(*t4 - *t3);
	memcpy (clock_offset, &offset, 8);

	return (timestamp64u_t *)t3;
}
