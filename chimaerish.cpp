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

/*
 * wirish headers
 */
#include <wirish/wirish.h>

extern "C" void
usb_debug_str(const char * str)
{
	SerialUSB.println(str);
}

extern "C" void
usb_debug_int(uint32_t i)
{
	SerialUSB.println(i);
}

__attribute__((constructor)) void
premain(void)
{
  init(); // board init
}

extern "C" void
cpp_setup(void)
{
	// we don't need USB communication, so we disable it
	//SerialUSB.begin();
	SerialUSB.end();
}
