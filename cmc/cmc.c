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

#include "cmc_private.h"

#include <math.h>
#include <string.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <calibration.h>

// engines
#include <tuio2.h>
#include <tuio1.h>
#include <scsynth.h>
#include <oscmidi.h>
#include <dummy.h>
#include <rtpmidi.h>

#define CMC_NOSCALE 0.0f

// globals
CMC_Engine *engines [ENGINE_MAX+1];
uint_fast8_t cmc_engines_active = 0;
CMC_Group cmc_groups[GROUP_MAX];

// locals
static uint16_t idle_word = 0;
static uint8_t idle_bit = 0;

static uint_fast8_t n_aoi;
static uint8_t aoi[BLOB_MAX*13]; //TODO how big? BLOB_MAX * 5,7,9 ?

static uint_fast8_t n_peaks;
static uint8_t peaks[BLOB_MAX];

static CMC_Blob *cmc_old;
static CMC_Blob *cmc_neu;

static uint_fast8_t I, J;
static uint_fast8_t old, neu;

static uint32_t fid, sid;

static float d, d_2;

static float vx[SENSOR_N+2];
static float vy[SENSOR_N+2];
static uint8_t vn[SENSOR_N+2];
static uint8_t va[SENSOR_N+2];

static CMC_Blob blobs[2][BLOB_MAX];
static uint8_t pacemaker = 0x0b; // pacemaker rate 2^11=2048

void
cmc_init ()
{
	uint_fast8_t i;

	I = 0;
	J = 0;
	fid = 0; // we start counting at 1, 0 marks an 'out of order' frame
	sid = 0; // we start counting at 0

	d = 1.0 / (float)SENSOR_N;
	d_2 = d / 2.0;

	for (i=0; i<SENSOR_N+2; i++)
	{
		vx[i] = d * i - d_2; //TODO caution: sensors[0] and sensors[145] are <0 & >1
		vy[i] = 0;
		vn[i] = 0;
	}

	cmc_group_clear ();

	old = 0;
	neu = 1;

	cmc_old = blobs[old];
	cmc_neu = blobs[neu];
}

uint_fast8_t __CCM_TEXT__
cmc_process (nOSC_Timestamp now, nOSC_Timestamp offset, int16_t *rela, CMC_Engine **engines)
{
	/*
	 * find areas of interest
	 */
	n_aoi = 0;
	uint_fast8_t pos;
	for (pos=0; pos<SENSOR_N; pos++)
	{
		uint_fast8_t newpos = pos+1;
		int16_t val = rela[pos];
		uint16_t aval = abs (val);
		if ( (aval << 1) > range.thresh[pos] ) // aval > thresh / 2, FIXME make this configurable!!
		{
			aoi[n_aoi++] = newpos;

			vn[newpos] = val < 0 ? POLE_NORTH : POLE_SOUTH;
			va[newpos] = aval > range.thresh[pos];

			float y = ((float)aval * range.U[pos]) - range.W;
			vy[newpos] = y; //FIXME get rid of cmc structure.
		}
		else
			vy[newpos] = 0.0; //FIXME neccessary?
	}

	uint_fast8_t changed = 1; //TODO actually check for changes

	/*
	 * detect peaks
	 */
	n_peaks = 0;
	uint_fast8_t up = 1;
	uint_fast8_t a;
	uint_fast8_t p1 = aoi[0];
	for (a=1; a<n_aoi; a++)
	{
		uint_fast8_t p0 = p1;
		p1 = aoi[a];

		if (up)
		{
			if (vy[p1] < vy[p0])
			{
				peaks[n_peaks++] = p0;
				up = 0;
			}
			// else up := 1
		}
		else // !up
		{
			if (vy[p1] > vy[p0]) //TODO what if two peaks are equally high?
				up = 1;
			// else up := 0
		}
	}

	/*
	 * handle peaks
	 */
	J = 0;
	uint_fast8_t p;
	for (p=0; p<n_peaks; p++)
	{
		float x, y;
		uint_fast8_t P = peaks[p];

		switch (config.interpolation.order)
		{
			case 0: // no interpolation
			{
        float y1 = vy[P];

				y1 = y1 < 0.f ? 0.f : (y1 > 1.f ? 1.f : y1);

				// lookup distance
				y1 = curve[(uint16_t)(y1*0x7FF)]; // FIXME incorporate *0x7ff into range.u and range.W

        x = vx[P]; 
        y = y1;

				break;
			}
			case 1: // linear interpolation
			{
				float x0, y0, a, b;

        float x1 = vx[P];
				float tm1 = vy[P-1];
				float y1 = vy[P];
				float tp1 = vy[P+1];

				if (tm1 >= tp1)
				{
          x0 = vx[P-1];
          y0 = tm1;
				}
				else // tp1 > tm1
				{
          x0 = vx[P+1];
          y0 = tp1;
				}

				y0 = y0 < 0.f ? 0.f : (y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f : (y1 > 1.f ? 1.f : y1);

				// lookup distance
				y0 = curve[(uint16_t)(y0*0x7FF)];
				y1 = curve[(uint16_t)(y1*0x7FF)];

        x = x1 + d * y0 / (y1+y0);
        a = (y1-y0) / (x1-x0);
        b = y0 - a*x0;
        y = a*x + b;

				break;
			}
			case 2: // quadratic, aka parabolic interpolation
			{
				float y0 = vy[P-1];
				float y1 = vy[P];
				float y2 = vy[P+1];

				y0 = y0 < 0.f ? 0.f : (y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f : (y1 > 1.f ? 1.f : y1);
				y2 = y2 < 0.f ? 0.f : (y2 > 1.f ? 1.f : y2);

//TODO use this linear lookup interpolation
// e.g. y0 = LOOKUP(y0);
#define LOOKUP(y) \
({ \
	float _y = (float)(y)*0x7ff; \
	uint32_t _b = floor(_y); \
	float _r = trunc(_y); \
	(float)(curve[_b] + _r*(curve[_b+1] - curve[_b])); \
})

				// lookup distance
				y0 = curve[(uint16_t)(y0*0x7FF)];
				y1 = curve[(uint16_t)(y1*0x7FF)];
				y2 = curve[(uint16_t)(y2*0x7FF)];

				// parabolic interpolation
				float divisor = y0 - 2.f*y1 + y2;
				//float divisor = y0 - (y1<<1) + y2;

				if (divisor == 0.f)
				{
					x = vx[P];
					y = y1;
				}
				else
				{
					float divisor_1 = 1.f / divisor;

					//float x = vx[P] + d_2*(y0 - y2) / divisor;
					x = vx[P] + d_2*(y0 - y2) * divisor_1; // multiplication instead of division

					//float y = y1; // dummy

					//float dividend = -y0*y0*0.125 + y0*y1 - y1*y1*2 + y0*y2*0.25 + y1*y2 - y2*y2*0.125; // 10 multiplications, 5 additions/subtractions
					float dividend = y0*(y1 - 0.125f*y0 + 0.25f*y2) + y2*(y1 - 0.125f*y2) - y1*y1*2.f; // 7 multiplications, 5 additions/subtractions
					//float dividend = y0*(y1 - (y0>>3) + (y2>>2)) + y2*(y1 - (y2>>3)) - y1*(y1<<1); // 3 multiplications, 4 bitshifts, 5 additions/subtractions

					//float y = dividend / divisor;
					y = dividend * divisor_1; // multiplication instead of division
				}
				break;
			}
			case 3: // cubic interpolation
			{
				float y0, y1, y2, y3, x1;

				float tm1 = vy[P-1];
				float thi = vy[P];
				float tp1 = vy[P+1];

				if (tm1 >= tp1)
				{
					x1 = vx[P-1];
					y0 = vy[P-2]; //FIXME caution
					y1 = tm1;
					y2 = thi;
					y3 = tp1;
				}
				else // tp1 > tm1
				{
					x1 = vx[P];
					y0 = tm1;
					y1 = thi;
					y2 = tp1;
					y3 = vy[P+2]; //FIXME caution
				}

				y0 = y0 < 0.f ? 0.f : (y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f : (y1 > 1.f ? 1.f : y1);
				y2 = y2 < 0.f ? 0.f : (y2 > 1.f ? 1.f : y2);
				y3 = y3 < 0.f ? 0.f : (y3 > 1.f ? 1.f : y3);

				// lookup distance
				y0 = curve[(uint16_t)(y0*0x7FF)];
				y1 = curve[(uint16_t)(y1*0x7FF)];
				y2 = curve[(uint16_t)(y2*0x7FF)];
				y3 = curve[(uint16_t)(y3*0x7FF)];

				// simple cubic splines
				//float a0 = y3 - y2 - y0 + y1;
				//float a1 = y0 - y1 - a0;
				//float a2 = y2 - y0;
				//float a3 = y1;
			
				// catmull-rom splines
				float a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
				float a1 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3;
				float a2 = -0.5f*y0 + 0.5f*y2;
				float a3 = y1;

        float A = 3.f * a0;
        float B = 2.f * a1;
        float C = a2;

				float mu;

        if (A == 0.f)
        {
          mu = 0.f; // TODO what to do here? fall back to quadratic?
        }
        else // A != 0.0
        {
          if (C == 0.f)
            mu = -B / A;
          else
          {
            float A2 = 2.f*A;
            float D = B*B - 2.f*A2*C;
            if (D < 0.f) // bad, this'd give an imaginary solution
              D = 0.f;
            else
              D = sqrtf(D); // FIXME use a lookup table
            mu = (-B - D) / A2;
          }
        }

				x = x1 + mu*d;
				float mu2 = mu*mu;
				y = a0*mu2*mu + a1*mu2 + a2*mu + a3;

				break;
			}
		}

		//TODO check for NaN
		x = x < 0.f ? 0.f : (x > 1.f ? 1.f : x); // 0 <= x <= 1
		y = y < 0.f ? 0.f : (y > 1.f ? 1.f : y); // 0 <= y <= 1

		CMC_Blob *blob = &cmc_neu[J];
		blob->sid = -1; // not assigned yet
		blob->pid = vn[P] == POLE_NORTH ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity (+B) magnetic fields increase the output voltage, north-polaritiy (-B) decrease it
		blob->group = NULL;
		blob->x = x;
		blob->p = y;
		blob->above_thresh = va[P];
		blob->state = CMC_BLOB_INVALID;

		J++;
	} // 50us per blob

	/*
	 * relate new to old blobs
	 */
	uint_fast8_t i, j;
	if (I || J)
	{
		idle_word = 0;
		idle_bit = 0;

		switch ( (J > I) - (J < I) ) // == signum (J-I)
		{
			case -1: // old blobs have disappeared
			{
				uint_fast8_t n_less = I - J; // how many blobs have disappeared
				i = 0;
				for (j=0; j<J; )
				{
					float diff0, diff1;

					if (n_less)
					{
						diff0 = fabs (cmc_neu[j].x - cmc_old[i].x); //TODO use assembly for fabs?
						diff1 = fabs (cmc_neu[j].x - cmc_old[i+1].x);
					}

					if ( n_less && (diff1 < diff0) )
					{
						cmc_old[i].state = CMC_BLOB_DISAPPEARED;

						n_less--;
						i++;
						// do not increase j
					}
					else
					{
						cmc_neu[j].sid = cmc_old[i].sid;
						cmc_neu[j].group = cmc_old[i].group;
						cmc_neu[j].state = CMC_BLOB_EXISTED;

						i++;
						j++;
					}
				}

				// if (n_less)
				for (i=I - n_less; i<I; i++)
					cmc_old[i].state = CMC_BLOB_DISAPPEARED;

				break;
			}

			case 0: // there has been no change in blob number, so we can relate the old and new lists 1:1 as they are both ordered according to x
			{
				for (j=0; j<J; j++)
				{
					cmc_neu[j].sid = cmc_old[j].sid;
					cmc_neu[j].group = cmc_old[j].group;
					cmc_neu[j].state = CMC_BLOB_EXISTED;
				}

				break;
			}

			case 1: // new blobs have appeared
			{
				uint_fast8_t n_more = J - I; // how many blobs have appeared
				j = 0;
				for (i=0; i<I; )
				{
					float diff0, diff1;
					
					if (n_more) // only calculate differences when there are still new blobs to be found
					{
						diff0 = fabs (cmc_neu[j].x - cmc_old[i].x);
						diff1 = fabs (cmc_neu[j+1].x - cmc_old[i].x);
					}

					if ( n_more && (diff1 < diff0) ) // cmc_neu[j] is the new blob
					{
						if (cmc_neu[j].above_thresh) // check whether it is above threshold for a new blob
						{
							cmc_neu[j].sid = ++(sid); // this is a new blob
							cmc_neu[j].group = NULL;
							cmc_neu[j].state = CMC_BLOB_APPEARED;
						}
						else
							cmc_neu[j].state = CMC_BLOB_IGNORED;

						n_more--;
						j++;
						// do not increase i
					}
					else // 1:1 relation
					{
						cmc_neu[j].sid = cmc_old[i].sid;
						cmc_neu[j].group = cmc_old[i].group;
						cmc_neu[j].state = CMC_BLOB_EXISTED;
						j++;
						i++;
					}
				}

				//if (n_more)
				for (j=J - n_more; j<J; j++)
				{
					if (cmc_neu[j].above_thresh) // check whether it is above threshold for a new blob
					{
						cmc_neu[j].sid = ++(sid); // this is a new blob
						cmc_neu[j].group = NULL;
						cmc_neu[j].state = CMC_BLOB_APPEARED;
					}
					else
						cmc_neu[j].state = CMC_BLOB_IGNORED;
				}

				break;
			}
		}

		/*
		 * overwrite blobs that are to be ignored
		 */
		uint_fast8_t newJ = 0;
		for (j=0; j<J; j++)
		{
			uint_fast8_t ignore = cmc_neu[j].state == CMC_BLOB_IGNORED;

			//FIXME remove duplicate instructions
			if (newJ != j)
				memmove (&cmc_neu[newJ], &cmc_neu[j], sizeof(CMC_Blob));

			if (!ignore)
				newJ++;
		}
		J = newJ;

		/*
		 * relate blobs to groups
		 */
		for (j=0; j<J; j++)
		{
			CMC_Blob *tar = &cmc_neu[j];

			uint16_t gid;
			for (gid=0; gid<GROUP_MAX; gid++)
			{
				CMC_Group *ptr = &cmc_groups[gid];

				if ( ( (tar->pid & ptr->pid) == tar->pid) && (tar->x >= ptr->x0) && (tar->x <= ptr->x1) ) //TODO inclusive/exclusive?
				{
					if (tar->group && (tar->group != ptr) ) // give it a new sid when group has changed since last step
					{
						// mark old blob as DISAPPEARED
						for (i=0; i<I; i++)
							if (cmc_old[i].sid == tar->sid)
							{
								cmc_old[i].state = CMC_BLOB_DISAPPEARED;
								break;
							}

						// mark new blob as APPEARED and give it a new sid
						tar->sid = ++(sid);
						tar->state = CMC_BLOB_APPEARED;
					}

					tar->group = ptr;

					if ( (ptr->m != CMC_NOSCALE) && ( (ptr->x0 != 0.0) || (ptr->m != 1.0) ) ) // we need scaling
						tar->x = (tar->x - ptr->x0) * ptr->m;

					break; // match found, do not search further
				}
			}
		}
	}
	else // I == J == 0
	{
		changed = 0;
		idle_word++;
	}

	/*
	 * (not)advance idle counters
	 */
	uint8_t idle = 0;
	if (idle_bit < pacemaker)
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
	uint_fast8_t res = changed || idle;
	if (res)
	{
		++(fid);

		uint_fast8_t e;
		for (e=0; e<ENGINE_MAX; e++)
		{
			CMC_Engine *engine;
			
			if ( !(engine = engines[e]) ) // terminator reached
				break;

			if (engine->frame_cb)
				engine->frame_cb (fid, now, offset, I, J);

			if (engine->on_cb || engine->set_cb)
				for (j=0; j<J; j++)
				{
					CMC_Blob *tar = &cmc_neu[j];
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

			//if (engine->off_cb && (I > J) ) //FIXME I and J can be equal with different SIDs
			if (engine->off_cb)
				for (i=0; i<I; i++)
				{
					CMC_Blob *tar = &cmc_old[i];
					if (tar->state == CMC_BLOB_DISAPPEARED)
						engine->off_cb (tar->sid, tar->group->gid, tar->pid);
				}
		}
	}

	/*
	 * switch blob buffers
	 */
	old = !old;
	neu = !neu;
	I = J;

	cmc_old = blobs[old];
	cmc_neu = blobs[neu];

	return res;
}

void 
cmc_group_clear ()
{
	uint16_t gid;
	for (gid=0; gid<GROUP_MAX; gid++)
	{
		CMC_Group *grp = &cmc_groups[gid];

		grp->gid = gid;
		grp->pid = CMC_BOTH;
		grp->x0 = 0.0;
		grp->x1 = 1.0;
		grp->m = CMC_NOSCALE;
	}
}

uint_fast8_t
cmc_group_get (uint16_t gid, uint16_t *pid, float *x0, float *x1, uint_fast8_t *scale)
{
	if(gid >= GROUP_MAX)
		return 0;

	CMC_Group *grp = &cmc_groups[gid];

	*pid = grp->pid;
	*x0 = grp->x0;
	*x1 = grp->x1;
	*scale = grp->m == CMC_NOSCALE ? 0 : 1;

	return 1;
}

uint_fast8_t
cmc_group_set (uint16_t gid, uint16_t pid, float x0, float x1, uint_fast8_t scale)
{
	if( (gid >= GROUP_MAX) || (x0 < 0.f) || (x0 > 1.f) || (x1 < 0.f) || (x1 > 1.f) )
		return 0; // wrong parameter ranges

	CMC_Group *grp = &cmc_groups[gid];

	grp->pid = pid;
	grp->x0 = x0;
	grp->x1 = x1;
	grp->m = scale ? 1.f/(x1-x0) : CMC_NOSCALE;

	return 1;
}

void
cmc_engines_update ()
{
	cmc_engines_active = 0;

	if (config.tuio2.enabled)
		engines[cmc_engines_active++] = &tuio2_engine;

	if (config.tuio1.enabled)
		engines[cmc_engines_active++] = &tuio1_engine;

	if (config.scsynth.enabled)
		engines[cmc_engines_active++] = &scsynth_engine;

	if (config.oscmidi.enabled)
		engines[cmc_engines_active++] = &oscmidi_engine;

	if (config.dummy.enabled)
		engines[cmc_engines_active++] = &dummy_engine;

	if (config.rtpmidi.enabled)
		engines[cmc_engines_active++] = &rtpmidi_engine;

	engines[cmc_engines_active] = NULL;
}
