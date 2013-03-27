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

/*
 * wirish headers
 */
#include <wirish/wirish.h>

HardwareSPI spi(2);

__attribute__ ((constructor)) void
premain ()
{
  init (); // board init
}

extern "C" void
cpp_setup ()
{
	// we don't need USB communication, so we disable it
	SerialUSB.end ();

	// set up SPI for usage with wiz820io
  spi.begin (SPI_18MHZ, MSBFIRST, 0);
  //spi.begin (SPI_9MHZ, MSBFIRST, 0);
}
