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

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>

#include <oscquery.h>
#include <wiz.h>
#include <config.h>

void DEBUG(const char *fmt, ...);

#define debug_int32(i) DEBUG("i", i)
#define debug_float(f) DEBUG("f", f)
#define debug_str(s) DEBUG("s", s)
#define debug_blob(f) DEBUG("b", l, b)

#define debug_int64(i) DEBUG("h", h)
#define debug_double(d) DEBUG("d", d)
#define debug_timestamp(t) DEBUG("t", t)

extern const OSC_Query_Item debug_tree [1];

#endif // _DEBUG_H_
