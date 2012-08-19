/*
 * Copyright (c) 2012 Hanspeter Portner (agenthp@users.sf.net)
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

#include "cmc_private.h"

#include <math.h>
#include <string.h>

#include <chimutil.h>
#include <config.h>

CMC *
cmc_new (uint8_t ns, uint8_t mb, uint16_t bitdepth, uint16_t th0, uint16_t th1)
{
	uint8_t i;

	CMC *cmc = calloc (1, sizeof (CMC));

	cmc->I = 0;
	cmc->J = 0;
	cmc->fid = 0;
	cmc->sid = 0;

	cmc->n_sensors = ns;
	cmc->max_blobs = mb;
	cmc->bitdepth = bitdepth;
	cmc->_bitdepth = 1.0ur / bitdepth;

	cmc->thresh0 = th0;
	cmc->thresh1 = th1;
	cmc->thresh0_f = sqrt (cmc->thresh0 * cmc->_bitdepth);
	cmc->_thresh0_f = 1.0ur - cmc->thresh0_f;

	cmc->d = 1.0ur / (ns-1);

	cmc->sensors = calloc (ns+2, sizeof (CMC_Sensor));
	cmc->old_blobs = calloc (mb, sizeof (CMC_Blob));
	cmc->new_blobs = calloc (mb, sizeof (CMC_Blob));

	for (i=0; i<ns+2; i++)
	{
		cmc->sensors[i].x = cmc->d * (i-1);
		cmc->sensors[i].v = 0;
		cmc->sensors[i].n = 0;
	}

	cmc->matrix = calloc (mb, sizeof (fix15_t *));
	for (i=0; i<mb; i++)
		cmc->matrix[i] = calloc (mb, sizeof (fix15_t));

	cmc->tuio = tuio2_new (mb);

	cmc->groups = _cmc_group_new ();

	return cmc;
}

void
cmc_free (CMC *cmc)
{
	uint8_t i;

	_cmc_group_free (cmc->groups);
	tuio2_free (cmc->tuio);
	for (i=0; i<cmc->max_blobs; i++)
		free (cmc->matrix[i]);
	free (cmc->matrix);
	free (cmc->new_blobs);
	free (cmc->old_blobs);
	free (cmc->sensors);
	free (cmc);
}

static uint8_t idle = 0; // IDLE_CYCLE of 256

uint8_t
cmc_process (CMC *cmc, int16_t raw[16][10], uint16_t offset[16][9], uint8_t order[16][9], uint8_t mux_max, uint8_t adc_len)
{
	uint8_t p;
	uint8_t i, j;

	// 11us
	for (p=0; p<mux_max; p++)
		//for (i=0; i<adc_len; i++) //FIXME missing sensor unit
		for (i=0; i<adc_len-1; i++)
		{
			uint8_t pos = order[p][i];
			int16_t val = raw[p][i] - offset[p][i];
			uint16_t aval = abs (val);
			if (aval > cmc->thresh0)
			{
				cmc->sensors[pos+1].v = aval;
				cmc->sensors[pos+1].n = val < 0;
			}
			else
				cmc->sensors[pos+1].v = 0;
		}

	// 80us

	uint8_t changed = 1; //TODO actually check for changes

	/*
	 * find maxima
	 */
	cmc->J = 0;

	// look at array for blobs
	for (i=1; i<cmc->n_sensors+1; i++) // TODO merge the loop with the upper one ^^^^
	{
		if (cmc->sensors[i].v)
		{
			uint8_t I;
			uint8_t peak = 1;

			for (I=1; I<config.cmc.peak_thresh; I++)
			{
				if ( (i >= I) && (cmc->sensors[i].v <= cmc->sensors[i-I].v) )
				{
					peak = 0;
					break;
				}

				if ( (i+I <= cmc->n_sensors+1) && (cmc->sensors[i].v < cmc->sensors[i+I].v) )
				{
					peak = 0;
					break;
				}
			}

			if (peak)
			{
				//fit parabola
				ufix32_t x1 = cmc->sensors[i].x;
				ufix32_t d = cmc->d;

				//fix31_t y0 = cmc->sensors[i-1].v * cmc->_bitdepth;
				//fix31_t y1 = cmc->sensors[i].v * cmc->_bitdepth;
				//fix31_t y2 = cmc->sensors[i+1].v * cmc->_bitdepth;

				// lookup table
				fix31_t y0 = dist[cmc->sensors[i-1].v];
				fix31_t y1 = dist[cmc->sensors[i].v];
				fix31_t y2 = dist[cmc->sensors[i+1].v];

				ufix32_t x = x1 + d/2*(y0 - y2) / (y0 - y1 + y2 - y1);
				//ufix32_t y = (-0.125r*y0_2 - 0.125r*y2_2 + 0.25r*y0*y2 + y0*y1 - y1_2 + y1*y2 - y1_2) * teiler;
				ufix32_t y = y0*0.5r + y1*0.5r + y2*0.5r; // TODO is this good enough an approximation?

				// rescale according to threshold
				y -= cmc->thresh0_f;
				y /= cmc->_thresh0_f; // TODO division is expensive, solve differently?

				cmc->new_blobs[cmc->J].sid = -1; // not assigned yet
				cmc->new_blobs[cmc->J].uid = cmc->sensors[i].n ? CMC_SOUTH : CMC_NORTH;
				cmc->new_blobs[cmc->J].group = NULL;
				cmc->new_blobs[cmc->J].x = x;
				cmc->new_blobs[cmc->J].p = y;
				cmc->new_blobs[cmc->J].above_thresh = cmc->sensors[i].v > cmc->thresh1 ? 1 : 0;
				cmc->new_blobs[cmc->J].ignore = 0;

				cmc->J++;

				if (cmc->J >= cmc->max_blobs) // make sure to not exceed maximal number of blobs
					break;
			}
		}
	}

	/*
	 * relate new to old blobs
	 */
	if (cmc->I || cmc->J)
	{
		if (cmc->I == cmc->J) // there has been no change in blob number, so we can relate the old and new lists 1:1 as they are both ordered according to x
		{
			for (j=0; j<cmc->J; j++)
			{
				cmc->new_blobs[j].sid = cmc->old_blobs[j].sid;
				cmc->new_blobs[j].group = cmc->old_blobs[j].group;
			}
		}
		else if (cmc->I > cmc->J) // old blobs have disappeared
		{
			uint8_t n_less = cmc->I - cmc->J; // how many blobs have disappeared
			i = 0;
			for (j=0; j<cmc->J; )
			{
				ufix32_t diff0, diff1;

				if (n_less)
				{
					diff0 = cmc->new_blobs[j].x > cmc->old_blobs[i].x ? cmc->new_blobs[j].x - cmc->old_blobs[i].x : cmc->old_blobs[i].x - cmc->new_blobs[j].x;
					diff1 = cmc->new_blobs[j].x > cmc->old_blobs[i+1].x ? cmc->new_blobs[j].x - cmc->old_blobs[i+1].x : cmc->old_blobs[i+1].x - cmc->new_blobs[j].x;
				}

				if ( n_less && (diff1 < diff0) )
				{
					i += 1;
					cmc->new_blobs[j].sid = cmc->old_blobs[i].sid;
					cmc->new_blobs[j].group = cmc->old_blobs[i].group;

					i += 1;
					j += 1;
				}
				else
				{
					cmc->new_blobs[j].sid = cmc->old_blobs[i].sid;
					cmc->new_blobs[j].group = cmc->old_blobs[i].group;

					i += 1;
					j += 1;
				}
			}
		}
		else // cmc->J > cmc->I // news blobs have appeared
		{
			uint8_t n_more = cmc->J - cmc->I; // how many blobs have appeared
			j = 0;
			for (i=0; i<cmc->I; )
			{
				ufix32_t diff0, diff1;
				
				if (n_more) // only calculate differences when there are still new blobs to be found
				{
					diff0 = cmc->new_blobs[j].x > cmc->old_blobs[i].x ? cmc->new_blobs[j].x - cmc->old_blobs[i].x : cmc->old_blobs[i].x - cmc->new_blobs[j].x;
					diff1 = cmc->new_blobs[j+1].x > cmc->old_blobs[i].x ? cmc->new_blobs[j+1].x - cmc->old_blobs[i].x : cmc->old_blobs[i].x - cmc->new_blobs[j+1].x;
				}

				if ( n_more && (diff1 < diff0) ) // cmc->new_blobs[j] is the new blob
				{
					if (cmc->new_blobs[j].above_thresh) // check whether it is above threshold for a new blob
						cmc->new_blobs[j].sid = ++(cmc->sid); // this is a new blob
					else
						cmc->new_blobs[j].ignore = 1;
					cmc->new_blobs[j].group = NULL;

					n_more -= 1;
					j += 1;
					// do not increase i
				}
				else // 1:1 relation
				{
					cmc->new_blobs[j].sid = cmc->old_blobs[i].sid;
					cmc->new_blobs[j].group = cmc->old_blobs[i].group;
					j += 1;
					i += 1;
				}
			}

			if (n_more)
				for (j=cmc->J - n_more; j<cmc->J; j++)
				{
					if (cmc->new_blobs[j].above_thresh) // check whether it is above threshold for a new blob
						cmc->new_blobs[j].sid = ++(cmc->sid); // this is a new blob
					else
						cmc->new_blobs[j].ignore = 1;
					cmc->new_blobs[j].group = NULL;
				}
		}

		// overwrite blobs that are to be ignored
		uint8_t newJ = 0;
		for (j=0; j<cmc->J; j++)
		{
			uint8_t ignore = cmc->new_blobs[j].ignore;

			if (newJ != j)
			{
				cmc->new_blobs[newJ].sid = cmc->new_blobs[j].sid;
				cmc->new_blobs[newJ].uid = cmc->new_blobs[j].uid;
				cmc->new_blobs[newJ].group = cmc->new_blobs[j].group;
				cmc->new_blobs[newJ].x = cmc->new_blobs[j].x;
				cmc->new_blobs[newJ].p = cmc->new_blobs[j].p;
				cmc->new_blobs[newJ].above_thresh = cmc->new_blobs[j].above_thresh;
				cmc->new_blobs[newJ].ignore = cmc->new_blobs[j].ignore;
			}

			if (!ignore)
				newJ++;
		}
		cmc->J = newJ;

		// relate blobs to groups
		for (j=0; j<cmc->J; j++)
		{
			CMC_Blob *tar = &cmc->new_blobs[j];

			CMC_Group *ptr = cmc->groups;
			while (ptr)
			{
				if ( ( (tar->uid & ptr->uid) == tar->uid) && (tar->x >= ptr->x0) && (tar->x <= ptr->x1) ) //TODO inclusive/exclusive?
				{
					if (tar->group && (tar->group != ptr) ) // give it a new sid when group has changed since last step
						tar->sid = ++(cmc->sid);

					tar->group = ptr;
					break; // match found, do not search further
				}
				ptr = ptr->next;
			}
		}
	}
	else // cmc->I == cmc->J == 0
	{
		changed = 0;
		idle++; // automatic overflow
	}

	// switch blob buffers
	CMC_Blob *tmp = cmc->old_blobs;
	cmc->old_blobs = cmc->new_blobs;
	cmc->new_blobs = tmp;
	cmc->I = cmc->J;

	// only increase frame count when there were changes or idle overflowed
	if (changed || !idle)
		++(cmc->fid);

	return changed || !idle;
}

uint16_t
cmc_write_tuio2 (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf)
{
	uint8_t j;
	uint16_t size;

	tuio2_frm_set (cmc->tuio, cmc->fid, timestamp);
	for (j=0; j<cmc->I; j++)
	{
		CMC_Group *group = cmc->old_blobs[j].group;
		ufix32_t X = cmc->old_blobs[j].x;

		// resize x to group boundary
		if (group->tid > 0)
			X = (X - group->x0)*group->m;

		tuio2_tok_set (cmc->tuio, j,
			cmc->old_blobs[j].sid,
			(cmc->old_blobs[j].uid<<16) | group->tid,
			X,
			cmc->old_blobs[j].p);
	}
	size = tuio2_serialize (cmc->tuio, buf, cmc->I);
	return size;
}

uint16_t 
cmc_dump (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	msg = nosc_message_add_timestamp (msg, timestamp);
	for (i=0; i<cmc->n_sensors; i++)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump", buf);
	nosc_message_free (msg);

	return size;
}

uint16_t 
cmc_dump_partial (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf, uint8_t s0, uint8_t s1)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	msg = nosc_message_add_timestamp (msg, timestamp);
	for (i=s0; i<s1; i++)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump_partial", buf);
	nosc_message_free (msg);

	return size;
}

uint16_t 
cmc_dump_first (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	msg = nosc_message_add_timestamp (msg, timestamp);
	for (i=0; i<cmc->n_sensors; i+=0x10)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump_first", buf);
	nosc_message_free (msg);

	return size;
}

uint16_t 
cmc_dump_unit (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf, uint8_t unit)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	msg = nosc_message_add_timestamp (msg, timestamp);
	msg = nosc_message_add_int32 (msg, unit);
	for (i=unit*0x10; i<(unit+1)*0x10; i++)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].n ? -cmc->sensors[i+1].v : cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump_unit", buf);
	nosc_message_free (msg);

	return size;
}

void 
_cmc_group_free (CMC_Group *group)
{
	CMC_Group *ptr = group;

	while (ptr)
		ptr = _cmc_group_pop (ptr);
}

CMC_Group *
_cmc_group_push (CMC_Group *group, uint16_t tid, uint16_t uid, float x0, float x1)
{
	CMC_Group *new = calloc (1, sizeof (CMC_Group));

	new->next = group;
	new->tid = tid;
	new->uid = uid;
	new->x0 = x0;
	new->x1 = x1;
	new->m = 1.0ur/(x1-x0);

	return new;
}

CMC_Group *
_cmc_group_pop (CMC_Group *group)
{
	CMC_Group *tmp = group->next;
	free (group);
	return tmp;
}

CMC_Group *
_cmc_group_new ()
{
	return _cmc_group_push (NULL, 0, CMC_BOTH, 0.0, 1.0);
}

void 
cmc_group_clear (CMC *cmc)
{
	_cmc_group_free (cmc->groups);
	cmc->groups = _cmc_group_new ();
	cmc->n_groups = 1;
}

uint8_t 
cmc_group_add (CMC *cmc, uint16_t tid, uint16_t uid, float x0, float x1)
{
	if (cmc->n_groups >= config.cmc.max_groups)
		return 0;

	cmc->groups = _cmc_group_push (cmc->groups, tid, uid, x0, x1);
	cmc->n_groups += 1;
	return 1;
}

uint8_t
cmc_group_set (CMC *cmc, uint16_t tid, uint16_t uid, float x0, float x1)
{
	CMC_Group *ptr = cmc->groups;

	while (ptr)
		if (tid == ptr->tid)
		{
			ptr->uid = uid;
			ptr->x0 = x0;
			ptr->x1 = x1;
			ptr->m = 1.0ur/(x1-x0);
			return 1;
		}
	return 0;
}
