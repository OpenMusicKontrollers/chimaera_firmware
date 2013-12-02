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

#include <chimaera.h>
#include <sensors.h>

#if SENSOR_N == 16
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 0 };
#elif SENSOR_N == 32
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA0, PA2, PA3, PA5, PA6, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 1, 0 };
#elif SENSOR_N == 48
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA0, PA2, PA3, PA5, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 2, 1, 0};
#elif SENSOR_N == 64
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA0, PA3, PA6, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 3, 1, 2, 0 };
#elif SENSOR_N == 80
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA0, PA3, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 4, 2, 3, 1, 0};
#elif SENSOR_N == 96
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA3, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 5, 2, 4, 1, 3, 0 };
#elif SENSOR_N == 112
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PA3, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 6, 3, 5, 2, 4, 1, 0 };
#elif SENSOR_N == 128
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 7, 3, 6, 2, 5, 1, 4, 0 };
#elif SENSOR_N == 144
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {PB0};
uint8_t adc_order [ADC_LENGTH] = { 8, 4, 7, 3, 6, 2, 5, 1, 0 };
#elif SENSOR_N == 160
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1, PB0}; // analog input pins read out by the ADC3
uint8_t adc_unused [] = {};
uint8_t adc_order [ADC_LENGTH] = { 9, 5, 8, 4, 7, 3, 6, 2, 1, 0};
#endif
