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

#include <chimaera.h>
#include <config.h>

#include <dump.h>

static const char *dump_str = "/dump";
static const char *dump_fmt = "ib";

static uint32_t frame = 0;

osc_data_t *
dump_update(osc_data_t *buf, osc_data_t *end, OSC_Timetag now, OSC_Timetag offset, int32_t len, int16_t *swap)
{
	(void)now;
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	osc_data_t *pack = NULL;
	osc_data_t *bndl;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_bundle_item(buf_ptr, end, &pack);
	buf_ptr = osc_start_bundle(buf_ptr, end, offset, &bndl);

	buf_ptr = osc_start_bundle_item(buf_ptr, end, &itm);
	{
		buf_ptr = osc_set_path(buf_ptr, end, dump_str);
		buf_ptr = osc_set_fmt(buf_ptr, end, dump_fmt);
		buf_ptr = osc_set_int32(buf_ptr, end, ++frame);
		buf_ptr = osc_set_blob(buf_ptr, end, len, swap);
	}
	buf_ptr = osc_end_bundle_item(buf_ptr, end, itm);

	buf_ptr = osc_end_bundle(buf_ptr, end, bndl);
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_bundle_item(buf_ptr, end, pack);

	return buf_ptr;
}

/*
 * Config
 */
static uint_fast8_t
_dump_enabled(const char *path, const char *fmt, uint_fast8_t argc, osc_data_t *buf)
{
	uint_fast8_t res = config_check_bool(path, fmt, argc, buf, &config.dump.enabled);
	cmc_engines_update();
	return res;
}

/*
 * Query
 */

const OSC_Query_Item dump_tree [] = {
	OSC_QUERY_ITEM_METHOD("enabled", "Enable/disable", _dump_enabled, config_boolean_args),
};
