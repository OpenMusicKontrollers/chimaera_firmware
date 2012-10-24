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

#include <libmaple/systick.h>

#include <chimutil.h>

#include "sntp_private.h"

fix_32_32_t t0 = 0.0ULLK;

uint16_t 
sntp_request (uint8_t *buf, timestamp64u_t t3)
{
	uint16_t len = sizeof (sntp_t);

	memset (buf, 0, len);

	sntp_t *request = (sntp_t *) buf;

	request->li_vn_mode = (0x0<<6) + (0x4<<3) + 0x3;
	request->transmit_timestamp.part.sec = htonl (t3.part.sec);
	request->transmit_timestamp.part.frac = htonl (t3.part.frac);
	
	return len;
}

void
sntp_dispatch (uint8_t *buf, timestamp64u_t t4)
{
	sntp_t *answer = (sntp_t *) buf;

	timestamp64u_t t1, t2, t3;

	t1.part.sec = htonl (answer->originate_timestamp.part.sec);
	t1.part.frac = htonl (answer->originate_timestamp.part.frac);

	t2.part.sec = htonl (answer->receive_timestamp.part.sec);
	t2.part.frac = htonl (answer->receive_timestamp.part.frac);

	t3.part.sec = htonl (answer->transmit_timestamp.part.sec);
	t3.part.frac = htonl (answer->transmit_timestamp.part.frac);

	fix_32_32_t T1 = utime2fix (t1);
	fix_32_32_t T2 = utime2fix (t2);
	fix_32_32_t T3 = utime2fix (t3);
	fix_32_32_t T4 = utime2fix (t4);

	//Originate Timestamp     T1   time request sent by client
  //Receive Timestamp       T2   time request received by server
  //Transmit Timestamp      T3   time reply sent by server
  //Destination Timestamp   T4   time reply received by client

	//The roundtrip delay d and local clock offset t are defined as
	//d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.

	//fix_32_32_t roundtrip_delay = (T4 - T1) - (T3 - T2); //TODO set config.tuio.offset with this value
	fix_s31_32_t clock_offset = (fix_s31_32_t)(T2 - T1) - (fix_s31_32_t)(T4 - T3);

	if (t0 == 0.0ULLK)
		t0 = T3;
	else
		t0 += clock_offset;
}

void
sntp_timestamp_refresh (timestamp64u_t *now)
{
	fix_32_32_t uptime = systick_uptime () * 0.0001ULLK; // that many 100us
	fix_32_32_t _now = t0 + uptime;
	*now = ufix2time (_now);
}
