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

#ifndef _PTP_PRIVATE_H_
#define _PTP_PRIVATE_H_

#include <stdint.h>

#include <ptp.h>
#include <armfix.h>

typedef struct _PTP_Timestamp PTP_Timestamp;
typedef struct _PTP_Request PTP_Request;
#define PTP_Sync PTP_Request
#define PTP_Follow_Up PTP_Request
typedef struct _PTP_Response PTP_Response;
typedef struct _PTP_Announce PTP_Announce;

struct _PTP_Timestamp {
	uint16_t epoch;
	uint32_t sec;
	uint32_t nsec;
} __attribute((packed));

struct _PTP_Request {
	uint8_t message_id;
	uint8_t version;
	uint16_t message_length;
	uint8_t subdomain_number;
	uint8_t UNUSED_2;
	uint16_t flags;
	uint64_t correction; // 6.2 ns
	uint32_t UNUSED_3;
	uint64_t clock_identity;
	uint16_t source_port_id;
	uint16_t sequence_id;
	uint8_t control;
	uint8_t log_message_interval;
	PTP_Timestamp timestamp;
} __attribute((packed));

struct _PTP_Response {
	PTP_Request req;
	uint64_t requesting_clock_identity;
	uint16_t requesting_source_port_id;
} __attribute((packed));

struct _PTP_Announce {
	PTP_Request req;
	uint16_t origin_current_utc_offset;
	uint8_t UNUSED_4;
	uint8_t priotity_1;
	uint8_t grandmaster_clock_class;
	uint8_t grandmaster_clock_accuracy;
	uint16_t grandmaster_clock_variance;
	uint8_t priority_2;
	uint64_t grandmaster_clock_identity;
	uint16_t local_steps_removed;
	uint8_t time_source;
} __attribute((packed));

typedef enum _PTP_Message_ID {
	// PTP event port (319) 
	PTP_MESSAGE_ID_SYNC										= 0x00,
	PTP_MESSAGE_ID_DELAY_REQ							= 0x01,
	PTP_MESSAGE_ID_PDELAY_REQ							= 0x02,
	PTP_MESSAGE_ID_PDELAY_RESP						= 0x03,

	// PTP general port (320)
	PTP_MESSAGE_ID_FOLLOW_UP							= 0x08,
	PTP_MESSAGE_ID_DELAY_RESP							= 0x09,
	PTP_MESSAGE_ID_PDELAY_RESP_FOLLOW_UP	= 0x0a,
	PTP_MESSAGE_ID_ANNOUNCE								= 0x0b,
	PTP_MESSAGE_ID_SIGNALING							= 0x0c,
	PTP_MESSAGE_ID_MANAGEMENT							= 0x0d
} PTP_Message_ID;

#define PTP_VERSION_2											0x02

#define PTP_FLAGS_SECURITY								(1U << 15)
#define PTP_FLAGS_PROFILE_SPECIFIC_2			(1U << 14)
#define PTP_FLAGS_PROFILE_SPECIFIC_1			(1U << 13)
#define PTP_FLAGS_UNICAST									(1U << 10)
#define PTP_FLAGS_TWO_STEP								(1U << 9)
#define PTP_FLAGS_ALTERNATE_MASTER				(1U << 8)
#define PTP_FLAGS_FREQUENCY_TRACEABLE			(1U << 5)
#define PTP_FLAGS_TIME_TRACEABLE					(1U << 4)
#define PTP_FLAGS_TIMESCALE								(1U << 3)
#define PTP_FLAGS_UTC_REASONABLE					(1U << 2)
#define PTP_FLAGS_LI_59										(1U << 1)
#define PTP_FLAGS_LI_61										(1U << 0)

#define PTP_CONTROL_SYNC									0x00
#define PTP_CONTROL_DELAY_REQ							0x01
#define PTP_CONTROL_FOLLOW_UP							0x02
#define PTP_CONTROL_DELAY_RESP						0x03
#define PTP_CONTROL_OTHER									0x05

#endif // _PTP_PRIVATE_H_
