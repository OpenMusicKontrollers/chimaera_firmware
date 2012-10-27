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

#include <armfix.h>

typedef union _uswapy uswapy;
typedef union _sswapy sswapy;

union _uswapy {
	uint64_t timestamp;
	fix_32_32_t fixtime;
};

union _sswapy {
	int64_t timestamp;
	fix_s31_32_t fixtime;
};

fix_32_32_t
utime2fix (uint64_t x)
{
	uswapy s1;
	s1.timestamp = x;
	return s1.fixtime;
}

fix_s31_32_t
stime2fix (int64_t x)
{
	sswapy s1;
	s1.timestamp = x;
	return s1.fixtime;
}

uint64_t
ufix2time (fix_32_32_t x)
{
	uswapy s1;
	s1.fixtime = x;
	return s1.timestamp;
}

int64_t
sfix2time (fix_s31_32_t x)
{
	sswapy s1;
	s1.fixtime = x;
	return s1.timestamp;
}
