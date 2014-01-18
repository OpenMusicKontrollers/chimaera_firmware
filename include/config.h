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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

#include <nosc.h>
#include <chimaera.h>

#define SRC_PORT 0
#define DST_PORT 1
#define NAME_LENGTH 16

typedef uint_fast8_t (*Socket_Enable_Cb) (uint8_t flag);
typedef struct _Firmware_Version Firmware_Version;
typedef struct _Socket_Config Socket_Config;
typedef struct _Config Config;

struct _Firmware_Version {
	uint8_t revision;	// board layout revision
	uint8_t major;		// major version
	uint8_t minor;		// minor version
	uint8_t patch;		// patch level
};

struct _Socket_Config {
	uint8_t sock;
	uint8_t enabled;
	uint16_t port[2]; // SRC_PORT, DST_PORT
	uint8_t ip[4];
};

enum {
	SOCK_ARP		= 0,
	SOCK_OUTPUT	= 1,
	SOCK_CONFIG = 2,
	SOCK_SNTP		= 3,
	SOCK_DEBUG	= 4,
	SOCK_MDNS		= 5,
	SOCK_DHCPC	= 6,
	SOCK_RTP		= 7
};

struct _Config {
	/*
	 * read-only
	 */
 	Firmware_Version version;

	/*
	 * read-write
	 */
	char name[NAME_LENGTH];

	struct _comm {
		uint8_t custom_mac; // locally administered MAC flag
		uint8_t mac [6];
		uint8_t ip [4];
		uint8_t gateway [4];
		uint8_t subnet [4];
	} comm;

	struct _dump {
		uint8_t enabled;
	} dump;

	struct _tuio2 {
		uint8_t enabled;
		uint8_t long_header;
	} tuio2;

	struct _tuio1 {
		uint8_t enabled;
		uint8_t custom_profile;
	} tuio1;

	struct _scsynth {
		uint8_t enabled;
	} scsynth;

	struct _oscmidi {
		uint8_t enabled;
		uint8_t effect;
		float offset;
		float range;
		float mul;
	} oscmidi;

	struct _dummy {
		uint8_t enabled;
	} dummy;

	struct _rtpmidi {
		uint8_t enabled;
	} rtpmidi;

	struct _output {
		Socket_Config socket;
		nOSC_Timestamp offset;
	} output;

	struct _config {
		uint16_t rate;
		Socket_Config socket;
	} config;

	struct _sntp {
		uint8_t tau;
		Socket_Config socket;
	} sntp;

	struct _debug {
		Socket_Config socket;
	} debug;

	struct _ipv4ll {
		uint8_t enabled;
	} ipv4ll;

	struct _mdns {
		Socket_Config socket;
	} mdns;

	struct _dhcpc {
		Socket_Config socket;
	} dhcpc;

	struct _movingaverage {
		uint8_t enabled;
		uint8_t bitshift;
	} movingaverage;

	struct _interpolation {
		uint8_t order;
	} interpolation;

	uint16_t rate; // the maximal update rate the chimaera should run at
};

extern Config config;
extern const nOSC_Method config_serv [];

uint_fast8_t version_match ();

uint_fast8_t config_load ();
uint_fast8_t config_save ();

uint_fast8_t groups_load ();
uint_fast8_t groups_save ();

#endif
