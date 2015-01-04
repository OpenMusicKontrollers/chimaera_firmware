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

#ifndef _SENSORS_H_
#define _SENSORS_H_

#include <chimaera.h>
#include <oscquery.h>

extern uint8_t adc1_sequence [ADC_DUAL_LENGTH]; // analog input pins read out by the ADC1
extern uint8_t adc2_sequence [ADC_DUAL_LENGTH]; // analog input pins read out by the ADC2
extern uint8_t adc3_sequence [ADC_SING_LENGTH]; // analog input pins read out by the ADC3
extern uint8_t adc_unused [ADC_UNUSED_LENGTH];
extern uint8_t adc_order [ADC_LENGTH];
extern const OSC_Query_Item sensors_tree [6];

enum Interpolation_Mode {
	INTERPOLATION_NONE,
	INTERPOLATION_QUADRATIC,
	INTERPOLATION_CATMULL,
	INTERPOLATION_LAGRANGE
};

#endif // _SENSORS_H_
