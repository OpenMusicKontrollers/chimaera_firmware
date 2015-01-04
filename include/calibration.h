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

#ifndef _CALIBRATION_H_
#define _CALIBRATION_H_

typedef struct _Calibration Calibration;

struct _Calibration {
	uint16_t qui [SENSOR_N]; // quiscent value
	uint16_t thresh [SENSOR_N]; // threshold value
	float U [SENSOR_N]; // AS^(-1) * Sc^(-1)
	float W; // Bmin * Sc^(-1)
	float C [3];
};

// globals
extern Calibration range;
extern uint_fast8_t zeroing;
extern uint_fast8_t calibrating;
extern float curve [0x800]; // lookup table for distance-magnetic-flux relationship
extern const OSC_Query_Item calibration_tree [14];

uint_fast8_t range_load(uint_fast8_t pos);
uint_fast8_t range_reset(void);
uint_fast8_t range_save(uint_fast8_t pos);

void range_curve_update(void);

void range_calibrate(int16_t *raw12, int16_t *raw3, uint8_t *order12, uint8_t *order3, int16_t *sum, int16_t *rela);
void range_init(void);

#endif // _CALIBRATION_H_
