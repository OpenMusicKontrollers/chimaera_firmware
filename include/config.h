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

typedef struct _Socket_Config Socket_Config;
typedef struct _Config Config;

struct _Socket_Config {
	uint8_t enabled;
	uint8_t sock;
	uint16_t port;
	uint8_t ip[4];
};

struct _Config {
	/*
	 * read-only
	 */
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

	Socket_Config tuio;
	Socket_Config config;
	Socket_Config sntp;
	Socket_Config dump;
	struct _rtpmidi {
		Socket_Config payload;
		Socket_Config session;
	} rtpmidi;

	struct _cmc {
		uint16_t rate;
		uint16_t diff;
		uint16_t thresh0;
		uint16_t thresh1;
	} cmc;
};

extern Config config;

nOSC_Server *config_methods_add (nOSC_Server *serv, void *data);

#ifdef __cplusplus
}
#endif

#endif
