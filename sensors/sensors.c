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

#include <stdio.h>

#include <sensors.h>
#include <config.h>
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

static const nOSC_Query_Value interpolation_mode_args_values [] = {
	[INTERPOLATION_NONE]			= { .s = "none" },
	[INTERPOLATION_LINEAR]		= { .s = "linear" },
	[INTERPOLATION_QUADRATIC]	= { .s = "quadratic" },
	[INTERPOLATION_CUBIC]			= { .s = "cubic" },
	[INTERPOLATION_SPLINE]		= { .s = "spline" },
};

static uint_fast8_t
_sensors_number(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	size = CONFIG_SUCCESS("isi", uuid, path, SENSOR_N);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_rate(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, config.sensors.rate);
	else
	{
		config.sensors.rate = args[1].i;

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
_sensors_movingaverage(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("isi", uuid, path, 1U << config.sensors.movingaverage_bitshift);
	else
		switch(args[1].i)
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

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_sensors_interpolation(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	uint8_t *interpolation = &config.sensors.interpolation_mode;

	if(argc == 1) // query
		size = CONFIG_SUCCESS("iss", uuid, path, interpolation_mode_args_values[*interpolation]);
	else
	{
		uint_fast8_t i;
		for(i=0; i<sizeof(interpolation_mode_args_values)/sizeof(nOSC_Query_Value); i++)
			if(!strcmp(args[1].s, interpolation_mode_args_values[i].s))
			{
				*interpolation = i;
				break;
			}
		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);
}

static uint_fast8_t
_group_clear(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	cmc_group_clear();

	size = CONFIG_SUCCESS("is", uuid, path);
	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_group_attributes(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	uint16_t gid;

	sscanf(path, "/sensors/group/attributes/%hu", &gid);
	CMC_Group *grp = &cmc_groups[gid];

	if(argc == 1) // request group info
	{
		size = CONFIG_SUCCESS("isffiii", uuid, path, grp->x0, grp->x1,
			grp->pid & CMC_NORTH ? 1 : 0,
			grp->pid & CMC_SOUTH ? 1 : 0,
			grp->m == CMC_NOSCALE ? 0 : 1);
	}
	else // set group info
	{
		grp->x0 = args[1].f;
		grp->x1 = args[2].f;
		grp->pid = 0;
		grp->pid |= args[3].i ? CMC_NORTH : 0;
		grp->pid |= args[4].i ? CMC_SOUTH : 0;
		grp->m = args[5].i ? 1.f/(grp->x1 - grp->x0) : CMC_NOSCALE;

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	CONFIG_SEND(size);

	return 1;
}

static uint_fast8_t
_group_number(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	size = CONFIG_SUCCESS("isi", uuid, path, GROUP_MAX);
	CONFIG_SEND(size);

	return 1;
}

/*
 * Query
 */

static const nOSC_Query_Argument group_number_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Number", nOSC_QUERY_MODE_R, GROUP_MAX, GROUP_MAX, 1)
};

static const nOSC_Query_Argument group_attributes_args [] = {
	nOSC_QUERY_ARGUMENT_FLOAT("Min", nOSC_QUERY_MODE_RW, 0.f, 1.f, 0.f),
	nOSC_QUERY_ARGUMENT_FLOAT("Max", nOSC_QUERY_MODE_RW, 0.f, 1.f, 0.f),
	nOSC_QUERY_ARGUMENT_BOOL("North?", nOSC_QUERY_MODE_RW),
	nOSC_QUERY_ARGUMENT_BOOL("South?", nOSC_QUERY_MODE_RW),
	nOSC_QUERY_ARGUMENT_BOOL("Scale?", nOSC_QUERY_MODE_RW)
};

static const nOSC_Query_Item group_attribute_tree [] = {
	nOSC_QUERY_ITEM_METHOD("%i", "Group %i", _group_attributes, group_attributes_args),
};

static const nOSC_Query_Item group_tree [] = {
	nOSC_QUERY_ITEM_METHOD("reset", "Reset all groups", _group_clear, NULL),
	nOSC_QUERY_ITEM_METHOD("number", "Number", _group_number, group_number_args),
	nOSC_QUERY_ITEM_ARRAY("attributes/", "Attributes", group_attribute_tree, GROUP_MAX)
};

static const nOSC_Query_Argument sensors_number_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Number", nOSC_QUERY_MODE_R, SENSOR_N, SENSOR_N, 16)
};

static const nOSC_Query_Argument sensors_rate_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Hz", nOSC_QUERY_MODE_RW, 0, 10000, 100)
};

static const nOSC_Query_Value sensors_movingaverage_windows_values [] = {
	{ .i = 1 },
	{ .i = 2 },
	{ .i = 4 },
	{ .i = 8 }
};

static const nOSC_Query_Argument sensors_movingaverage_args [] = {
	nOSC_QUERY_ARGUMENT_INT32_VALUES("Sample window", nOSC_QUERY_MODE_RW, sensors_movingaverage_windows_values)
};

static const nOSC_Query_Argument sensors_interpolation_args [] = {
	nOSC_QUERY_ARGUMENT_STRING_VALUES("Order", nOSC_QUERY_MODE_RW, interpolation_mode_args_values)
};

const nOSC_Query_Item sensors_tree [] = {
	nOSC_QUERY_ITEM_NODE("group/", "Group", group_tree),

	nOSC_QUERY_ITEM_METHOD("movingaverage", "Movingaverager", _sensors_movingaverage, sensors_movingaverage_args),
	nOSC_QUERY_ITEM_METHOD("interpolation", "Interpolation", _sensors_interpolation, sensors_interpolation_args),
	nOSC_QUERY_ITEM_METHOD("rate", "Update rate", _sensors_rate, sensors_rate_args),

	nOSC_QUERY_ITEM_METHOD("number", "Sensor number", _sensors_number, sensors_number_args),
};
