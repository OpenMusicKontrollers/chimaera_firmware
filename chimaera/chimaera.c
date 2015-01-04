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

#include <chimaera.h>

uint_fast8_t buf_o_ptr = 0;
const uint_fast8_t buf_i_ptr = 0;

// the buffers should be aligned to 32bit, as most we write to it is a multiple of 32bit(OSC, SNTP, DHCP, ARP, etc.)
uint8_t buf_o [2][CHIMAERA_BUFSIZE] __attribute__((aligned(4))); // general purpose output buffer
uint8_t buf_i [1][CHIMAERA_BUFSIZE] __attribute__((aligned(4))); // general purpose input buffer;
