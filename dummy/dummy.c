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

#include <string.h>
#include <math.h> // floor

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <cmc.h>

#include <dummy.h>

static const char *dummy_idle_str = "/idle";
static const char *dummy_on_str = "/on";
static const char *dummy_off_str ="/off";
static const char *dummy_set_str ="/set";

static const char *dummy_idle_fmt = "";
static const char *dummy_on_fmt = "iiiff";
static const char *dummy_off_fmt [2] = {
	[0] = "i",			// !redundancy
	[1] = "iii"			// redundancy
};
static const char *dummy_set_fmt [2][2] = {
	[0] = {					// !redundancy
		[0] = "iff",		// !derivatives
		[1] = "iffff" 	// derivatives
	},
	[1] = {					// redundancy
		[0] = "iiiff",	// !derivatives
		[1] = "iiiffff"	// derivatives
	}
};

static osc_data_t *pack;
static osc_data_t *bndl;

static osc_data_t *
dummy_engine_frame_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, end, fev->offset, &bndl);

	if(!(fev->nblob_old + fev->nblob_new))
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, end, dummy_idle_str);
			buf_ptr = osc_set_fmt(buf_ptr, end, dummy_idle_fmt);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);
	}

	return buf_ptr;
}

static osc_data_t *
dummy_engine_end_cb(osc_data_t *buf, osc_data_t *end, CMC_Frame_Event *fev)
{
	(void)fev;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, end, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, pack);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_on_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_on_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_on_fmt);
		buf_ptr = osc_set_int32(buf_ptr, end, bev->sid);
		buf_ptr = osc_set_int32(buf_ptr, end, bev->gid);
		buf_ptr = osc_set_int32(buf_ptr, end, bev->pid);
		buf_ptr = osc_set_float(buf_ptr, end, bev->x);
		buf_ptr = osc_set_float(buf_ptr, end, bev->y);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_off_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	uint_fast8_t redundancy = config.dummy.redundancy;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_off_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_off_fmt[redundancy]);
		buf_ptr = osc_set_int32(buf_ptr, end, bev->sid);
		if(redundancy)
		{
			buf_ptr = osc_set_int32(buf_ptr, end, bev->gid);
			buf_ptr = osc_set_int32(buf_ptr, end, bev->pid);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_set_cb(osc_data_t *buf, osc_data_t *end, CMC_Blob_Event *bev)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	uint_fast8_t redundancy = config.dummy.redundancy;
	uint_fast8_t derivatives = config.dummy.derivatives;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_set_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_set_fmt[redundancy][derivatives]);
		buf_ptr = osc_set_int32(buf_ptr, end, bev->sid);
		if(redundancy)
		{
			buf_ptr = osc_set_int32(buf_ptr, end, bev->gid);
			buf_ptr = osc_set_int32(buf_ptr, end, bev->pid);
		}
		buf_ptr = osc_set_float(buf_ptr, end, bev->x);
		buf_ptr = osc_set_float(buf_ptr, end, bev->y);
		if(derivatives)
		{
			buf_ptr = osc_set_float(buf_ptr, end, bev->vx);
			buf_ptr = osc_set_float(buf_ptr, end, bev->vy);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

CMC_Engine dummy_engine = {
	NULL,
	dummy_engine_frame_cb,
	dummy_engine_on_cb,
	dummy_engine_off_cb,
	dummy_engine_set_cb,
	dummy_engine_end_cb
};

/*
 * Config
 */
static uint_fast8_t
_dummy_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.dummy.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_dummy_redundancy(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.dummy.redundancy);
}

static uint_fast8_t
_dummy_derivatives(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.dummy.derivatives);
}

/*
 * Query
 */

const OSC_Query_Item dummy_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _dummy_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("redundancy", "Send redundant data", _dummy_redundancy, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("derivatives", "Calculate derivatives", _dummy_derivatives, config_boolean_args)
};
