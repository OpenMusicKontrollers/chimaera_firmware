/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#ifndef _SENSORS_H_
#define _SENSORS_H_

extern uint8_t adc1_sequence [ADC_DUAL_LENGTH]; // analog input pins read out by the ADC1
extern uint8_t adc2_sequence [ADC_DUAL_LENGTH]; // analog input pins read out by the ADC2
extern uint8_t adc3_sequence [ADC_SING_LENGTH]; // analog input pins read out by the ADC3
extern uint8_t adc_unused [ADC_LENGTH - 2*ADC_DUAL_LENGTH - ADC_SING_LENGTH];
extern uint8_t adc_order [ADC_LENGTH];

#endif // _SENSORS_H
