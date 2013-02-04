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
#include "../config/config_private.h"
#include "../sntp/sntp_private.h"

#include <math.h>
#include <string.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <../sntp/sntp_private.h>

#define s2d2 0.70710678118655r // sqrt(2) / 2
#define os8 0.35355339059327r // 1 / sqrt(8)

static uint16_t idle_word = 0;
static uint8_t idle_bit = 0;
CMC cmc;

void
cmc_init ()
{
	uint8_t i;

	cmc.I = 0;
	cmc.J = 0;
	cmc.fid = 0; // we start counting at 1, 0 marks an 'out of order' frame
	cmc.sid = 0; // we start counting at 0

	cmc.d = 1.0ulr / SENSOR_N;
	cmc.d_2 = cmc.d / 2;

	for (i=0; i<SENSOR_N+2; i++)
	{
		cmc.x[i] = cmc.d * i - cmc.d_2; //TODO caution: sensors[0] and sensors[145] get saturated to 0 and 1
		cmc.v[i] = 0;
		cmc.n[i] = 0;
	}

	cmc.n_groups = 0;
	cmc_group_add (0, CMC_BOTH, 0.0, 1.0);

	cmc.old = 0;
	cmc.neu = 1;
}

#define THRESH_MIN 30 // absolute minimum threshold FIXME make this configurable

uint8_t n_aoi;
uint8_t aoi[64]; //TODO how big?

uint8_t n_peaks;
uint8_t peaks[BLOB_MAX];

static int
aoi_cmp (const void *a, const void *b)
{
	const uint8_t *A = a;
	const uint8_t *B = b;

	if (*A == *B)
		return 0;
	
	if (*A < *B)
		return -1;
	
	return 1;
}

uint8_t
cmc_process (int16_t *rela)
{
	n_aoi = 0;
	uint8_t pos;
	for (pos=0; pos<SENSOR_N; pos++)
	{
		int16_t val = rela[pos];
		uint16_t aval = abs (val);
		uint8_t pole = val < 0 ? POLE_NORTH : POLE_SOUTH;
		uint8_t newpos = pos+1;
		if ( (aval > THRESH_MIN) && (aval*4 > adc_range[newpos].thresh[pole]) ) // thresh0 == thresh1 / 4
		{
			aoi[n_aoi++] = newpos;

			cmc.n[newpos] = pole;
			cmc.a[newpos] = aval > adc_range[newpos].thresh[pole];
			cmc.v[newpos] = adc_range[newpos].A[pole].fix * lookup_sqrt[aval]
										+ adc_range[newpos].B[pole].fix * lookup[aval]
										+ adc_range[newpos].C[pole].fix;
		}
	}

	uint8_t changed = 1; //TODO actually check for changes

	/*
	 * find maxima
	 */

	// search for peaks
	n_peaks = 0;
	uint8_t a;
	fix_0_32_t max = cmc.v[aoi[0]];
	uint8_t peak = aoi[0];
	uint8_t up = 1;
	for (a=1; a<n_aoi; a++)
	{
		if (aoi[a] - aoi[a-1] > 1) // new aoi
		{
			up = 1;
			peak = aoi[a];
			max = cmc.v[peak];
			continue;
		}

		if (up)
		{
			if (cmc.v[aoi[a]] >= max)
			{
				peak = aoi[a];
				max = cmc.v[peak];
			}
			else // < max
			{
				peaks[n_peaks++] = peak;
				up = 0;
			}
		}
	}

	// handle found peaks to create blobs
	cmc.J = 0;
	uint8_t p;
	for (p=0; p<n_peaks; p++)
	{
		uint8_t P = peaks[p];
		//fit parabola

		fix_s_31_t y0 = cmc.v[P-1];
		fix_s_31_t y1 = cmc.v[P];
		fix_s_31_t y2 = cmc.v[P+1];

		// parabolic interpolation
		//fix_s_31_t divisor = y0 - y1 + y2 - y1;
		fix_s31_32_t divisor = y0 - 2*y1 + y2;
		fix_0_32_t x = cmc.x[P] + cmc.d_2*(y0 - y2) / divisor;

		fix_0_32_t y = y1; // this is better, simpler and faster than any interpolation (just KISS)

		//fix_s31_32_t dividend = -0.125r*y0*y0 + y0*y1 - 2*y1*y1 + 0.25r*y0*y2 + y1*y2 - 0.125r*y2*y2;
		//fix_0_32_t y = dividend / divisor;

		if ( (x > 0.r) && (y > 0.0r) ) //FIXME why can this happen in the first place?
		{
			cmc.blobs[cmc.neu][cmc.J].sid = -1; // not assigned yet
			cmc.blobs[cmc.neu][cmc.J].uid = cmc.n[P] == POLE_NORTH ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity (+B) magnetic fields increase the output voltage, north-polaritiy (-B) decrease it
			cmc.blobs[cmc.neu][cmc.J].group = NULL;
			cmc.blobs[cmc.neu][cmc.J].x = x;
			cmc.blobs[cmc.neu][cmc.J].p = y;
			cmc.blobs[cmc.neu][cmc.J].above_thresh = cmc.a[P];
			cmc.blobs[cmc.neu][cmc.J].ignore = 0;

			cmc.J++;
		}
	} // 50us per blob

	/*
	 * relate new to old blobs
	 */
	uint8_t i, j;
	if (cmc.I || cmc.J)
	{
		idle_word = 0;
		idle_bit = 0;

		if (cmc.I == cmc.J) // there has been no change in blob number, so we can relate the old and new lists 1:1 as they are both ordered according to x
		{
			for (j=0; j<cmc.J; j++)
			{
				cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][j].sid;
				cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][j].group;
			}
		}
		else if (cmc.I > cmc.J) // old blobs have disappeared
		{
			uint8_t n_less = cmc.I - cmc.J; // how many blobs have disappeared
			i = 0;
			for (j=0; j<cmc.J; )
			{
				fix_0_32_t diff0, diff1;

				if (n_less)
				{
					diff0 = cmc.blobs[cmc.neu][j].x > cmc.blobs[cmc.old][i].x ? cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i].x : cmc.blobs[cmc.old][i].x - cmc.blobs[cmc.neu][j].x;
					diff1 = cmc.blobs[cmc.neu][j].x > cmc.blobs[cmc.old][i+1].x ? cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i+1].x : cmc.blobs[cmc.old][i+1].x - cmc.blobs[cmc.neu][j].x;
				}

				if ( n_less && (diff1 < diff0) )
				{
					i += 1;
					cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][i].sid;
					cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][i].group;

					i += 1;
					j += 1;
				}
				else
				{
					cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][i].sid;
					cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][i].group;

					i += 1;
					j += 1;
				}
			}
		}
		else // cmc.J > cmc.I // news blobs have appeared
		{
			uint8_t n_more = cmc.J - cmc.I; // how many blobs have appeared
			j = 0;
			for (i=0; i<cmc.I; )
			{
				fix_0_32_t diff0, diff1;
				
				if (n_more) // only calculate differences when there are still new blobs to be found
				{
					diff0 = cmc.blobs[cmc.neu][j].x > cmc.blobs[cmc.old][i].x ? cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i].x : cmc.blobs[cmc.old][i].x - cmc.blobs[cmc.neu][j].x;
					diff1 = cmc.blobs[cmc.neu][j+1].x > cmc.blobs[cmc.old][i].x ? cmc.blobs[cmc.neu][j+1].x - cmc.blobs[cmc.old][i].x : cmc.blobs[cmc.old][i].x - cmc.blobs[cmc.neu][j+1].x;
				}

				if ( n_more && (diff1 < diff0) ) // cmc.blobs[cmc.neu][j] is the new blob
				{
					if (cmc.blobs[cmc.neu][j].above_thresh) // check whether it is above threshold for a new blob
						cmc.blobs[cmc.neu][j].sid = ++(cmc.sid); // this is a new blob
					else
						cmc.blobs[cmc.neu][j].ignore = 1;
					cmc.blobs[cmc.neu][j].group = NULL;

					n_more -= 1;
					j += 1;
					// do not increase i
				}
				else // 1:1 relation
				{
					cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][i].sid;
					cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][i].group;
					j += 1;
					i += 1;
				}
			}

			if (n_more)
				for (j=cmc.J - n_more; j<cmc.J; j++)
				{
					if (cmc.blobs[cmc.neu][j].above_thresh) // check whether it is above threshold for a new blob
						cmc.blobs[cmc.neu][j].sid = ++(cmc.sid); // this is a new blob
					else
						cmc.blobs[cmc.neu][j].ignore = 1;
					cmc.blobs[cmc.neu][j].group = NULL;
				}
		}

		// overwrite blobs that are to be ignored
		uint8_t newJ = 0;
		for (j=0; j<cmc.J; j++)
		{
			uint8_t ignore = cmc.blobs[cmc.neu][j].ignore;

			if (newJ != j)
			{
				cmc.blobs[cmc.neu][newJ].sid = cmc.blobs[cmc.neu][j].sid;
				cmc.blobs[cmc.neu][newJ].uid = cmc.blobs[cmc.neu][j].uid;
				cmc.blobs[cmc.neu][newJ].group = cmc.blobs[cmc.neu][j].group;
				cmc.blobs[cmc.neu][newJ].x = cmc.blobs[cmc.neu][j].x;
				cmc.blobs[cmc.neu][newJ].p = cmc.blobs[cmc.neu][j].p;
				cmc.blobs[cmc.neu][newJ].above_thresh = cmc.blobs[cmc.neu][j].above_thresh;
				cmc.blobs[cmc.neu][newJ].ignore = cmc.blobs[cmc.neu][j].ignore;
			}

			if (!ignore)
				newJ++;
		}
		cmc.J = newJ;

		// relate blobs to groups
		for (j=0; j<cmc.J; j++)
		{
			CMC_Blob *tar = &cmc.blobs[cmc.neu][j];

			uint8_t g;
			for (g=0; g<cmc.n_groups; g++)
			{
				CMC_Group *ptr = &cmc.groups[g];

				if ( ( (tar->uid & ptr->uid) == tar->uid) && (tar->x >= ptr->x0) && (tar->x <= ptr->x1) ) //TODO inclusive/exclusive?
				{
					if (tar->group && (tar->group != ptr) ) // give it a new sid when group has changed since last step
						tar->sid = ++(cmc.sid);

					tar->group = ptr;
					break; // match found, do not search further
				}
			}
		}
	}
	else // cmc.I == cmc.J == 0
	{
		changed = 0;
		idle_word++;
	}

	// handler idle counters
	uint8_t idle = 0;
	if (idle_bit < config.pacemaker)
	{
		idle = idle_word == (1 << idle_bit);
		if (idle)
			idle_bit++;
	}
	else // handle pacemaker
	{
		idle = idle_word == (1 << idle_bit);
		if (idle)
			idle_word = 0;
	}

	// switch blob buffers
	cmc.old = !cmc.old;
	cmc.neu = !cmc.neu;
	cmc.I = cmc.J;

	// only increase frame count when there were changes or idle overflowed
	uint8_t res = changed || idle;
	if (res)
		++(cmc.fid);

	return res;
}

//TODO get an array of cb functions when more than one engine is used
void
cmc_engine_update (uint64_t now, CMC_Engine_Frame_Cb frame_cb, CMC_Engine_Token_Cb token_cb)
{
	uint8_t j;
	uint16_t size;

	if (frame_cb)
		frame_cb (cmc.fid, now, cmc.I);

	if (token_cb)
		for (j=0; j<cmc.I; j++)
		{
			CMC_Group *group = cmc.blobs[cmc.old][j].group;
			fix_0_32_t X = cmc.blobs[cmc.old][j].x;
			uint16_t tid = 0;

			// resize x to group boundary
			// TODO check for x0 and m, whether a calculation is needed in the first place, e.g. 0, 1
			if (group->tid != 0)
			{
				X = (X - group->x0)*group->m;
				tid = group->tid;
			}

			token_cb (j,
				cmc.blobs[cmc.old][j].sid,
				cmc.blobs[cmc.old][j].uid,
				tid,
				X,
				cmc.blobs[cmc.old][j].p);
		}
}

void 
cmc_group_clear ()
{
	cmc.n_groups = 0;
	cmc_group_add (0, CMC_BOTH, 0.0, 1.0);
}

uint8_t
cmc_group_add (uint16_t tid, uint16_t uid, float x0, float x1)
{
	uint8_t i;
	for (i=cmc.n_groups; i>0; i--)
	{
		CMC_Group *grp = &cmc.groups[i-1];
		CMC_Group *grp2 = &cmc.groups[i];

		grp2->tid = grp->tid;
		grp2->uid = grp->uid;
		grp2->x0 = grp->x0;
		grp2->x1 = grp->x1;
		grp2->m = grp->m;
	}

	CMC_Group *grp = &cmc.groups[0];

	grp->tid = tid; //TODO check whether this id already exists
	grp->uid = uid;
	grp->x0 = x0;
	grp->x1 = x1;
	grp->m = 1.0ur/(x1-x0);

	cmc.n_groups++;

	return 1;
}

uint8_t
cmc_group_set (uint16_t tid, uint16_t uid, float x0, float x1)
{
	uint8_t i;
	for (i=0; i<cmc.n_groups; i++)
	{
		CMC_Group *grp = &cmc.groups[i];

		if (grp->tid == tid)
		{
			grp->uid = uid;
			grp->x0 = x0;
			grp->x1 = x1;
			grp->m = 1.0uk/(x1-x0);

			break;
		}
	}

	return 1;
}

uint8_t 
cmc_group_del (uint16_t tid)
{
	uint8_t i, j;
	for (i=j=0; i<cmc.n_groups; i++,j++)
	{
		CMC_Group *grp = &cmc.groups[i];

		if (grp->tid == tid)
			j++; // overwrite it

		if (i != j)
		{
			CMC_Group *grp2 = &cmc.groups[j];
			grp->tid = grp2->tid;
			grp->uid = grp2->uid;
			grp->x0 = grp2->x0;
			grp->x1 = grp2->x1;
			grp->m = grp2->m;
		}
	}

	cmc.n_groups -= j-i;

	return 1;
}

uint8_t *
cmc_group_buf_get (uint8_t *size)
{
	*size = cmc.n_groups * sizeof (CMC_Group);
	return (uint8_t *)cmc.groups;
}

uint8_t *
cmc_group_buf_set (uint8_t size)
{
	cmc.n_groups = size / sizeof (CMC_Group);
	return (uint8_t *)cmc.groups;
}
