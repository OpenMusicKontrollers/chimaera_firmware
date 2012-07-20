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

#include <chimaera.h>
#include <config.h>

uint8_t buf[1024]; // general purpose buffer used mainly for nOSC serialization
uint8_t buf_in[1024]; // general purpose buffer used mainly for nOSC serialization

uint32_t debug_counter = 0;

void
debug_str (const char *str)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf, "/debug", "is", debug_counter++, str);
	dma_udp_send (config.config.sock, buf, size);
}

void
debug_int32 (int32_t i)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf, "/debug", "ii", debug_counter++, i);
	dma_udp_send (config.config.sock, buf, size);
}

void
debug_float (float f)
{
	uint16_t size;
	size = nosc_message_vararg_serialize (buf, "/debug", "if", debug_counter++, f);
	dma_udp_send (config.config.sock, buf, size);
}
