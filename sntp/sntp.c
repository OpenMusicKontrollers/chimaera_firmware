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

#include <string.h>

#include <libmaple/systick.h>

#include "sntp_private.h"

#include <chimutil.h>
#include <config.h>

static nOSC_Timestamp t0 = 0ULLK;

uint16_t __CCM_TEXT__
sntp_request (uint8_t *buf, nOSC_Timestamp t3)
{
	uint16_t len = sizeof (sntp_t);

	memset (buf, 0, len);

	sntp_t *request = (sntp_t *) buf;

	timestamp64_t T3;
	T3.fix = t3;

	request->li_vn_mode = SNTP_LEAP_INDICATOR_NO_WARNING | SNTP_VERSION_4 | SNTP_MODE_CLIENT;
	request->transmit_timestamp.ntp.sec = htonl (T3.osc.sec);
	request->transmit_timestamp.ntp.frac = htonl (T3.osc.frac);
	
	return len;
}

void __CCM_TEXT__
sntp_dispatch (uint8_t *buf, nOSC_Timestamp t4)
{
	sntp_t *answer = (sntp_t *)buf;

	// check whether its a SNTP version 4 server answer
	if ( (answer->li_vn_mode & 0x3f) != (SNTP_VERSION_4 | SNTP_MODE_SERVER) )
		return;

	volatile timestamp64_t t1, t2, t3, _t4;

	t1.osc.sec = ntohl (answer->originate_timestamp.ntp.sec);
	t1.osc.frac = ntohl (answer->originate_timestamp.ntp.frac);

	t2.osc.sec = ntohl (answer->receive_timestamp.ntp.sec);
	t2.osc.frac = ntohl (answer->receive_timestamp.ntp.frac);

	t3.osc.sec = ntohl (answer->transmit_timestamp.ntp.sec);
	t3.osc.frac = ntohl (answer->transmit_timestamp.ntp.frac);

	_t4.fix = t4;

	//Originate Timestamp     T1   time request sent by client
  //Receive Timestamp       T2   time request received by server
  //Transmit Timestamp      T3   time reply sent by server
  //Destination Timestamp   T4   time reply received by client

	//The roundtrip delay d and local clock offset t are defined as
	//d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.

	fix_s31_32_t clock_offset;
	//fix_32_32_t roundtrip_delay = (T4 - T1) - (T3 - T2); //TODO set config.output.offset with this value by default?
	clock_offset = 0.5LLK * ((fix_s31_32_t)(t2.fix - t1.fix) - (fix_s31_32_t)(_t4.fix - t3.fix));

	if (t0 == 0ULLK)
		t0 = t3.fix;
	else
		t0 += clock_offset;
}

void __CCM_TEXT__
sntp_timestamp_refresh (nOSC_Timestamp *now, nOSC_Timestamp *offset)
{
	*now = t0 + systick_uptime() * 0.0001ULLK; // that many 100us

	if (offset)
	{
		if (config.output.offset > 0ULLK)
			*offset = *now + config.output.offset;
		else
			*offset = nOSC_IMMEDIATE;
	}
}
