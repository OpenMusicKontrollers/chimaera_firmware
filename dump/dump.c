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

#include <chimaera.h>
#include <config.h>

#include <dump.h>

static const char *dump_str = "/dump";
static const char *dump_fmt = "ib";

static uint32_t frame = 0;
static int32_t len;
static uint8_t *payload;

void
dump_init(int32_t size, int16_t *swap)
{
	len = size;
	payload = (uint8_t *)swap;
}

osc_data_t *
dump_update(osc_data_t *buf, OSC_Timetag now, OSC_Timetag offset)
{
	osc_data_t *buf_ptr = buf;
	osc_data_t *itm;
	osc_data_t *pack;

	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_start_item_variable(buf_ptr, &pack);
	{
		buf_ptr = osc_start_bundle(buf_ptr, offset);
		buf_ptr = osc_start_item_variable(buf_ptr, &itm);
		{
			buf_ptr = osc_set_path(buf_ptr, dump_str);
			buf_ptr = osc_set_fmt(buf_ptr, dump_fmt);
			buf_ptr = osc_set_int32(buf_ptr, ++frame);
			buf_ptr = osc_set_blob(buf_ptr, len, payload);
		}
		buf_ptr = osc_end_item_variable(buf_ptr, itm);
	}
	if(cmc_engines_active + config.dump.enabled > 1)
		buf_ptr = osc_end_item_variable(buf_ptr, pack);

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
