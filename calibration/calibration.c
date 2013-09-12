/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
#include <eeprom.h>
#include "../cmc/cmc_private.h"

#include "calibration_private.h"

float Y1 = 0.7;
Calibration range __CCM__;
uint16_t arr [2][SENSOR_N]; //FIXME reuse some other memory
uint_fast8_t zeroing = 0;
uint_fast8_t calibrating = 0;

uint_fast8_t curvefitting = 0;
uint_fast8_t curvefit_nr = 0;
int16_t curvefit_south = 0;
int16_t curvefit_north = 0;

static float
_as (uint16_t qui, uint16_t out_s, uint16_t out_n, uint16_t b)
{
	float _qui = (float)qui;
	float _out_s = (float)out_s;
	float _out_n = (float)out_n;
	float _b = (float)b;

	return _qui / _b * (_out_s - _out_n) / (_out_s + _out_n);
}

uint_fast8_t
range_load (uint_fast8_t pos)
{
	/* TODO
	if (version_match ()) // EEPROM and FLASH config versions match
		eeprom_bulk_read (eeprom_24LC64, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&range, sizeof (range));
	else // EEPROM and FLASH config version do not match, overwrite old with new default one
	{
		range_reset ();
		range_save (pos);
	}
	*/
	eeprom_bulk_read (eeprom_24LC64, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&range, sizeof (range));

	return 1;
}

uint_fast8_t
range_reset ()
{
	uint_fast8_t i;
	for (i=0; i<SENSOR_N; i++)
	{
		range.thresh[i] = 0;
		range.qui[i] = 0x7ff;
		range.as_1_sc_1[i] = 1.0;
		range.bmin_sc_1 = 0.0;
	}

	return 1;
}

uint_fast8_t
range_save (uint_fast8_t pos)
{
	eeprom_bulk_write (eeprom_24LC64, EEPROM_RANGE_OFFSET + pos*EEPROM_RANGE_SIZE, (uint8_t *)&range, sizeof (range));

	return 1;
}

void
range_calibrate (int16_t *raw12, int16_t *raw3, uint8_t *order12, uint8_t *order3, int16_t *sum, int16_t *rela)
{
	uint_fast8_t i;
	uint_fast8_t pos;

	// fill rela vector from raw vector
	for (i=0; i<MUX_MAX*ADC_DUAL_LENGTH*2; i++)
	{
		pos = order12[i];
		rela[pos] = raw12[i];
	}

	for (i=0; i<MUX_MAX*ADC_SING_LENGTH; i++)
	{
		pos = order3[i];
		rela[pos] = raw3[i];
	}

	// do the calibration
	for (i=0; i<SENSOR_N; i++)
	{
		uint16_t avg;

		if (zeroing)
		{
			// moving average over 16 samples
			range.qui[i] -= range.qui[i] >> 4;
			range.qui[i] += rela[i];
		}

		//TODO is this the best way to get a mean of min and max?
		if (rela[i] > (avg = arr[POLE_SOUTH][i] >> 4) )
		{
			arr[POLE_SOUTH][i] -= avg;
			arr[POLE_SOUTH][i] += rela[i];
		}

		if (rela[i] < (avg =arr[POLE_NORTH][i] >> 4) )
		{
			arr[POLE_NORTH][i] -= avg;
			arr[POLE_NORTH][i] += rela[i];
		}
	}
}

// calibrate quiescent current
void
range_update_quiescent ()
{
	uint_fast8_t i;

	for (i=0; i<SENSOR_N; i++)
	{
		// final average over last 16 samples
		range.qui[i] >>= 4;

		// reset array to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i] << 4;
		arr[POLE_NORTH][i] = range.qui[i] << 4;
	}
}

// calibrate threshold
void
range_update_b0 ()
{
	uint_fast8_t i;
	uint16_t thresh_s, thresh_n;

	for (i=0; i<SENSOR_N; i++)
	{
		arr[POLE_SOUTH][i] >>= 4; // average over 16 samples
		arr[POLE_NORTH][i] >>= 4;

		thresh_s = arr[POLE_SOUTH][i] - range.qui[i];
		thresh_n = range.qui[i] - arr[POLE_NORTH][i];

		range.thresh[i] = (thresh_s + thresh_n) / 2;

		// reset thresh to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i] << 4;
		arr[POLE_NORTH][i] = range.qui[i] << 4;
	}
}

// calibrate amplification and sensitivity
void
range_update_b1 ()
{
	//FIXME approximate Y1, so that MAX ( (0x7ff * as_1_sc1[i]) - bmin_sc_1) == 1
	//FIXME or calculate Y1' out of A, B, C
	uint_fast8_t i;
	uint16_t b = (float)0x7ff * Y1;
	float as_1;
	float bmin, bmax_s, bmax_n;
	float sc_1;

	float m_bmin = 0;
	float m_bmax = 0;

	for (i=0; i<SENSOR_N; i++)
	{
		arr[POLE_SOUTH][i] >>= 4; // average over 16 samples
		arr[POLE_NORTH][i] >>= 4;

		as_1 = 1.0 / _as (range.qui[i], arr[POLE_SOUTH][i], arr[POLE_NORTH][i], b);

		bmin = (float)range.thresh[i] * as_1;
		bmax_s = ((float)arr[POLE_SOUTH][i] - (float)range.qui[i]) * as_1 / Y1;
		bmax_n = ((float)range.qui[i] - (float)arr[POLE_NORTH][i]) * as_1 / Y1;

		m_bmin += bmin;
		m_bmax += (bmax_s + bmax_n) / 2.0;
	}

	m_bmin /= (float)SENSOR_N;
	m_bmax /= (float)SENSOR_N;

	sc_1 = 1.0 / (m_bmax - m_bmin);
	range.bmin_sc_1 = m_bmin * sc_1;

	for (i=0; i<SENSOR_N; i++)
	{
		as_1 = 1.0 / _as (range.qui[i], arr[POLE_SOUTH][i], arr[POLE_NORTH][i], b);
		range.as_1_sc_1[i] = as_1 * sc_1;

		// reset thresh to quiescent value
		arr[POLE_SOUTH][i] = range.qui[i] << 4;
		arr[POLE_NORTH][i] = range.qui[i] << 4;
	}
}
