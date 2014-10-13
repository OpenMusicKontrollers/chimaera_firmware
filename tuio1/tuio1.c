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
#include <math.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

#include <tuio1.h>

static const char *profile_str [2] = {
	"/tuio/2Dobj",
	"/tuio/_sixya"
};
static const char *fseq_str = "fseq";
static const char *set_str = "set";
static const char *alive_str = "alive";

static const char *frm_fmt = "si";
static const char *tok_fmt [2] = {
	"siiffffffff", // "set" s i x y a X Y A m r
	"siifff" // "set" s i x y a
};
static char alv_fmt [BLOB_MAX+2] = {
	OSC_STRING
}; // this has a variable string len

static int32_t alv_ids [BLOB_MAX];
static uint_fast8_t counter;

static osc_data_t *pack;
static osc_data_t *bndl;
static osc_data_t *pp;

static osc_data_t *
tuio1_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)fid;
	(void)now;
	(void)nblob_old;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	uint_fast8_t i;
	for(i=0; i<nblob_new; i++)
		alv_fmt[i+1] = OSC_INT32;
	alv_fmt[nblob_new+1] = '\0';

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, profile_str[config.tuio1.custom_profile]);
		buf_ptr = osc_set_fmt(buf_ptr, alv_fmt);

		buf_ptr = osc_set_string(buf_ptr, alive_str);
		pp = buf_ptr;
		buf_ptr += nblob_new * 4;
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	counter = 0; // reset token pointer

	return buf_ptr;
}

static osc_data_t *
tuio1_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	(void)now;
	(void)offset;
	(void)nblob_old;
	(void)nblob_new;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	for(i=0; i<nblob_new; i++)
		pp = osc_set_int32(pp, alv_ids[i]);

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, profile_str[config.tuio1.custom_profile]);
		buf_ptr = osc_set_fmt(buf_ptr, frm_fmt);

		buf_ptr = osc_set_string(buf_ptr, fseq_str);
		buf_ptr = osc_set_int32(buf_ptr, fid);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
tuio1_engine_token_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, profile_str[config.tuio1.custom_profile]);
		buf_ptr = osc_set_fmt(buf_ptr, tok_fmt[config.tuio1.custom_profile]);

		buf_ptr = osc_set_string(buf_ptr, set_str);
		buf_ptr = osc_set_int32(buf_ptr, sid);
		buf_ptr = osc_set_int32(buf_ptr, gid);
		buf_ptr = osc_set_float(buf_ptr, x);
		buf_ptr = osc_set_float(buf_ptr, y);
		buf_ptr = osc_set_float(buf_ptr, pid == CMC_NORTH ? 0.f : M_PI);
		if(!config.tuio1.custom_profile)
		{
			buf_ptr = osc_set_float(buf_ptr, 0.f);
			buf_ptr = osc_set_float(buf_ptr, 0.f);
			buf_ptr = osc_set_float(buf_ptr, 0.f);
			buf_ptr = osc_set_float(buf_ptr, 0.f);
			buf_ptr = osc_set_float(buf_ptr, 0.f);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	alv_ids[counter++] = sid;

	return buf_ptr;
}

CMC_Engine tuio1_engine = {
	NULL,
	tuio1_engine_frame_cb,
	tuio1_engine_token_cb,
	NULL,
	tuio1_engine_token_cb,
	tuio1_engine_end_cb
};

/*
 * Config
 */

static uint_fast8_t
_tuio1_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.tuio1.enabled);
	cmc_engines_update();
	return res;
}

static uint_fast8_t
_tuio1_custom_profile(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.tuio1.custom_profile);
}

/*
 * Query
 */

const OSC_Query_Item tuio1_tree [] = {
	// read-write
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _tuio1_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("custom_profile", "Toggle custom profile", _tuio1_custom_profile, config_boolean_args),
};
