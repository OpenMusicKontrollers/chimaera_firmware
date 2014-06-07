/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

// globals
fix_s31_32_t D0, D1, DD0 = 0.0002LLK, DD1 = 0.0002LLK;
fix_s31_32_t O0, O1, OO0, OO1;

#define JAN_1970 2208988800UL

static OSC_Timetag t0 = 0ULLK;

void
sntp_reset()
{
	t0 = 0.0ULLK;
	D0 = D1 = DD0 = DD1 = 0.0LLK;
	O0 = O1 = OO0 = OO1 = 0.0LLK;
}

uint16_t __CCM_TEXT__
sntp_request(uint8_t *buf, OSC_Timetag t3)
{
	uint16_t len = sizeof(sntp_t);

	memset(buf, 0, len);

	sntp_t *request =(sntp_t *) buf;

	timestamp64_t T3;
	T3.fix = t3;

	request->li_vn_mode = SNTP_LEAP_INDICATOR_NO_WARNING | SNTP_VERSION_4 | SNTP_MODE_CLIENT;
	request->transmit_timestamp.ntp.sec = htonl(T3.osc.sec);
	request->transmit_timestamp.ntp.frac = htonl(T3.osc.frac);
	
	return len;
}

void //__CCM_TEXT__
sntp_dispatch(uint8_t *buf, OSC_Timetag t4)
{
	sntp_t *answer =(sntp_t *)buf;

	// check whether its a SNTP version 4 server answer
	if( (answer->li_vn_mode & 0x3f) !=(SNTP_VERSION_4 | SNTP_MODE_SERVER) )
		return;

	//volatile timestamp64_t t1, t2, t3;
	timestamp64_t t1, t2, t3;

	t1.osc.sec = ntohl(answer->originate_timestamp.ntp.sec);
	t1.osc.frac = ntohl(answer->originate_timestamp.ntp.frac);

	t2.osc.sec = ntohl(answer->receive_timestamp.ntp.sec);
	t2.osc.frac = ntohl(answer->receive_timestamp.ntp.frac);

	t3.osc.sec = ntohl(answer->transmit_timestamp.ntp.sec);
	t3.osc.frac = ntohl(answer->transmit_timestamp.ntp.frac);

	t1.fix -= JAN_1970;
	t2.fix -= JAN_1970;
	t3.fix -= JAN_1970;
	t4 -= JAN_1970;

	//Originate Timestamp     T1   time request sent by client
  //Receive Timestamp       T2   time request received by server
  //Transmit Timestamp      T3   time reply sent by server
  //Destination Timestamp   T4   time reply received by client

	//The delay d and local clock offset t are defined as
	//d = ( (T4 - T1) - (T3 - T2) ) / 2     t = ( (T2 - T1) + (T3 - T4) ) / 2.

	//const fix_s31_32_t Ds = 0.015625LLK; // 1/64
	//const fix_s31_32_t Os = 0.015625LLK; // 1/64
	const fix_s31_32_t Ds = 0.25LLK; // 1/4
	const fix_s31_32_t Os = 0.25LLK; // 1/4

	if(t0 == 0.0ULLK) // first sync
		t0 = t3.fix + DD1 - t4;
	else
	{
		D1 = (fix_s31_32_t)t4 - (fix_s31_32_t)t1.fix;
		D1 -= (fix_s31_32_t)t3.fix - (fix_s31_32_t)t2.fix;
		D1 /= 2;

		if(D0 == 0.0LLK)
			DD0 = D0 = D1;

		DD1 = Ds * (D0 + D1) / 2 + DD0 * (1.0LLK - Ds);

		O1 = (fix_s31_32_t)t2.fix - (fix_s31_32_t)t1.fix;
		O1 += (fix_s31_32_t)t3.fix - (fix_s31_32_t)t4;
		O1 /= 2;
		OO1 = Os * (O0 + O1) / 2 + OO0 * (1.0LLK - Os);

		t0 += OO1;
	}

	D0 = D1;
	DD0 = DD1;
	O0 = O1;
	OO0 = OO1;

#if 1
	DEBUG("stt", "sNTPv4", OO1, DD1);
#endif
}

void __CCM_TEXT__
sntp_timestamp_refresh(uint32_t tick, OSC_Timetag *now, OSC_Timetag *offset)
{
	*now = t0 + tick*SNTP_SYSTICK_DURATION;

	if(offset)
	{
		if( (config.output.offset > 0ULLK) && (t0 != 0ULLK) )
			*offset = *now + config.output.offset;
		else
			*offset = OSC_IMMEDIATE;
	}
}

/*
 * Config
 */

static uint_fast8_t
_sntp_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_socket_enabled(&config.sntp.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_sntp_address(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_address(&config.sntp.socket, path, fmt, argc, buf);
}

static uint_fast8_t
_sntp_tau(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_uint8(path, fmt, argc, buf, &config.sntp.tau);
}

static uint_fast8_t
_sntp_offset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	float offset = OO1;
	size = CONFIG_SUCCESS("isf", uuid, path, offset);

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sntp_delay(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	float trip = DD1;
	size = CONFIG_SUCCESS("isf", uuid, path, trip);

	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

static const OSC_Query_Argument sntp_tau_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Seconds", OSC_QUERY_MODE_RW, 1, 60, 1)
};

static const OSC_Query_Argument sntp_offset_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Seconds", OSC_QUERY_MODE_R, -INFINITY, INFINITY, 0.f)
};

static const OSC_Query_Argument sntp_delay_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Seconds", OSC_QUERY_MODE_R, 0.f, INFINITY, 0.f)
};

const OSC_Query_Item sntp_tree [] = {
	// read-write
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _sntp_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("address", "Single remote address", _sntp_address, config_address_args),
	OSC_QUERY_ITEM_METHOD("tau", "Update period", _sntp_tau, sntp_tau_args),

	// read-only
	OSC_QUERY_ITEM_METHOD("offset", "Sync offset", _sntp_offset, sntp_offset_args),
	OSC_QUERY_ITEM_METHOD("delay", "Sync delay", _sntp_delay, sntp_delay_args)
};
