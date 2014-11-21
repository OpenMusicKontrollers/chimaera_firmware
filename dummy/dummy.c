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
static const char *dummy_off_fmt [] = {
	[0] = "i",			// !redundancy
	[1] = "iii"			// redundancy
};
static const char *dummy_set_fmt [] = {
	[0] = "iff",		// !redundancy
	[1] = "iiiff"		// redundancy
};

static osc_data_t *pack;
static osc_data_t *bndl;

static osc_data_t *
dummy_engine_frame_cb(osc_data_t *buf, osc_data_t *end, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, end, offset, &bndl);

	if(!(nblob_old + nblob_new))
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
dummy_engine_end_cb(osc_data_t *buf, osc_data_t *end, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	(void)offset;
	(void)nblob_old;
	(void)nblob_new;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, end, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, pack);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_on_cb(osc_data_t *buf, osc_data_t *end, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_on_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_on_fmt);
		buf_ptr = osc_set_int32(buf_ptr, end, sid);
		buf_ptr = osc_set_int32(buf_ptr, end, gid);
		buf_ptr = osc_set_int32(buf_ptr, end, pid);
		buf_ptr = osc_set_float(buf_ptr, end, x);
		buf_ptr = osc_set_float(buf_ptr, end, y);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_off_cb(osc_data_t *buf, osc_data_t *end, uint32_t sid, uint16_t gid, uint16_t pid)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	uint_fast8_t redundancy = config.dummy.redundancy;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_off_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_off_fmt[redundancy]);
		buf_ptr = osc_set_int32(buf_ptr, end, sid);
		if(redundancy)
		{
			buf_ptr = osc_set_int32(buf_ptr, end, gid);
			buf_ptr = osc_set_int32(buf_ptr, end, pid);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_set_cb(osc_data_t *buf, osc_data_t *end, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	uint_fast8_t redundancy = config.dummy.redundancy;

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dummy_set_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dummy_set_fmt[redundancy]);
		buf_ptr = osc_set_int32(buf_ptr, end, sid);
		if(redundancy)
		{
			buf_ptr = osc_set_int32(buf_ptr, end, gid);
			buf_ptr = osc_set_int32(buf_ptr, end, pid);
		}
		buf_ptr = osc_set_float(buf_ptr, end, x);
		buf_ptr = osc_set_float(buf_ptr, end, y);
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

/*
 * Query
 */

const OSC_Query_Item dummy_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _dummy_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("redundancy", "Send redundant data", _dummy_redundancy, config_boolean_args),
};
