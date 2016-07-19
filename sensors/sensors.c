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

#include <stdio.h>

#include <sensors.h>
#include <config.h>
#include <chimutil.h>
#include <cmc.h>

#if SENSOR_N == 16
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 0 };
#elif SENSOR_N == 32
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA0, PA2, PA3, PA5, PA6, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 1, 0 };
#elif SENSOR_N == 48
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA0, PA2, PA3, PA5, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 2, 1, 0};
#elif SENSOR_N == 64
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA0, PA3, PA6, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 3, 1, 2, 0 };
#elif SENSOR_N == 80
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA0, PA3, PA6, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 4, 2, 3, 1, 0};
#elif SENSOR_N == 96
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA3, PA7, PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 5, 2, 4, 1, 3, 0 };
#elif SENSOR_N == 112
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PA3, PA7, PB0};
uint8_t adc_order [ADC_LENGTH] = { 6, 3, 5, 2, 4, 1, 0 };
#elif SENSOR_N == 128
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PB0, PB1};
uint8_t adc_order [ADC_LENGTH] = { 7, 3, 6, 2, 5, 1, 4, 0 };
#elif SENSOR_N == 144
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB0}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {PB1};
uint8_t adc_order [ADC_LENGTH] = { 8, 4, 7, 3, 6, 2, 5, 1, 0 };
#elif SENSOR_N == 160
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB1, PB0}; // analog input pins read out by the ADC3
uint8_t adc_unused [ADC_UNUSED_LENGTH] = {};
uint8_t adc_order [ADC_LENGTH] = { 9, 5, 8, 4, 7, 3, 6, 2, 1, 0};
#endif

/*
 * Config
 */

static const OSC_Query_Value interpolation_mode_args_values [] = {
	[INTERPOLATION_NONE]			= { .s = "none" },
	[INTERPOLATION_QUADRATIC]	= { .s = "quadratic" },
	[INTERPOLATION_CATMULL]		= { .s = "catmullrom" },
	[INTERPOLATION_LAGRANGE]	= { .s = "lagrange" },
};

static uint_fast8_t
_sensors_number(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	size = CONFIG_SUCCESS("isi", uuid, path, SENSOR_N);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_rate(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, config.sensors.rate);
	else
	{
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		config.sensors.rate = i;

		if(config.sensors.rate)
		{
			timer_pause(adc_timer);
			adc_timer_reconfigure();
			timer_resume(adc_timer);
		}
		else
			timer_pause(adc_timer);

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_movingaverage(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, 1U << config.sensors.movingaverage_bitshift);
	else
	{
		int32_t i;
		buf_ptr = osc_get_int32(buf_ptr, &i);
		switch(i)
		{
			case 1:
				config.sensors.movingaverage_bitshift = 0;
				size = CONFIG_SUCCESS("is", uuid, path);
				break;
			case 2:
				config.sensors.movingaverage_bitshift = 1;
				size = CONFIG_SUCCESS("is", uuid, path);
				break;
			case 4:
				config.sensors.movingaverage_bitshift = 2;
				size = CONFIG_SUCCESS("is", uuid, path);
				break;
			case 8:
				config.sensors.movingaverage_bitshift = 3;
				size = CONFIG_SUCCESS("is", uuid, path);
				break;
			default:
				size = CONFIG_FAIL("iss", uuid, path, "valid sample windows are 1, 2, 4 and 8");
		}
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_interpolation(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	uint8_t *interpolation = &config.sensors.interpolation_mode;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, interpolation_mode_args_values[*interpolation]);
	else
	{
		uint_fast8_t i;
		const char *s;
		buf_ptr = osc_get_string(buf_ptr, &s);
		for(i=0; i<sizeof(interpolation_mode_args_values)/sizeof(OSC_Query_Value); i++)
			if(!strcmp(s, interpolation_mode_args_values[i].s))
			{
				*interpolation = i;
				break;
			}
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_velocity_stiffness(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_uint8(path, fmt, argc, buf, &config.sensors.velocity_stiffness);
	cmc_velocity_stiffness_update(config.sensors.velocity_stiffness);
	return res;
}

static uint_fast8_t
_group_reset(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	cmc_group_reset();

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	return 1;
}

#define SENSORS_GID(PATH) \
({ \
	uint16_t _gid = 0; \
 	sscanf(PATH, "/sensors/group/attributes/%hu/", &_gid); \
 	(&cmc_groups[_gid]); \
})

static uint_fast8_t
_group_min(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	CMC_Group *grp = SENSORS_GID(path);

	uint_fast8_t ret = config_check_float(path, fmt, argc, buf, &grp->x0);
	if(grp->m != CMC_NOSCALE)
		grp->m = 1.f / (grp->x1 - grp->x0);

	return ret;
}

static uint_fast8_t
_group_max(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	CMC_Group *grp = SENSORS_GID(path);

	uint_fast8_t ret = config_check_float(path, fmt, argc, buf, &grp->x1);
	if(grp->m != CMC_NOSCALE)
		grp->m = 1.f / (grp->x1 - grp->x0);

	return ret;
}

static uint_fast8_t
_group_north(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	CMC_Group *grp = SENSORS_GID(path);

	uint8_t booly = grp->pid & CMC_NORTH ? 1 : 0;
	uint_fast8_t ret = config_check_bool(path, fmt, argc, buf, &booly);
	if(booly)
		grp->pid |= CMC_NORTH;
	else
		grp->pid &= ~CMC_NORTH;

	if(argc > 1)
		cmc_group_update(); // there may be new groups, we need to update

	return ret;
}

static uint_fast8_t
_group_south(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	CMC_Group *grp = SENSORS_GID(path);

	uint8_t booly = grp->pid & CMC_SOUTH ? 1 : 0;
	uint_fast8_t ret = config_check_bool(path, fmt, argc, buf, &booly);
	if(booly)
		grp->pid |= CMC_SOUTH;
	else
		grp->pid &= ~CMC_SOUTH;

	if(argc > 1)
		cmc_group_update(); // there may be new groups, we need to update

	return ret;
}

static uint_fast8_t
_group_scale(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	CMC_Group *grp = SENSORS_GID(path);

	uint8_t booly = grp->m == CMC_NOSCALE ? 0 : 1;
	uint_fast8_t ret = config_check_bool(path, fmt, argc, buf, &booly);
	if(booly)
		grp->m = 1.f / (grp->x1 - grp->x0);
	else
		grp->m = CMC_NOSCALE;

	return ret;
}

static uint_fast8_t
_group_number(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	(void)fmt;
	(void)argc;
	osc_data_t *buf_ptr = buf;
	uint16_t size;
	int32_t uuid;

	buf_ptr = osc_get_int32(buf_ptr, &uuid);

	size = CONFIG_SUCCESS("isi", uuid, path, cmc_groups_n);
	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

static const OSC_Query_Argument group_number_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Number", OSC_QUERY_MODE_R, 0, GROUP_MAX, 1)
};

static const OSC_Query_Argument min_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Min", OSC_QUERY_MODE_RW, 0.f, 1.f, 0.f)
};

static const OSC_Query_Argument max_args [] = {
	OSC_QUERY_ARGUMENT_FLOAT("Max", OSC_QUERY_MODE_RW, 0.f, 1.f, 0.f)
};

static const OSC_Query_Argument north_args [] = {
	OSC_QUERY_ARGUMENT_BOOL("North?", OSC_QUERY_MODE_RW)
};

static const OSC_Query_Argument south_args [] = {
	OSC_QUERY_ARGUMENT_BOOL("South?", OSC_QUERY_MODE_RW)
};

static const OSC_Query_Argument scale_args [] = {
	OSC_QUERY_ARGUMENT_BOOL("Scale?", OSC_QUERY_MODE_RW)
};

static const OSC_Query_Item group_attributes_tree [] = {
	OSC_QUERY_ITEM_METHOD("min", "Minimum", _group_min, min_args),
	OSC_QUERY_ITEM_METHOD("max", "Maximum", _group_max, max_args),
	OSC_QUERY_ITEM_METHOD("north", "Respond to north polarized fields", _group_north, north_args),
	OSC_QUERY_ITEM_METHOD("south", "Respond to south polarized fields", _group_south, south_args),
	OSC_QUERY_ITEM_METHOD("scale", "Scale output", _group_scale, scale_args),
};

static const OSC_Query_Item group_attribute_array [] = {
	OSC_QUERY_ITEM_NODE("%i/", "Group", group_attributes_tree)
};

static const OSC_Query_Item group_tree [] = {
	OSC_QUERY_ITEM_METHOD("reset", "Reset all groups", _group_reset, NULL),
	OSC_QUERY_ITEM_METHOD("number", "Number", _group_number, group_number_args),
	OSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", group_attribute_array, GROUP_MAX)
};

static const OSC_Query_Argument sensors_number_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Number", OSC_QUERY_MODE_R, 0, 160, 16)
};

static const OSC_Query_Argument sensors_rate_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Hz", OSC_QUERY_MODE_RW, 0, 10000, 100)
};

static const OSC_Query_Value sensors_movingaverage_windows_values [] = {
	{ .i = 1 },
	{ .i = 2 },
	{ .i = 4 },
	{ .i = 8 }
};

static const OSC_Query_Argument sensors_movingaverage_args [] = {
	OSC_QUERY_ARGUMENT_INT32_VALUES("Sample window", OSC_QUERY_MODE_RW, sensors_movingaverage_windows_values)
};

static const OSC_Query_Argument sensors_interpolation_args [] = {
	OSC_QUERY_ARGUMENT_STRING_VALUES("Order", OSC_QUERY_MODE_RW, interpolation_mode_args_values)
};

static const OSC_Query_Argument sensors_velocity_stiffness_args [] = {
	OSC_QUERY_ARGUMENT_INT32("Stiffness", OSC_QUERY_MODE_RW, 1, 128, 1)
};

const OSC_Query_Item sensors_tree [] = {
	OSC_QUERY_ITEM_NODE("group/", "Group", group_tree),

	OSC_QUERY_ITEM_METHOD("movingaverage", "Movingaverager", _sensors_movingaverage, sensors_movingaverage_args),
	OSC_QUERY_ITEM_METHOD("interpolation", "Interpolation", _sensors_interpolation, sensors_interpolation_args),
	OSC_QUERY_ITEM_METHOD("velocity_stiffness", "Stiffness of velocity filter", _sensors_velocity_stiffness, sensors_velocity_stiffness_args),
	OSC_QUERY_ITEM_METHOD("rate", "Update rate", _sensors_rate, sensors_rate_args),

	OSC_QUERY_ITEM_METHOD("number", "Sensor number", _sensors_number, sensors_number_args),
};
