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

#include <tuio2.h>

static const char *frm_str = "/tuio2/frm";
static const char *tok_str = "/tuio2/tok";
static const char *alv_str = "/tuio2/alv";

static const char *frm_fmt [2] = { "it", "itsiii" };
static const char *tok_fmt = "iiifff";
static char alv_fmt [BLOB_MAX+1]; // this has a variable string len

static int32_t alv_ids [BLOB_MAX];
static uint_fast8_t counter;

static int32_t addr;
static int32_t inst;

static osc_data_t *pack;
static osc_data_t *bndl;

static void
tuio2_init()
{
	uint_fast8_t i;
	uint_fast8_t long_header = config.tuio2.long_header;

	// initialize long frame
	uint8_t *mac = config.comm.mac; //FIXME this should be when MAC is changed
	addr =(mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]; // MAC[2:5] last four bytes
	inst = uid_seed(); // 32 bit conglomerate of unique ID
}

static osc_data_t *
tuio2_engine_frame_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	uint_fast8_t long_header = config.tuio2.long_header;

	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, offset, &bndl);

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, frm_str);
		buf_ptr = osc_set_fmt(buf_ptr, frm_fmt[long_header]);

		buf_ptr = osc_set_int32(buf_ptr, fid);
		buf_ptr = osc_set_timetag(buf_ptr, now);
		if(long_header)
		{
			buf_ptr = osc_set_string(buf_ptr, config.name);
			buf_ptr = osc_set_int32(buf_ptr, addr);
			buf_ptr = osc_set_int32(buf_ptr, inst);
			buf_ptr = osc_set_int32(buf_ptr, (SENSOR_N << 16) | 1);
		}
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	counter = 0; // reset token pointer

	return buf_ptr;
}

static osc_data_t *
tuio2_engine_end_cb(osc_data_t *buf, uint32_t fid, OSC_Timetag now, OSC_Timetag offset, uint_fast8_t nblob_old, uint_fast8_t nblob_new)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	uint_fast8_t i;
	for(i=0; i<counter; i++)
		alv_fmt[i] = OSC_INT32;
	alv_fmt[counter] = '\0';

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, alv_str);
		buf_ptr = osc_set_fmt(buf_ptr, alv_fmt);

		for(i=0; i<counter; i++)
			buf_ptr = osc_set_int32(buf_ptr, alv_ids[i]);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, pack);

	return buf_ptr;
}

static osc_data_t *
tuio2_engine_token_cb(osc_data_t *buf, uint32_t sid, uint16_t gid, uint16_t pid, float x, float y)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;

	buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, tok_str);
		buf_ptr = osc_set_fmt(buf_ptr, tok_fmt);

		buf_ptr = osc_set_int32(buf_ptr, sid);
		buf_ptr = osc_set_int32(buf_ptr, pid);
		buf_ptr = osc_set_int32(buf_ptr, gid);
		buf_ptr = osc_set_float(buf_ptr, x);
		buf_ptr = osc_set_float(buf_ptr, y);
		buf_ptr = osc_set_float(buf_ptr, pid == CMC_NORTH ? 0.f : M_PI);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, itm);

	alv_ids[counter++] = sid;

	return buf_ptr;
}

CMC_Engine tuio2_engine = {
	tuio2_init,
	tuio2_engine_frame_cb,
	tuio2_engine_token_cb,
	NULL,
	tuio2_engine_token_cb,
	tuio2_engine_end_cb
};

/*
 * Config
 */
static uint_fast8_t
_tuio2_long_header(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	return config_check_bool(path, fmt, argc, buf, &config.tuio2.long_header);
}

static uint_fast8_t
_tuio2_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.tuio2.enabled);
	cmc_engines_update();
	return res;
}

/*
 * Query
 */

const OSC_Query_Item tuio2_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _tuio2_enabled, config_boolean_args),
	OSC_QUERY_ITEM_METHOD("long_header", "Enalbe/disable frame long header", _tuio2_long_header, config_boolean_args),
};
