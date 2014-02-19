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

static uint_fast8_t
_sensors_number(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	size = CONFIG_SUCCESS("isi", uuid, path, SENSOR_N);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

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

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

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

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_sensors_interpolation(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	return config_check_uint8(path, fmt, argc, args, &config.sensors.interpolation_order);
}

static uint_fast8_t
_group_clear(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	cmc_group_clear();

	size = CONFIG_SUCCESS("is", uuid, path);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_attributes(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;
	uint16_t gid = args[1].i;

	uint16_t pid;
	float x0;
	float x1;
	uint_fast8_t scale;
		
	CMC_Group *grp = &cmc_groups[gid];

	if(argc == 2) // request group info
	{

		pid = grp->pid;
		x0 = grp->x0;
		x1 = grp->x1;
		scale = grp->m == CMC_NOSCALE ? 0 : 1;

		size = CONFIG_SUCCESS("isiiffi", uuid, path, gid, pid, x0, x1, scale);
	}
	else // set group info
	{
		pid = args[2].i;
		x0 = args[3].f;
		x1 = args[4].f;
		scale = args[5].i;
		
		grp->pid = pid;
		grp->x0 = x0;
		grp->x1 = x1;
		grp->m = scale ? 1.f/(x1-x0) : CMC_NOSCALE;

		size = CONFIG_SUCCESS("is", uuid, path);
	}

	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_load(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(groups_load())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "groups could not be loaded from EEPROM");
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_save(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	if(groups_save())
		size = CONFIG_SUCCESS("is", uuid, path);
	else
		size = CONFIG_FAIL("iss", uuid, path, "groups could not be saved to EEPROM");
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

static uint_fast8_t
_group_number(const char *path, const char *fmt, uint_fast8_t argc, nOSC_Arg *args)
{
	uint16_t size;
	int32_t uuid = args[0].i;

	size = CONFIG_SUCCESS("isi", uuid, path, GROUP_MAX);
	udp_send(config.config.socket.sock, BUF_O_BASE(buf_o_ptr), size);

	return 1;
}

/*
 * Query
 */

static const nOSC_Query_Argument group_number_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Number", nOSC_QUERY_MODE_R, GROUP_MAX, GROUP_MAX)
};

static const nOSC_Query_Argument group_attributes_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Number", nOSC_QUERY_MODE_RWX, 0, GROUP_MAX - 1),
	nOSC_QUERY_ARGUMENT_INT32("Polarity", nOSC_QUERY_MODE_RW, 0, 0x180),
	nOSC_QUERY_ARGUMENT_FLOAT("Min", nOSC_QUERY_MODE_RW, 0.f, 1.f),
	nOSC_QUERY_ARGUMENT_FLOAT("Max", nOSC_QUERY_MODE_RW, 0.f, 1.f),
	nOSC_QUERY_ARGUMENT_BOOL("Scale?", nOSC_QUERY_MODE_RW),
};

const nOSC_Query_Item group_tree [] = {
	nOSC_QUERY_ITEM_METHOD("load", "Load from EEPROM", _group_load, NULL),
	nOSC_QUERY_ITEM_METHOD("save", "Save to EEPROM", _group_save, NULL),
	nOSC_QUERY_ITEM_METHOD("reset", "Reset all groups", _group_clear, NULL),
	nOSC_QUERY_ITEM_METHOD("number", "Number", _group_number, group_number_args),
	nOSC_QUERY_ITEM_METHOD("attributes", "Attributes", _group_attributes, group_attributes_args),
};

static const nOSC_Query_Argument sensors_number_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Number", nOSC_QUERY_MODE_R, SENSOR_N, SENSOR_N)
};

static const nOSC_Query_Argument sensors_rate_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Hz", nOSC_QUERY_MODE_RW, 0, 10000)
};

static const nOSC_Query_Argument sensors_movingaverage_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Sample window [1,2,4,8]", nOSC_QUERY_MODE_RW, 1, 8)
};

static const nOSC_Query_Argument sensors_interpolation_args [] = {
	nOSC_QUERY_ARGUMENT_INT32("Order [0,1,2,3,4]", nOSC_QUERY_MODE_RW, 0, 4)
};

const nOSC_Query_Item sensors_tree [] = {
	nOSC_QUERY_ITEM_NODE("group/", "Group", group_tree),

	nOSC_QUERY_ITEM_METHOD("movingaverage", "Movingaverager", _sensors_movingaverage, sensors_movingaverage_args),
	nOSC_QUERY_ITEM_METHOD("interpolation", "Interpolation", _sensors_interpolation, sensors_interpolation_args),
	nOSC_QUERY_ITEM_METHOD("rate", "Update rate", _sensors_rate, sensors_rate_args),

	nOSC_QUERY_ITEM_METHOD("number", "Sensor number", _sensors_number, sensors_number_args),
};
