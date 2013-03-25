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

uint16_t idle_word = 0;
uint8_t idle_bit = 0;
CMC cmc;

#define THRESH_MIN 30 // absolute minimum threshold FIXME make this configurable

uint8_t n_aoi;
uint8_t aoi[64]; //TODO how big?

uint8_t n_peaks;
uint8_t peaks[BLOB_MAX];

const char *none_str = "none";

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

	cmc_group_clear ();

	cmc.old = 0;
	cmc.neu = 1;
}

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
cmc_process (uint64_t now, int16_t *rela, CMC_Engine **engines)
{
	n_aoi = 0;
	uint8_t pos;
	for (pos=0; pos<SENSOR_N; pos++)
	{
		int16_t val = rela[pos];
		uint16_t aval = abs (val);
		uint8_t pole = val < 0 ? POLE_NORTH : POLE_SOUTH;
		uint8_t newpos = pos+1;
		if ( (aval > THRESH_MIN) && (aval*4 > range.thresh[pos]) ) // thresh0 == thresh1 / 4
		{
			aoi[n_aoi++] = newpos;

			cmc.n[newpos] = pole;
			cmc.a[newpos] = aval > range.thresh[pos];

			//TODO merge this two functions into a single one
			fix_16_16_t b = (fix_16_16_t)aval * range.as_1[pos]; // calculate magnetic field, aval == |raw(i) - qui(i)|
			cmc.v[newpos] = (b - range.bmin) * range.sc_1; // rescale to [0,1]
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

		//TODO do curvefit instead of simple sqrt
		//y0 = A*y0 + B*sqrt(y0) + C;
		y0 = sqrt (y0);
		y1 = sqrt (y1);
		y2 = sqrt (y2);

		// parabolic interpolation
		//fix_s_31_t divisor = y0 - y1 + y2 - y1;
		fix_s31_32_t divisor = y0 - 2*y1 + y2;
		fix_0_32_t x = cmc.x[P] + cmc.d_2*(y0 - y2) / divisor;

		//fix_0_32_t y = y1; // this is better, simpler and faster than any interpolation (just KISS)

		fix_s31_32_t dividend = -y0*y0*0.125LR + y0*y1 - y1*y1*2 + y0*y2*0.25LR + y1*y2 - y2*y2*0.125LR;
		fix_0_32_t y = dividend / divisor;

		if ( (x > 0.0LR) && (y > 0.0LR) ) //FIXME why can this happen in the first place?
		{
			cmc.blobs[cmc.neu][cmc.J].sid = -1; // not assigned yet
			cmc.blobs[cmc.neu][cmc.J].pid = cmc.n[P] == POLE_NORTH ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity (+B) magnetic fields increase the output voltage, north-polaritiy (-B) decrease it
			cmc.blobs[cmc.neu][cmc.J].group = NULL;
			cmc.blobs[cmc.neu][cmc.J].x = x;
			cmc.blobs[cmc.neu][cmc.J].p = y;
			cmc.blobs[cmc.neu][cmc.J].above_thresh = cmc.a[P];
			cmc.blobs[cmc.neu][cmc.J].state = CMC_BLOB_INVALID;

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

		switch ( (cmc.J > cmc.I) - (cmc.J < cmc.I) ) // == signum (cmc.J-cmc.I)
		{
			case -1: // old blobs have disappeared
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
						cmc.blobs[cmc.old][i].state = CMC_BLOB_DISAPPEARED;

						n_less--;
						i++;
						// do not increase j
					}
					else
					{
						cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][i].sid;
						cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][i].group;
						cmc.blobs[cmc.neu][j].state = CMC_BLOB_EXISTED;

						i++;
						j++;
					}
				}

				// if (n_less)
				for (i=cmc.I - n_less; i<cmc.I; i++)
					cmc.blobs[cmc.old][i].state = CMC_BLOB_DISAPPEARED;

				break;
			}

			case 0: // there has been no change in blob number, so we can relate the old and new lists 1:1 as they are both ordered according to x
			{
				for (j=0; j<cmc.J; j++)
				{
					cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][j].sid;
					cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][j].group;
					cmc.blobs[cmc.neu][j].state = CMC_BLOB_EXISTED;
				}

				break;
			}

			case 1: // new blobs have appeared
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
						{
							cmc.blobs[cmc.neu][j].sid = ++(cmc.sid); // this is a new blob
							cmc.blobs[cmc.neu][j].group = NULL;
							cmc.blobs[cmc.neu][j].state = CMC_BLOB_APPEARED;
						}
						else
							cmc.blobs[cmc.neu][j].state = CMC_BLOB_IGNORED;

						n_more--;
						j++;
						// do not increase i
					}
					else // 1:1 relation
					{
						cmc.blobs[cmc.neu][j].sid = cmc.blobs[cmc.old][i].sid;
						cmc.blobs[cmc.neu][j].group = cmc.blobs[cmc.old][i].group;
						cmc.blobs[cmc.neu][j].state = CMC_BLOB_EXISTED;
						j++;
						i++;
					}
				}

				//if (n_more)
				for (j=cmc.J - n_more; j<cmc.J; j++)
				{
					if (cmc.blobs[cmc.neu][j].above_thresh) // check whether it is above threshold for a new blob
					{
						cmc.blobs[cmc.neu][j].sid = ++(cmc.sid); // this is a new blob
						cmc.blobs[cmc.neu][j].group = NULL;
						cmc.blobs[cmc.neu][j].state = CMC_BLOB_APPEARED;
					}
					else
						cmc.blobs[cmc.neu][j].state = CMC_BLOB_IGNORED;
				}

				break;
			}
		}

		// overwrite blobs that are to be ignored
		uint8_t newJ = 0;
		for (j=0; j<cmc.J; j++)
		{
			//uint8_t ignore = cmc.blobs[cmc.neu][j].ignore;
			uint8_t ignore = cmc.blobs[cmc.neu][j].state == CMC_BLOB_IGNORED;

			if (newJ != j)
				memmove (&cmc.blobs[cmc.neu][newJ], &cmc.blobs[cmc.neu][j], sizeof(CMC_Blob));

			if (!ignore)
				newJ++;
		}
		cmc.J = newJ;

		// relate blobs to groups
		for (j=0; j<cmc.J; j++)
		{
			CMC_Blob *tar = &cmc.blobs[cmc.neu][j];

			uint8_t gid;
			for (gid=0; gid<GROUP_MAX; gid++)
			{
				CMC_Group *ptr = &cmc.groups[gid];

				if ( ( (tar->pid & ptr->pid) == tar->pid) && (tar->x >= ptr->x0) && (tar->x <= ptr->x1) ) //TODO inclusive/exclusive?
				{
					if (tar->group && (tar->group != ptr) ) // give it a new sid when group has changed since last step
						tar->sid = ++(cmc.sid); //TODO also set state

					tar->group = ptr;
					tar->fp = tar->p;

					if ( (ptr->x0 != 0ULR) || (ptr->m != 1ULK) )
						tar->fx = (tar->x - ptr->x0) * ptr->m;
					else // no scaling
						tar->fx = tar->x;

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

	// handle idle counters
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

	// run engines here TODO check loop structure for efficiency
	uint8_t res = changed || idle;
	if (res)
	{
		++(cmc.fid);

		uint8_t e;
		for (e=0; e<ENGINE_N; e++)
		{
			CMC_Engine *engine = engines[e];
			if (*(engine->enabled))
			{
				if (engine->frame_cb)
					engine->frame_cb (cmc.fid, now, cmc.I, cmc.J);

				if (engine->on_cb || engine->set_cb)
					for (j=0; j<cmc.J; j++)
					{
						CMC_Blob *tar = &cmc.blobs[cmc.neu][j];
						if (tar->state == CMC_BLOB_APPEARED)
						{
							if (engine->on_cb)
								engine->on_cb (tar->sid, tar->group->gid, tar->pid, tar->fx, tar->fp);
						}
						else // tar->state == CMC_BLOB_EXISTED
						{
							if (engine->set_cb)
								engine->set_cb (tar->sid, tar->group->gid, tar->pid, tar->fx, tar->fp);
						}
					}

				if (engine->off_cb && (cmc.I > cmc.J) )
					for (i=0; i<cmc.I; i++)
					{
						CMC_Blob *tar = &cmc.blobs[cmc.old][i];
						if (tar->state == CMC_BLOB_DISAPPEARED)
							engine->off_cb (tar->sid, tar->group->gid, tar->pid);
					}
			}
		}
	}

	// switch blob buffers
	cmc.old = !cmc.old;
	cmc.neu = !cmc.neu;
	cmc.I = cmc.J;

	return res;
}

void 
cmc_group_clear ()
{
	uint8_t gid;
	for (gid=0; gid<GROUP_MAX; gid++)
	{
		CMC_Group *grp = &cmc.groups[gid];

		grp->gid = gid;
		grp->pid = CMC_BOTH;
		strcpy (grp->name, none_str);
		grp->x0 = 0.0ULR;
		grp->x1 = 1.0ULR;
		grp->m = 1.0ULK;
	}
}

uint8_t
cmc_group_get (uint16_t gid, char **name, uint16_t *pid, float *x0, float *x1)
{
	CMC_Group *grp = &cmc.groups[gid];

	*name = grp->name;
	*pid = grp->pid;
	*x0 = grp->x0;
	*x1 = grp->x1;

	return 1;
}

uint8_t
cmc_group_set (uint16_t gid, char *name, uint16_t pid, float x0, float x1)
{
	CMC_Group *grp = &cmc.groups[gid];

	grp->pid = pid;
	strcpy (grp->name, name);
	grp->x0 = x0;
	grp->x1 = x1;
	grp->m = 1.0ULK/(x1-x0);

	return 1;
}

char *
cmc_group_name_get (uint16_t gid)
{
	CMC_Group *grp = &cmc.groups[gid];
	return grp->name;
}

uint8_t *
cmc_group_buf_get (uint16_t *size)
{
	if (size)
		*size = GROUP_MAX * sizeof (CMC_Group);
	return (uint8_t *)cmc.groups;
}
