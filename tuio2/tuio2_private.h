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

#ifndef TUIO2_PRIVATE_H
#define TUIO2_PRIVATE_H

#include <tuio2.h>
#include <nosc.h>

typedef struct _Tuio2_Tok Tuio2_Tok;
typedef struct _Tuio2 Tuio2;

struct _Tuio2_Tok {
	char *path;
	nOSC_Arg args [4 + 1];
};

struct _Tuio2 {
	nOSC_Message *bndl [BLOB_MAX + 3];
	nOSC_Message *frm;

	Tuio2_Tok tok [BLOB_MAX];
	nOSC_Message alv [BLOB_MAX + 1];
};

#endif /* TUIO2_PRIVATE_H */
