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

#include <math.h>
#include <string.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

#define s2d2 0.70710678118655r // sqrt(2) / 2
#define os8 0.35355339059327r // 1 / sqrt(8)

uint8_t idle = 0; // IDLE_CYCLE of 256
CMC cmc;

void
cmc_init ()
{
	uint8_t i;

	cmc.I = 0;
	cmc.J = 0;
	cmc.fid = 0;
	cmc.sid = 0;

	cmc.d = 1.0ur / (SENSOR_N-1);
	cmc.d_2 = cmc.d / 2;

	for (i=0; i<SENSOR_N+2; i++)
	{
		cmc.sensors[i].x = cmc.d * (i-1);
		cmc.sensors[i].v = 0;
		cmc.sensors[i].n = 0;
	}

	cmc.n_groups = 0;
	cmc_group_add (0, CMC_BOTH, 0.0, 1.0);

	cmc.old = 0;
	cmc.neu = 1;
}

uint8_t
cmc_process (int16_t raw[16][10], uint8_t order[16][9])
{
	uint8_t p;
	uint8_t i, j;

	// 11us
	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_LENGTH; i++) //FIXME missing sensor unit
		{
			uint8_t pos = order[p][i];
			int16_t val = raw[p][i] - adc_range[p][i].mean; //TODO handle min and max, too
			uint16_t aval = abs (val);
			if (aval > config.cmc.thresh0)
			{
				cmc.sensors[pos+1].n = val < 0;
				if (cmc.sensors[pos+1].n) // north pole
					cmc.sensors[pos+1].v = aval*adc_range[p][i].m_north; //FIXME check for overflow
				else // south pole
					cmc.sensors[pos+1].v = aval*adc_range[p][i].m_south; //FIXME check for overflow
			}
			else
				cmc.sensors[pos+1].v = 0;
		}

	fix_0_16_t m_thresh1 = (float)0x7fff / (0x7ff - config.cmc.thresh1); //TODO check value

	// 80us

	uint8_t changed = 1; //TODO actually check for changes

	/*
	 * find maxima
	 */
	cmc.J = 0;

	// look at array for blobs
	// TODO simplify this by just searching for maximums and surrounding zeroes...
	for (i=1; i<SENSOR_N+1; i++) // TODO merge the loop with the upper one ^^^^
	{
		if (cmc.sensors[i].v)
		{
			uint8_t I;
			uint8_t peak = 1;

			for (I=1; I<config.cmc.peak_thresh; I++)
			{
				if ( (i >= I) && (cmc.sensors[i].v <= cmc.sensors[i-I].v) )
				{
					peak = 0;
					break;
				}

				if ( (i+I <= SENSOR_N+1) && (cmc.sensors[i].v < cmc.sensors[i+I].v) )
				{
					peak = 0;
					break;
				}
			}

			if (peak)
			{
				//fit parabola

				int16_t p0, p1, p2;
				
				p0 = cmc.sensors[i-1].v - config.cmc.thresh1;
				if (p0 < 0) p0 = 0;
				else p0 *= m_thresh1;
				
				p1 = cmc.sensors[i].v - config.cmc.thresh1;
				if (p1 < 0) p1 = 0;
				else p1 *= m_thresh1;
				
				p2 = cmc.sensors[i+1].v - config.cmc.thresh1;
				if (p2 < 0) p2 = 0;
				else p2 *= m_thresh1;

				fix_s_31_t y0 = dist[p0];
				fix_s_31_t y1 = dist[p1];
				fix_s_31_t y2 = dist[p2];

				fix_s_31_t divisor = y0 - y1 + y2 - y1;
				fix_0_32_t x = cmc.sensors[i].x + cmc.d_2*(y0 - y2) / divisor;

				/* TODO this would be correct, but gives bad glitches
				fix_s_31_t tmp = s2d2*y1;
				fix_s_31_t _y = tmp - os8*y0 - os8*y2 + tmp;
				fix_0_32_t y = -_y*_y / divisor;
				*/

				fix_0_32_t y = y0*0.33r + y1*0.66r + y2*0.33r; // TODO this is good enough an approximation

				cmc.blobs[cmc.neu][cmc.J].sid = -1; // not assigned yet
				cmc.blobs[cmc.neu][cmc.J].uid = cmc.sensors[i].n ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity (+B) magnetic fields increase the output voltage, north-polaritiy (-B) decrease it
				cmc.blobs[cmc.neu][cmc.J].group = NULL;
				cmc.blobs[cmc.neu][cmc.J].x = x;
				cmc.blobs[cmc.neu][cmc.J].p = y;
				cmc.blobs[cmc.neu][cmc.J].above_thresh = cmc.sensors[i].v > config.cmc.thresh1 ? 1 : 0;
				cmc.blobs[cmc.neu][cmc.J].ignore = 0;

				cmc.J++;

				if (cmc.J >= BLOB_MAX) // make sure to not exceed maximal number of blobs
					break;
			}
		}
	}

	/*
	 * relate new to old blobs
	 */
	if (cmc.I || cmc.J)
	{
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
		idle++; // automatic overflow
	}

	// switch blob buffers
	cmc.old = !cmc.old;
	cmc.neu = !cmc.neu;
	cmc.I = cmc.J;

	// only increase frame count when there were changes or idle overflowed
	if (changed || !idle)
		++(cmc.fid);

	return changed || !idle;
}

uint16_t
cmc_write_tuio2 (timestamp64u_t timestamp, uint8_t *buf)
{
	uint8_t j;
	uint16_t size;

	tuio2_frm_set (cmc.fid, timestamp);
	for (j=0; j<cmc.I; j++)
	{
		CMC_Group *group = cmc.blobs[cmc.old][j].group;
		fix_0_32_t X = cmc.blobs[cmc.old][j].x;
		uint16_t tid = 0;

		// resize x to group boundary
		if (group->tid != 0)
		{
			X = (X - group->x0)*group->m;
			tid = group->tid;
		}

		tuio2_tok_set (j,
			cmc.blobs[cmc.old][j].sid,
			(cmc.blobs[cmc.old][j].uid<<16) | tid,
			X,
			cmc.blobs[cmc.old][j].p);
	}
	size = tuio2_serialize (buf, cmc.I);
	return size;
}

uint16_t 
cmc_dump_unit (timestamp64u_t timestamp, uint8_t *buf, uint8_t unit)
{
	uint8_t i;
	uint16_t size;

	nOSC_Message *msg = NULL;
	msg = nosc_message_add_timestamp (msg, timestamp);
	msg = nosc_message_add_int32 (msg, unit);
	for (i=unit*0x10; i<(unit+1)*0x10; i++)
		msg = nosc_message_add_int32 (msg, cmc.sensors[i+1].n ? -cmc.sensors[i+1].v : cmc.sensors[i+1].v);
	size = nosc_message_serialize (msg, "/cmc/dump_unit", buf);
	nosc_message_free (msg);

	return size;
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
			grp->m = 1.0ur/(x1-x0);

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
