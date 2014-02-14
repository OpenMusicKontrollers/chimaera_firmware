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

// globals
fix_32_32_t roundtrip_delay;
fix_s31_32_t clock_offset;

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

	volatile timestamp64_t t1, t2, t3;

	t1.osc.sec = ntohl (answer->originate_timestamp.ntp.sec);
	t1.osc.frac = ntohl (answer->originate_timestamp.ntp.frac);

	t2.osc.sec = ntohl (answer->receive_timestamp.ntp.sec);
	t2.osc.frac = ntohl (answer->receive_timestamp.ntp.frac);

	t3.osc.sec = ntohl (answer->transmit_timestamp.ntp.sec);
	t3.osc.frac = ntohl (answer->transmit_timestamp.ntp.frac);

	//Originate Timestamp     T1   time request sent by client
  //Receive Timestamp       T2   time request received by server
  //Transmit Timestamp      T3   time reply sent by server
  //Destination Timestamp   T4   time reply received by client

	//The roundtrip delay d and local clock offset t are defined as
	//d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.

	roundtrip_delay = (t4 - t1.fix) - (t3.fix - t2.fix);
	clock_offset = 0.5LLK * ((fix_s31_32_t)(t2.fix - t1.fix) - (fix_s31_32_t)(t4 - t3.fix));

	if (t0 == 0ULLK) // first sync
		t0 = t3.fix + 0.5ULLK*roundtrip_delay - t4;
	else
		t0 += clock_offset;
}

void __CCM_TEXT__
sntp_timestamp_refresh (uint32_t tick, nOSC_Timestamp *now, nOSC_Timestamp *offset)
{
	*now = t0 + tick*SNTP_SYSTICK_DURATION;

	if (offset)
	{
		if (config.output.offset > 0ULLK)
			*offset = *now + config.output.offset;
		else
			*offset = nOSC_IMMEDIATE;
	}
}

/*
 * Config
 */

static uint_fast8_t
_sntp_enabled (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_socket_enabled (&config.sntp.socket, path, fmt, argc, args);
}

static uint_fast8_t
_sntp_address (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_address (&config.sntp.socket, path, fmt, argc, args);
}

static uint_fast8_t
_sntp_tau (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_check_uint8 (path, fmt, argc, args, &config.sntp.tau);
}

static uint_fast8_t
_sntp_offset (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	float offset = clock_offset;
	size = CONFIG_SUCCESS ("isf", uuid, path, offset);

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_sntp_roundtrip (const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	float trip = roundtrip_delay;
	size = CONFIG_SUCCESS ("isf", uuid, path, trip);

	udp_send (config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

/*
 * Query
 */

static const nOSC_Query_Argument sntp_enabled_args [] = {
	nOSC_QUERY_ARGUMENT_BOOL("bool", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument sntp_address_args [] = {
	nOSC_QUERY_ARGUMENT_STRING("host:port", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Argument sntp_tau_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("seconds", nOSC_QUERY_MODE_RW, 1, 60)
};

static const nOSC_Query_Argument sntp_offset_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("seconds", nOSC_QUERY_MODE_R, -INFINITY, INFINITY)
};

static const nOSC_Query_Argument sntp_roundtrip_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("seconds", nOSC_QUERY_MODE_R, 0.f, INFINITY)
};

const nOSC_Query_Item sntp_tree [] = {
	// read-write
	nOSC_QUERY_ITEM_METHOD("enabled", "enable/disable", _sntp_enabled, sntp_enabled_args),
	nOSC_QUERY_ITEM_METHOD("address", "remote address", _sntp_address, sntp_address_args),
	nOSC_QUERY_ITEM_METHOD("tau", "update period", _sntp_tau, sntp_tau_args),

	// read-only
	nOSC_QUERY_ITEM_METHOD("offset", "sync offset", _sntp_offset, sntp_offset_args),
	nOSC_QUERY_ITEM_METHOD("roundtrip", "sync roundtrip delay", _sntp_roundtrip, sntp_roundtrip_args)
};
