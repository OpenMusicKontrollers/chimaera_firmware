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
static const char *dummy_off_fmt = "iii";
static const char *dummy_set_fmt = "iiiff";

static osc_data_t *pack;
static osc_data_t *bndl;

void
dummy_init()
{
	// do nothing
}

static osc_data_t *
dummy_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	if(!(nblob_old + nblob_new))
	{
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, dummy_idle_str);
			buf_ptr = osc_set_fmt(buf_ptr, dummy_idle_fmt);
		}
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);
	}

	return buf_ptr;
}

static osc_data_t *
dummy_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_on_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, dummy_on_str);
		buf_ptr = osc_set_fmt(buf_ptr, dummy_on_fmt);
		buf_ptr = osc_set_int32(buf_ptr, sid);
		buf_ptr = osc_set_int32(buf_ptr, gid);
		buf_ptr = osc_set_int32(buf_ptr, pid);
		buf_ptr = osc_set_float(buf_ptr, x);
		buf_ptr = osc_set_float(buf_ptr, y);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_off_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, dummy_off_str);
		buf_ptr = osc_set_fmt(buf_ptr, dummy_off_fmt);
		buf_ptr = osc_set_int32(buf_ptr, sid);
		buf_ptr = osc_set_int32(buf_ptr, gid);
		buf_ptr = osc_set_int32(buf_ptr, pid);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

static osc_data_t *
dummy_engine_set_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, dummy_set_str);
		buf_ptr = osc_set_fmt(buf_ptr, dummy_set_fmt);
		buf_ptr = osc_set_int32(buf_ptr, sid);
		buf_ptr = osc_set_int32(buf_ptr, gid);
		buf_ptr = osc_set_int32(buf_ptr, pid);
		buf_ptr = osc_set_float(buf_ptr, x);
		buf_ptr = osc_set_float(buf_ptr, y);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	return buf_ptr;
}

CMC_Engine dummy_engine = {
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

/*
 * Query
 */

const OSC_Query_Item dummy_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _dummy_enabled, config_boolean_args),
};
