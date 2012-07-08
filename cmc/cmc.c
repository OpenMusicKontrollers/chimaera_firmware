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

CMC *
cmc_new (uint8_t ns, uint8_t mb, uint16_t bitdepth, uint16_t df, uint16_t th0, uint16_t th1)
{
	uint8_t i;

	CMC *cmc = calloc (1, sizeof (CMC));

	cmc->I = 0;
	cmc->J = 0;
	cmc->fid = 0;
	cmc->sid = 0;

	cmc->n_sensors = ns;
	cmc->max_blobs = mb;
	cmc->diff = df;
	cmc->bitdepth = bitdepth;
	cmc->_bitdepth = 1.0 / (float)bitdepth;

	cmc->thresh0 = th0;
	cmc->thresh1 = th1;
	cmc->thresh0_f = (float)cmc->thresh0 * cmc->_bitdepth;
	cmc->_thresh0_f = 1.0 / (1.0 - cmc->thresh0_f);

	cmc->d = 1.0 / (ns-1.0);

	cmc->sensors = calloc (ns+2, sizeof (CMC_Sensor));
	cmc->old_blobs = calloc (mb, sizeof (CMC_Blob));
	cmc->new_blobs = calloc (mb, sizeof (CMC_Blob));

	for (i=0; i<ns+2; i++)
	{
		cmc->sensors[i].x = cmc->d * (i-1.0);
		cmc->sensors[i].v = 0x0;
	}

	cmc->tuio = tuio2_new (mb);

	cmc->groups = _cmc_group_new ();

	return cmc;
}

void
cmc_free (CMC *cmc)
{
	_cmc_group_free (cmc->groups);
	tuio2_free (cmc->tuio);
	free (cmc->new_blobs);
	free (cmc->old_blobs);
	free (cmc->sensors);
	free (cmc);
}

void
cmc_set (CMC* cmc, uint8_t i, uint16_t v)
{
	cmc->sensors[i+1].v = v;
}

static uint8_t idle = 0; // IDLE_CYCLE of 256

uint8_t
cmc_process (CMC *cmc)
{
	uint8_t i, j;
	uint8_t changed = 1; //TODO actually check for changes

	/*
	 * find maxima
	 */
	cmc->J = 0;
	for (i=1; i<cmc->n_sensors+1; i++)
		if ( (cmc->sensors[i].v > cmc->thresh0) && (cmc->sensors[i].v - cmc->sensors[i-1].v > cmc->diff) && (cmc->sensors[i].v - cmc->sensors[i+1].v > cmc->diff) )
		{
			/*
			 * fit parabola
			 */

			float x1 = cmc->sensors[i].x;
			float d = cmc->d;

			float y0 = (float)cmc->sensors[i-1].v * cmc->_bitdepth;
			float y1 = (float)cmc->sensors[i].v * cmc->_bitdepth;
			float y2 = (float)cmc->sensors[i+1].v * cmc->_bitdepth;

			float x = x1 + 0.5*d*(y0 - y2) / (y0 - 2.0*y1 + y2);
			float y = y0 + y1 + y2; //TODO this can be bigger than 1.0

			// rescale according to threshold
			y -= cmc->thresh0_f;
			y *= cmc->_thresh0_f;

			if (x < 0.0) x = 0.0;
			if (x > 1.0) x = 1.0;
			if (y < 0.0) y = 0.0;
			if (y > 1.0) y = 1.0;

			cmc->new_blobs[cmc->J].sid = i;
			cmc->new_blobs[cmc->J].x = x;
			cmc->new_blobs[cmc->J].p = y;
			cmc->new_blobs[cmc->J].above_thresh = cmc->sensors[i].v > cmc->thresh1 ? 1 : 0;
			cmc->new_blobs[cmc->J].ignore = 0;

			cmc->J++;
		}

	/*
	 * relate new to old blobs
	 */

	if (cmc->I && cmc->J)
	{
		// fill distance matrix of old and new blobs
		float matrix [cmc->I][cmc->J];
		for (i=0; i<cmc->I; i++) // old blobs
			for (j=0; j<cmc->J; j++) // new blobs
				matrix[i][j] = fabs (cmc->new_blobs[j].x - cmc->old_blobs[i].x); // 1D

		uint8_t ptr = cmc->J;
		while (ptr)
		{
			float min = 99.9;
			uint8_t a = 0, b = 0;

			for (i=0; i<cmc->I; i++) // old blobs
				for (j=0; j<cmc->J; j++) // new blobs
				{
					if (matrix[i][j] == -1)
						continue;

					if (matrix[i][j] < min)
					{
						a = i;
						b = j;
						min = matrix[i][j];
					}
				}

			CMC_Blob *blb = &(cmc->old_blobs[a]);
			CMC_Blob *blb2 = &(cmc->new_blobs[b]);

			if (cmc->J > cmc->I)
			{
				switch (ptr)
				{
					case 1:
						if (blb2->above_thresh) // check whether it is above threshold for a new blob
							blb2->sid = ++(cmc->sid); // this is a new blob
						else
							blb2->ignore = 1;
						blb2->group = NULL;
						break;
					case 2:
						blb2->sid = blb->sid;
						blb2->group = blb->group;
						matrix[a][b] = -1;
						break;
					default:
						blb2->sid = blb->sid;
						blb2->group = blb->group;
						for (i=0; i<cmc->I; i++)
							matrix[i][b] = -1;
						for (j=0; j<cmc->J; j++)
							matrix[a][j] = -1;
						break;
				}
			}
			else // J <= I
			{
				blb2->sid = blb->sid;
				blb2->group = blb->group;
				for (i=0; i<cmc->I; i++)
					matrix[i][b] = -1;
				for (j=0; j<cmc->J; j++)
					matrix[a][j] = -1;
			}
			ptr--;
		}
	}
	else if (!cmc->I && cmc->J)
	{
		for (j=0; j<cmc->J; j++) // check whether it is above threshold for a new blob
		{
			if (cmc->new_blobs[j].above_thresh)
				cmc->new_blobs[j].sid = ++(cmc->sid); // this is a new blob
			else
				cmc->new_blobs[j].ignore = 1;
			cmc->new_blobs[j].group = NULL;
		}
	}
	else if (!cmc->I && !cmc->J)
	{
		changed = 0;
		idle++; // automatic overflow
	}

	// overwrite blobs that are to be ignored
	uint8_t newJ = 0;
	for (j=0; j<cmc->J; j++)
	{
		uint8_t ignore = cmc->new_blobs[j].ignore;

		if (newJ != cmc->J)
		{
			cmc->new_blobs[newJ].sid = cmc->new_blobs[j].sid;
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
			if ( (tar->x >= ptr->x0) && (tar->x <= ptr->x1) ) //TODO inclusive/exclusive?
			{
				if (tar->group && (tar->group != ptr) ) // give it a new sid when group has changed
					tar->sid = ++(cmc->sid);

				tar->group = ptr;
				break; // do not search further
			}
			ptr = ptr->next;
		}
	}

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
cmc_write (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf)
{
	uint8_t j;
	uint16_t size;
	float rx;

	tuio2_frm_set (cmc->tuio, cmc->fid, timestamp);
	for (j=0; j<cmc->I; j++)
	{
		CMC_Group *group;
		float X = cmc->old_blobs[j].x;
		// resize x to group boundary
		if (group = cmc->old_blobs[j].group)
			X = X*(group->x1 - group->x0) + group->x0;

		tuio2_tok_set (cmc->tuio, j,
			cmc->old_blobs[j].sid,
			cmc->old_blobs[j].group->cid,
			X,
			cmc->old_blobs[j].p);
	}
	size = tuio2_serialize (cmc->tuio, buf, cmc->I);
	return size;
}

uint16_t 
cmc_dump (CMC *cmc, uint8_t *buf)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	for (i=0; i<cmc->n_sensors; i++)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump", buf);
	nosc_message_free (msg);

	return size;
}

uint16_t 
cmc_dump_partial (CMC *cmc, uint8_t *buf, uint8_t s0, uint8_t s1)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	for (i=s0; i<s1; i++)
		msg = nosc_message_add_int32 (msg, cmc->sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump_partial", buf);
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
_cmc_group_push (CMC_Group *group, uint32_t cid, float x0, float x1)
{
	CMC_Group *new = calloc (1, sizeof (CMC_Group));

	new->next = group;
	new->cid = cid;
	new->x0 = x0;
	new->x1 = x1;

	return new;
}

CMC_Group *
_cmc_group_pop (CMC_Group *group)
{
	CMC_Group *tmp = group->next;
	free (group);
	return tmp;
}

void 
cmc_group_clear (CMC *cmc)
{
	_cmc_group_clear (cmc->groups);
	cmc->groups = _cmc_group_new ();
}

void 
cmc_group_add (CMC *cmc, uint32_t cid, float x0, float x1)
{
	//TODO we cannot create an indefinite number of groups, put a limit and return an error when overrun
	cmc->groups = _cmc_group_push (cmc->groups, cid, x0, x1);
}

CMC_Group *
_cmc_group_new ()
{
	return NULL;
}
