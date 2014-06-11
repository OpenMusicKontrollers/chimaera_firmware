/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
uint_fast8_t range_reset();
uint_fast8_t range_save(uint_fast8_t pos);

void range_curve_update();

void range_calibrate(int16_t *raw12, int16_t *raw3, uint8_t *order12, uint8_t *order3, int16_t *sum, int16_t *rela);
void range_init();

#endif // _CALIBRATION_H_
