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

#ifndef _CALIBRATION_PRIVATE_H_
#define _CALIBRATION_PRIVATE_H_

#include <calibration.h>

typedef struct _Calibration_Array Calibration_Array;
typedef struct _Calibration_Point Calibration_Point;

struct _Calibration_Array {
	uint16_t arr [2][SENSOR_N];
};

struct _Calibration_Point {
	uint_fast8_t i;
	uint_fast8_t state;
	float y1, y2, y3;
	float B0, B1, B2, B3;
};

#endif // _CALIBRATION_PRIVATE_H_
