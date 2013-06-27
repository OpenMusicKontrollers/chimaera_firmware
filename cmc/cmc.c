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

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

// engines
#include <tuio2.h>
#include <scsynth.h>
#include <oscmidi.h>
#include <rtpmidi.h>

uint16_t idle_word = 0;
uint8_t idle_bit = 0;
CMC cmc;

uint8_t n_aoi;
uint8_t aoi[BLOB_MAX*13]; //TODO how big? BLOB_MAX * 5,7,9 ?

uint8_t n_peaks;
uint8_t peaks[BLOB_MAX];

const char *none_str = "none";

CMC_Engine *engines [ENGINE_MAX+1];

void
cmc_init ()
{
	uint8_t i;

	cmc.I = 0;
	cmc.J = 0;
	cmc.fid = 0; // we start counting at 1, 0 marks an 'out of order' frame
	cmc.sid = 0; // we start counting at 0

	cmc.d = 1.0 / (float)SENSOR_N;
	cmc.d_2 = cmc.d / 2.0;

	for (i=0; i<SENSOR_N+2; i++)
	{
		cmc.x[i] = cmc.d * i - cmc.d_2; //TODO caution: sensors[0] and sensors[145] are <0 & >1
		cmc.v[i] = 0;
		cmc.n[i] = 0;
	}

	cmc_group_clear ();

	cmc.old = 0;
	cmc.neu = 1;
}

uint8_t
cmc_process (nOSC_Timestamp now, int16_t *rela, CMC_Engine **engines)
{
	/*
	 * find areas of interest
	 */

	n_aoi = 0;
	uint8_t pos;
	for (pos=0; pos<SENSOR_N; pos++)
	{
		int16_t val = rela[pos];
		uint16_t aval = abs (val);
		if ( (aval << 2) > range.thresh[pos] ) // thresh0 == thresh1 / 4, TODO make this configurable?
		{
			uint8_t newpos = pos+1;
			aoi[n_aoi++] = newpos;

			cmc.n[newpos] = val < 0 ? POLE_NORTH : POLE_SOUTH;
			cmc.a[newpos] = aval > range.thresh[pos];

			//float b = (float)aval * range.as_1[pos]; // calculate magnetic field, aval == |raw(i) - qui(i)|
			//cmc.v[newpos] = (b - range.bmin) * range.sc_1; // rescale to [0,1]

			// this is the same as the above, just in one function
			float y = ((float)aval * range.as_1_sc_1[pos]) - range.bmin_sc_1;
			cmc.v[newpos] = y; //FIXME get rid of cmc structure.
		}
		else
			cmc.v[pos+1] = 0.0; //FIXME necessary?
	}

	uint8_t changed = 1; //TODO actually check for changes

	/*
	 * detect peaks
	 */
	n_peaks = 0;
	uint8_t up = 1;
	uint8_t a;
	for (a=1; a<n_aoi; a++)
	{
		uint8_t p0 = aoi[a-1]; //TODO optimize
		uint8_t p1 = aoi[a]; //TODO optimize

		/*
		if (p1 > p0+1) // new local AOI
		{
			if (up)
				peaks[n_peaks++] = p0;
			else
				up = 1;
			continue; //FIXME
		}
		*/

		if (up)
		{
			if (cmc.v[p1] < cmc.v[p0])
			{
				peaks[n_peaks++] = p0;
				up = 0;
			}
			// else up := 1
		}
		else // !up
		{
			if (cmc.v[p1] > cmc.v[p0]) //TODO what if two peaks are equally high?
				up = 1;
			// else up := 0
		}
	}

	/*
	 * handle peaks
	 */
	cmc.J = 0;
	uint8_t p;
	for (p=0; p<n_peaks; p++)
	{
		float x, y;
		uint8_t P = peaks[p];

		switch (config.interpolation.order)
		{
			case 0: // no interpolation
			{
				//TODO
				break;
			}
			case 1: // linear interpolation
			{
				//TODO
				break;
			}
			case 2: // quadratic, aka parabolic interpolation
			{
				float y0 = cmc.v[P-1];
				float y1 = cmc.v[P];
				float y2 = cmc.v[P+1];

				if (y0 < 0.0) y0 = 0.0;
				if (y0 > 1.0) y0 = 1.0;
				if (y1 < 0.0) y1 = 0.0;
				if (y1 > 1.0) y1 = 1.0;
				if (y2 < 0.0) y2 = 0.0;
				if (y2 > 1.0) y2 = 1.0;

				// lookup square root
				float sqrt0 = lookup_sqrt[(uint16_t)(y0*0x7FF)];
				float sqrt1 = lookup_sqrt[(uint16_t)(y1*0x7FF)];
				float sqrt2 = lookup_sqrt[(uint16_t)(y2*0x7FF)];

				y0 = config.curve.A * sqrt0 + config.curve.B * y0; // + config.curve.C;
				y1 = config.curve.A * sqrt1 + config.curve.B * y1; // + config.curve.C;
				y2 = config.curve.A * sqrt2 + config.curve.B * y2; // + config.curve.C;

				// parabolic interpolation
				float divisor = y0 - 2.0*y1 + y2;
				//float divisor = y0 - (y1<<1) + y2;

				if (divisor == 0.0)
				{
					x = cmc.x[P];
					y = y1;
				}
				else
				{
					float divisor_1 = 1.0 / divisor;

					//float x = cmc.x[P] + cmc.d_2*(y0 - y2) / divisor;
					x = cmc.x[P] + cmc.d_2*(y0 - y2) * divisor_1; // multiplication instead of division

					//float y = y1; // dummy

					//float dividend = -y0*y0*0.125 + y0*y1 - y1*y1*2 + y0*y2*0.25 + y1*y2 - y2*y2*0.125; // 10 multiplications, 5 additions/subtractions
					float dividend = y0*(y1 - 0.125*y0 + 0.25*y2) + y2*(y1 - 0.125*y2) - y1*y1*2.0; // 7 multiplications, 5 additions/subtractions
					//float dividend = y0*(y1 - (y0>>3) + (y2>>2)) + y2*(y1 - (y2>>3)) - y1*(y1<<1); // 3 multiplications, 4 bitshifts, 5 additions/subtractions

					//float y = dividend / divisor;
					y = dividend * divisor_1; // multiplication instead of division
				}
				break;
			}
			case 3: // cubic interpolation
			{
				float y0, y1, y2, y3, x1;

				float tm1 = cmc.v[P-1];
				float thi = cmc.v[P];
				float tp1 = cmc.v[P+1];

				if (tm1 >= tp1)
				{
					x1 = cmc.x[P-1];
					y0 = cmc.v[P-2]; //FIXME caution
					y1 = tm1;
					y2 = thi;
					y3 = tp1;
				}
				else // tp1 > tm1
				{
					x1 = cmc.x[P];
					y0 = tm1;
					y1 = thi;
					y2 = tp1;
					y3 = cmc.v[P+2]; //FIXME caution
				}

				/*
				if (y0 < 0.0) y0 = 0.0;
				if (y0 > 1.0) y0 = 1.0;
				if (y1 < 0.0) y1 = 0.0;
				if (y1 > 1.0) y1 = 1.0;
				if (y2 < 0.0) y2 = 0.0;
				if (y2 > 1.0) y2 = 1.0;
				if (y3 < 0.0) y3 = 0.0;
				if (y3 > 1.0) y3 = 1.0;

				// lookup square root
				float sqrt0 = lookup_sqrt[(uint16_t)(y0*0x7FF)];
				float sqrt1 = lookup_sqrt[(uint16_t)(y1*0x7FF)];
				float sqrt2 = lookup_sqrt[(uint16_t)(y2*0x7FF)];
				float sqrt3 = lookup_sqrt[(uint16_t)(y3*0x7FF)];

				y0 = config.curve.A * sqrt0 + config.curve.B * y0; // + config.curve.C;
				y1 = config.curve.A * sqrt1 + config.curve.B * y1; // + config.curve.C;
				y2 = config.curve.A * sqrt2 + config.curve.B * y2; // + config.curve.C;
				y3 = config.curve.A * sqrt3 + config.curve.B * y3; // + config.curve.C;

				// simple cubic splines
				//float a0 = y3 - y2 - y0 + y1;
				//float a1 = y0 - y1 - a0;
				//float a2 = y2 - y0;
				//float a3 = y1;
			
				// catmull-rom splines
				float a0 = -0.5*y0 + 1.5*y1 - 1.5*y2 + 0.5*y3;
				float a1 = y0 - 2.5*y1 + 2.0*y2 - 0.5*y3;
				float a2 = -0.5*y0 + 0.5*y2;
				float a3 = y1;

				float mu;
				if (a0 == 0.0)
				{
					mu = -0.5 * a2 / a1;
				}
				else
				{
					float _3a0 = 3.0 * a0;
					float s = a1*a1 - _3a0*a2;
					if (s < 0.0)
						s = 0.0;
					else
						s = sqrt(s); //TODO sqrt(0:4) := 2*sqrt(0:1) -> lookup_sqrt

					mu = (-a1 - s) / _3a0;

					//TODO mu must be [0:1]
				}

				x = x1 + mu*cmc.d;
				float mu2 = mu*mu;
				y = a0*mu2*mu + a1*mu2 + a2*mu + a3;
				*/

				x = x1;
				y = y1;
				break;
			}
		}

		if (x < 0.0) x = 0.0;
		if (x > 1.0) x = 1.0;
		if (y < 0.0) y = 0.0;
		if (y > 1.0) y = 1.0;

		cmc.blobs[cmc.neu][cmc.J].sid = -1; // not assigned yet
		cmc.blobs[cmc.neu][cmc.J].pid = cmc.n[P] == POLE_NORTH ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity (+B) magnetic fields increase the output voltage, north-polaritiy (-B) decrease it
		cmc.blobs[cmc.neu][cmc.J].group = NULL;
		cmc.blobs[cmc.neu][cmc.J].x = x;
		cmc.blobs[cmc.neu][cmc.J].p = y;
		cmc.blobs[cmc.neu][cmc.J].above_thresh = cmc.a[P];
		cmc.blobs[cmc.neu][cmc.J].state = CMC_BLOB_INVALID;

		cmc.J++;
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
					float diff0, diff1;

					if (n_less)
					{
						diff0 = fabs (cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i].x); //TODO use assembly for fabs?
						diff1 = fabs (cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i+1].x);
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
					float diff0, diff1;
					
					if (n_more) // only calculate differences when there are still new blobs to be found
					{
						diff0 = fabs (cmc.blobs[cmc.neu][j].x - cmc.blobs[cmc.old][i].x);
						diff1 = fabs (cmc.blobs[cmc.neu][j+1].x - cmc.blobs[cmc.old][i].x);
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

		/*
		 * overwrite blobs that are to be ignored
		 */
		uint8_t newJ = 0;
		for (j=0; j<cmc.J; j++)
		{
			uint8_t ignore = cmc.blobs[cmc.neu][j].state == CMC_BLOB_IGNORED;

			if (newJ != j)
				memmove (&cmc.blobs[cmc.neu][newJ], &cmc.blobs[cmc.neu][j], sizeof(CMC_Blob));

			if (!ignore)
				newJ++;
		}
		cmc.J = newJ;

		/*
		 * relate blobs to groups
		 */
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
					{
						// mark old blob as DISAPPEARED
						for (i=0; i<cmc.I; i++)
							if (cmc.blobs[cmc.old][i].sid == tar->sid)
							{
								cmc.blobs[cmc.old][i].state = CMC_BLOB_DISAPPEARED;
								break;
							}

						// mark new blob as APPEARED and give it a new sid
						tar->sid = ++(cmc.sid);
						tar->state = CMC_BLOB_APPEARED;
					}

					tar->group = ptr;

					if ( (ptr->x0 != 0.0) || (ptr->m != 1.0) ) // we need scaling
						tar->x = (tar->x - ptr->x0) * ptr->m;

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

	/*
	 * (not)advance idle counters
	 */
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

	/*
	 * handle output engines
	 * FIXME check loop structure for efficiency
	 */
	uint8_t res = changed || idle;
	if (res)
	{
		++(cmc.fid);

		uint8_t e;
		for (e=0; e<ENGINE_MAX; e++)
		{
			CMC_Engine *engine;
			
			if ( !(engine = engines[e]) ) // terminator reached
				break;

			if (engine->frame_cb)
				engine->frame_cb (cmc.fid, now, cmc.I, cmc.J);

			if (engine->on_cb || engine->set_cb)
				for (j=0; j<cmc.J; j++)
				{
					CMC_Blob *tar = &cmc.blobs[cmc.neu][j];
					if (tar->state == CMC_BLOB_APPEARED)
					{
						if (engine->on_cb)
							engine->on_cb (tar->sid, tar->group->gid, tar->pid, tar->x, tar->p);
					}
					else // tar->state == CMC_BLOB_EXISTED
					{
						if (engine->set_cb)
							engine->set_cb (tar->sid, tar->group->gid, tar->pid, tar->x, tar->p);
					}
				}

			//if (engine->off_cb && (cmc.I > cmc.J) ) //FIXME I and J can be equal with different SIDs
			if (engine->off_cb)
				for (i=0; i<cmc.I; i++)
				{
					CMC_Blob *tar = &cmc.blobs[cmc.old][i];
					if (tar->state == CMC_BLOB_DISAPPEARED)
						engine->off_cb (tar->sid, tar->group->gid, tar->pid);
				}
		}
	}

	/*
	 * switch blob buffers
	 */
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
		grp->x0 = 0.0;
		grp->x1 = 1.0;
		grp->m = 1.0;
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
	grp->m = 1.0/(x1-x0);

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

void
cmc_engines_update ()
{
	uint8_t ptr = 0;

	if (config.tuio.enabled)
		engines[ptr++] = &tuio2_engine;

	if (config.scsynth.enabled)
		engines[ptr++] = &scsynth_engine;

	if (config.oscmidi.enabled)
		engines[ptr++] = &oscmidi_engine;

	if (config.rtpmidi.enabled)
		engines[ptr++] = &rtpmidi_engine;

	engines[ptr] = NULL;
}
