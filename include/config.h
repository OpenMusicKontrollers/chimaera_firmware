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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

#include <nosc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_REPLY_PATH "/reply"
#define SRC_PORT 0
#define DST_PORT 1

typedef struct _Socket_Config Socket_Config;
typedef struct _Config Config;
typedef struct _ADC_Range ADC_Range;

struct _Socket_Config {
	uint8_t sock;
	uint16_t port[2]; // SRC_PORT, DST_PORT
	uint8_t ip[4];
};

struct _Config {
	/*
	 * read-only
	 */
	uint8_t magic;

	struct _version {
		uint8_t major;
		uint8_t minor;
		uint8_t patch_level;
	} version;

	/*
	 * read-write
	 */
	struct _comm {
		uint8_t mac [6];
		uint8_t ip [4];
		uint8_t gateway [4];
		uint8_t subnet [4];
	} comm;

	struct _tuio {
		uint8_t enabled;
		Socket_Config socket;
		uint8_t long_header;
	} tuio;

	struct _config {
		uint16_t rate;
		uint8_t enabled;
		Socket_Config socket;
	} config;

	struct _sntp {
		uint8_t tau;
		uint8_t enabled;
		Socket_Config socket;
	} sntp;

	struct _dump {
		uint8_t enabled;
		Socket_Config socket;
	} dump;

	struct _debug {
		uint8_t enabled;
		Socket_Config socket;
	} debug;

	struct _ping {
		uint16_t rate;
		uint8_t enabled;
		Socket_Config socket;
	} ping;

	struct _cmc {
		uint16_t thresh0; // everything below is considered noise
		uint16_t thresh1; // everything above will trigger an ON event
		uint16_t thresh2; // this is the maximal value reacheable
		uint8_t max_groups; // the maximal number of groups that can be created
		uint8_t max_blobs; // the maximal number of concurrent blobs that can be handled
		uint8_t peak_thresh;
	} cmc;

	uint16_t rate; // the maximal update rate the chimaera should run at
};

struct _ADC_Range {
	uint16_t min;
	uint16_t mean;
	uint16_t max;
};

extern Config config;

uint8_t config_load ();
uint8_t config_save ();

extern ADC_Range range[16][9];

uint8_t range_load ();
uint8_t range_save ();

nOSC_Server *config_methods_add (nOSC_Server *serv);
nOSC_Server *ping_methods_add (nOSC_Server *serv);

#ifdef __cplusplus
}
#endif

#endif
