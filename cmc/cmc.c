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

#include "cmc_private.h"

#include <series/simd.h>

#include <math.h>
#include <string.h>

#include <chimaera.h>
#include <chimutil.h>
#include <config.h>
#include <calibration.h>
#include <sensors.h>

// engines
#include <tuio2.h>
#include <tuio1.h>
#include <scsynth.h>
#include <oscmidi.h>
#include <dummy.h>
#include <custom.h>

// globals
CMC_Engine *engines [ENGINE_MAX+1];
uint_fast8_t cmc_engines_active = 0;
CMC_Group *cmc_groups = config.groups;
uint16_t cmc_groups_n = GROUP_MAX;

#ifdef BENCHMARK
Stop_Watch sw_engine_process = {.id = "engine_process", .thresh=3000};
#endif

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

static float s, sm1; // filter stiffness, (1-stiffness)

static float vx[SENSOR_N+2];
static float vy[SENSOR_N+2];
static uint8_t vn[SENSOR_N+2];
static uint8_t va[SENSOR_N+2];

static CMC_Blob blobs[2][BLOB_MAX];
static uint8_t pacemaker = 0x0b; // pacemaker rate 2^11=2048

void
cmc_velocity_stiffness_update(uint8_t stiffness)
{
	s = 1.f / (float)stiffness;
	sm1 = 1.f - s;
	s *= 0.5;
}

void
cmc_init(void)
{
	uint_fast8_t i;

	I = 0;
	J = 0;
	fid = 0; // we start counting at 1, 0 marks an 'out of order' frame
	sid = 0; // we start counting at 0

	d = 1.0 /(float)SENSOR_N;
	d_2 = d / 2.0;

	for(i=0; i<SENSOR_N+2; i++)
	{
		vx[i] = d * i - d_2; // sensors[0] and sensors[SENSOR_N+1] are <0 and >1
		vy[i] = 0;
		vn[i] = 0;
	}

	old = 0;
	neu = 1;

	cmc_old = blobs[old];
	cmc_neu = blobs[neu];

	// update group and initialize output engines
	cmc_group_update();

	// update engines stack
	cmc_engines_update();
	cmc_velocity_stiffness_update(config.sensors.velocity_stiffness);
}

#if 0
// raw lookup
static inline __always_inline float
LOOKUP(float y)
{
	uint16_t ii = y*0x7ff;
	return curve[ii];
}
#else
// linear lookup interpolation
static inline __always_inline float
LOOKUP(float y)
{
	float f, i;
	uint16_t ii;

	y *= 0x7ff;
	f = modff(y, &i); // fractional part
	ii = i; // integral part
	return curve[ii] + f*(curve[ii+1] - curve[ii]);
}
#endif

// derive velocity and acceleration signals
//static inline __always_inline void
static void __CCM_TEXT__ //FIXME
VEL_ACCEL(CMC_Blob *_old, CMC_Blob *_neu, float rate)
{
	float dx = _neu->x - _old->x;
	_neu->vx.f1 = dx * rate;

	float dy = _neu->y - _old->y;
	_neu->vy.f1 = dy * rate;

	// first-order IIR filter
	_neu->vx.f11 = s*(_neu->vx.f1 + _old->vx.f1) + _old->vx.f11*sm1;
	_neu->vy.f11 = s*(_neu->vy.f1 + _old->vy.f1) + _old->vy.f11*sm1;

	_neu->v = sqrtf(_neu->vx.f11 * _neu->vx.f11 + _neu->vy.f11 * _neu->vy.f11);

	float dv =  _neu->v - _old->v;
	_neu->m = dv * rate;
}

#define VABS(A) \
({ \
	float X; \
	asm volatile ("VABS.F32 %[res], %[val1]" \
		: [res]"=w" (X) \
		: [val1]"w" (A) \
	); \
	(float)X; \
})

static OSC_Timetag last; // timestamp of last loop

osc_data_t *__CCM_TEXT__
cmc_process(OSC_Timetag now, OSC_Timetag offset, int16_t *rela, osc_data_t *buf, osc_data_t *end)
{
	/*
	 * derive REAL update rate for velocity and acceleration calculations
	 */
	float rate;
	if(last > 0ULLK)
		rate = 1.f / (now - last);
	else
		rate = config.sensors.rate;
	last = now;

	osc_data_t *buf_ptr = buf;
	/*
	 * find areas of interest
	 */
	n_aoi = 0;
	uint_fast8_t pos;
	for(pos=0; pos<SENSOR_N; pos++)
	{
		uint_fast8_t newpos = pos+1;
		int16_t val = rela[pos];
		uint16_t aval = abs(val);
		if( (aval << 1) > range.thresh[pos] ) // aval > thresh / 2?
		{
			aoi[n_aoi++] = newpos;

			vn[newpos] = val < 0 ? POLE_NORTH : POLE_SOUTH;
			va[newpos] = aval > range.thresh[pos];

			float y =((float)aval * range.U[pos]) - range.W;
			vy[newpos] = y;
		}
		else
			vy[newpos] = 0.0;
	}

	/*
	 * detect peaks
	 */
	n_peaks = 0;
	uint_fast8_t up = 1;
	uint_fast8_t a;
	uint_fast8_t p1 = aoi[0];
	for(a=1; a<n_aoi; a++)
	{
		uint_fast8_t p0 = p1;
		p1 = aoi[a];

		if(up)
		{
			if(vy[p1] < vy[p0])
			{
				peaks[n_peaks++] = p0;
				up = 0;
			}
			// else up := 1
		}
		else // !up
		{
			if(vy[p1] > vy[p0])
				up = 1;
			// else up := 0
		}
	}

	/*
	 * handle peaks
	 */
	J = 0;
	uint_fast8_t p;
	for(p=0; p<n_peaks; p++)
	{
		float x, y;
		uint_fast8_t P = peaks[p];

		switch(config.sensors.interpolation_mode)
		{
			case INTERPOLATION_NONE: // no interpolation
			{
        float y1 = vy[P];

				y1 = y1 < 0.f ? 0.f :(y1 > 1.f ? 1.f : y1);

				// lookup distance
				y1 = LOOKUP(y1);

        x = vx[P]; 
        y = y1;

				break;
			}
			case INTERPOLATION_QUADRATIC: // quadratic, aka parabolic interpolation
			{
				float y0 = vy[P-1];
				float y1 = vy[P];
				float y2 = vy[P+1];

				y0 = y0 < 0.f ? 0.f :(y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f :(y1 > 1.f ? 1.f : y1);
				y2 = y2 < 0.f ? 0.f :(y2 > 1.f ? 1.f : y2);

				// lookup distance
				y0 = LOOKUP(y0);
				y1 = LOOKUP(y1);
				y2 = LOOKUP(y2);

				// parabolic interpolation
				float divisor = y0 - 2.f*y1 + y2;
				//float divisor = y0 -(y1<<1) + y2;

				if(divisor == 0.f)
				{
					x = vx[P];
					y = y1;
				}
				else
				{
					float divisor_1 = 1.f / divisor;
					x = vx[P] + d_2*(y0 - y2) * divisor_1; // multiplication instead of division
					float dividend = y0*(y1 - 0.125f*y0 + 0.25f*y2) + y2*(y1 - 0.125f*y2) - y1*y1*2.f; // 7 multiplications, 5 additions/subtractions
					y = dividend * divisor_1; // multiplication instead of division
				}
				break;
			}
			case INTERPOLATION_CATMULL: // cubic interpolation: Catmull-Rom splines
			{
				float y0, y1, y2, y3, x1;

				float tm1 = vy[P-1];
				float thi = vy[P];
				float tp1 = vy[P+1];

				if(tm1 >= tp1)
				{
					x1 = vx[P-1];
					//y0 = vy[P-2];
					y0 = P >= 2 ? vy[P-2] : vy[P-1]; // check for underflow
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
					//y3 = vy[P+2];
					y3 = P <= (SENSOR_N) ? vy[P+2] : vy[P+1]; // check for overflow
				}

				y0 = y0 < 0.f ? 0.f :(y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f :(y1 > 1.f ? 1.f : y1);
				y2 = y2 < 0.f ? 0.f :(y2 > 1.f ? 1.f : y2);
				y3 = y3 < 0.f ? 0.f :(y3 > 1.f ? 1.f : y3);

				// lookup distance
				y0 = LOOKUP(y0);
				y1 = LOOKUP(y1);
				y2 = LOOKUP(y2);
				y3 = LOOKUP(y3);

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

        if(A == 0.f)
        {
          mu = 0.f; // TODO what to do here? fall back to quadratic?
        }
        else // A != 0.f
        {
          if(C == 0.f)
            mu = -B / A;
          else
          {
            float A2 = 2.f*A;
            float D = B*B - 2.f*A2*C;
            if(D < 0.f) // bad, this'd give an imaginary solution
              D = 0.f;
            else
              D = sqrtf(D);
            mu =(-B - D) / A2;
          }
        }

				x = x1 + mu*d;
				float mu2 = mu*mu;
				y = a0*mu2*mu + a1*mu2 + a2*mu + a3;

				break;
			}
			case INTERPOLATION_LAGRANGE: // cubic interpolation: Lagrange Poylnomial
			{
				float y0, y1, y2, y3, x1;

				x1 = vx[P];
				y0 = vy[P-1];
				y1 = vy[P];
				y2 = vy[P+1];
				//y3 = vy[P+2];
				y3 = P <= (SENSOR_N) ? vy[P+2] : vy[P+1]; // check for overflow

				y0 = y0 < 0.f ? 0.f :(y0 > 1.f ? 1.f : y0);
				y1 = y1 < 0.f ? 0.f :(y1 > 1.f ? 1.f : y1);
				y2 = y2 < 0.f ? 0.f :(y2 > 1.f ? 1.f : y2);
				y3 = y3 < 0.f ? 0.f :(y3 > 1.f ? 1.f : y3);

				// lookup distance
				y0 = LOOKUP(y0);
				y1 = LOOKUP(y1);
				y2 = LOOKUP(y2);
				y3 = LOOKUP(y3);

				float d2 = d * d;
				float d3 = d2 * d;

				float s1 = y0 - 2.f*y1 + y2;
				float s2 = y0 - 3.f*y1 + 3.f*y2 - y3;
				float sq = y0*(y0 - 9.f*y1 + 6.f*y2 + y3) + y1*(21.f*y1 - 39.f*y2 + 6.f*y3) + y2*(21.f*y2 - 9.f*y3) + y3*y3;

				if(sq < 0.f) // bad, this'd give an imaginary solution
					sq = 0.f;

				if(s2 == 0)
					x = x1; //FIXME what to do here?
				else
					x =(3.f*d*s1 + 3.f*x1*s2 + sqrtf(3.f)*d*sqrtf(
						y0*(y0 - 9.f*y1 + 6.f*y2 + y3) + y1*(21.f*y1 - 39.f*y2 + 6.f*y3) + y2*(21.f*y2 - 9.f*y3) + y3*y3
					)) /(3.f*s2);

				float X1 = x - x1;
				float X2 = X1 * X1;
				float X3 = X2 * X1;

				y = -(-6.f*d3*y1 + d2*X1*(2.f*y0 + 3.f*y1 - 6.f*y2 + y3) - 3.f*d*X2*s1 + X3*s2) /(6.f*d3);

				break;
			}
			default:
				x = 0.f;
				y = 0.f;
				break;
		}

		//TODO check for NaN
		x = x < 0.f ? 0.f :(x > 1.f ? 1.f : x); // 0 <= x <= 1
		y = y < 0.f ? 0.f :(y > 1.f ? 1.f : y); // 0 <= y <= 1

		if(config.output.invert.x)
			x = 1.f - x;
		if(config.output.invert.z)
			y = 1.f - y;

		CMC_Blob *blob = &cmc_neu[J];
		blob->sid = -1; // not assigned yet
		blob->pid = vn[P] == POLE_NORTH ? CMC_NORTH : CMC_SOUTH; // for the A1302, south-polarity(+B) magnetic fields increase the output voltage, north-polaritiy(-B) decrease it
		blob->group = NULL;
		blob->x = x;
		blob->y = y;
		memset(&blob->vx, 0, sizeof(CMC_Filt));
		memset(&blob->vy, 0, sizeof(CMC_Filt));
		blob->v = 0.f;
		blob->m = 0.f;
		blob->above_thresh = va[P];
		blob->state = CMC_BLOB_INVALID;

		J++;
	} // 50us per blob

	uint_fast8_t changed = 0;

	/*
	 * relate new to old blobs
	 */
	uint_fast8_t i, j;
	if(I || J)
	{
		switch((J > I) - (J < I) ) // == signum(J-I)
		{
			case -1: // old blobs have disappeared
			{
				uint_fast8_t n_less = I - J; // how many blobs have disappeared
				i = 0;
				for(j=0; j<J; )
				{
					float diff0, diff1;

					if(n_less)
					{
						diff0 = VABS(cmc_neu[j].x - cmc_old[i].x);
						diff1 = VABS(cmc_neu[j].x - cmc_old[i+1].x);
					}

					if( n_less && (diff1 < diff0) )
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
						cmc_neu[j].state = (cmc_old[i].x == cmc_neu[j].x) && (cmc_old[i].y == cmc_neu[j].y) ? CMC_BLOB_EXISTED_STILL : CMC_BLOB_EXISTED_DIRTY;
						VEL_ACCEL(&cmc_old[i], &cmc_neu[j], rate);

						i++;
						j++;
					}
				}

				// if(n_less)
				for(i=I - n_less; i<I; i++)
					cmc_old[i].state = CMC_BLOB_DISAPPEARED;
						
				changed = changed || 1;

				break;
			}

			case 0: // there has been no change in blob number, so we can relate the old and new lists 1:1 as they are both ordered according to x
			{
				for(j=0; j<J; j++)
				{
					cmc_neu[j].sid = cmc_old[j].sid;
					cmc_neu[j].group = cmc_old[j].group;
					cmc_neu[j].state = (cmc_old[j].x == cmc_neu[j].x) && (cmc_old[j].y == cmc_neu[j].y) ? CMC_BLOB_EXISTED_STILL : CMC_BLOB_EXISTED_DIRTY;
					VEL_ACCEL(&cmc_old[j], &cmc_neu[j], rate);

					changed = changed || (cmc_neu[j].state == CMC_BLOB_EXISTED_DIRTY);
				}

				break;
			}

			case 1: // new blobs have appeared
			{
				uint_fast8_t n_more = J - I; // how many blobs have appeared
				j = 0;
				for(i=0; i<I; )
				{
					float diff0, diff1;
					
					if(n_more) // only calculate differences when there are still new blobs to be found
					{
						diff0 = VABS(cmc_neu[j].x - cmc_old[i].x);
						diff1 = VABS(cmc_neu[j+1].x - cmc_old[i].x);
					}

					if( n_more && (diff1 < diff0) ) // cmc_neu[j] is the new blob
					{
						if(cmc_neu[j].above_thresh) // check whether it is above threshold for a new blob
						{
							cmc_neu[j].sid = ++(sid); // this is a new blob
							cmc_neu[j].group = NULL;
							cmc_neu[j].state = CMC_BLOB_APPEARED;

							changed = changed || 1;
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
						cmc_neu[j].state = (cmc_old[i].x == cmc_neu[j].x) && (cmc_old[i].y == cmc_neu[j].y) ? CMC_BLOB_EXISTED_STILL : CMC_BLOB_EXISTED_DIRTY;
						VEL_ACCEL(&cmc_old[i], &cmc_neu[j], rate);

						changed = changed || (cmc_neu[j].state == CMC_BLOB_EXISTED_DIRTY);
						j++;
						i++;
					}
				}

				//if(n_more)
				for(j=J - n_more; j<J; j++)
				{
					if(cmc_neu[j].above_thresh) // check whether it is above threshold for a new blob
					{
						cmc_neu[j].sid = ++(sid); // this is a new blob
						cmc_neu[j].group = NULL;
						cmc_neu[j].state = CMC_BLOB_APPEARED;

						changed = changed || 1;
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
		for(j=0; j<J; j++)
		{
			uint_fast8_t ignore = cmc_neu[j].state == CMC_BLOB_IGNORED;

			if(newJ != j)
				memmove(&cmc_neu[newJ], &cmc_neu[j], sizeof(CMC_Blob));

			if(!ignore)
				newJ++;
		}
		J = newJ;

		/*
		 * relate blobs to groups
		 */
		for(j=0; j<J; j++)
		{
			CMC_Blob *tar = &cmc_neu[j];

			uint16_t gid;
			for(gid=0; gid<GROUP_MAX; gid++)
			{
				CMC_Group *ptr = &cmc_groups[gid];

				if( ((tar->pid & ptr->pid) == tar->pid) && (tar->x >= ptr->x0) && (tar->x <= ptr->x1) )
				{
					if(tar->group && (tar->group != ptr) ) // give it a new sid when group has changed since last step
					{
						// mark old blob as DISAPPEARED
						for(i=0; i<I; i++)
							if(cmc_old[i].sid == tar->sid)
							{
								cmc_old[i].state = CMC_BLOB_DISAPPEARED;
								break;
							}

						// mark new blob as APPEARED and give it a new sid
						tar->sid = ++(sid);
						tar->state = CMC_BLOB_APPEARED;

						changed = changed || 1;
					}

					tar->group = ptr;

					if( (ptr->m != CMC_NOSCALE) && ((ptr->x0 != 0.0) || (ptr->m != 1.0) ) ) // we need scaling
						tar->x =(tar->x - ptr->x0) * ptr->m;

					break; // match found, do not search further
				}
			}
		}

		if(changed)
		{
			idle_bit = 0;
			idle_word = 0;
		}
		else
			idle_word++;
	}
	else // I == J == 0
		idle_word++;

	/*
	 * (not)advance idle counters
	 */
	uint_fast8_t idle = 0;
	if(idle_bit < pacemaker)
	{
		idle = idle_word == (1 << idle_bit);
		if(idle)
			idle_bit++;
	}
	else // handle pacemaker
	{
		idle = idle_word == (1 << idle_bit);
		if(idle)
			idle_word = 0;
	}

	/*
	 * handle output engines
	 */
	uint_fast8_t res = changed || idle;
	if(res)
	{
		++(fid);
#ifdef BENCHMARK
		stop_watch_start(&sw_engine_process);
#endif

		float zero = config.output.invert.z ? 1.f : 0.f;
		uint_fast8_t e;
		for(e=0; e<ENGINE_MAX; e++)
		{
			CMC_Engine *engine;
			
			if( !(engine = engines[e]) ) // terminator reached
				break;

			CMC_Frame_Event fev = {
				.fid = fid,
				.now = now,
				.offset = offset,
				.nblob_old = I,
				.nblob_new = J
			};

			if(engine->frame_cb)
				buf_ptr = engine->frame_cb(buf_ptr, end, &fev);

			if(engine->on_cb || engine->set_cb)
				for(j=0; j<J; j++)
				{
					CMC_Blob *tar = &cmc_neu[j];
					CMC_Blob_Event bev = {
						.sid = tar->sid,
						.gid = tar->group->gid,
						.pid = tar->pid,
						.x = tar->x,
						.y = tar->y,
						.vx = tar->vx.f11,
						.vy = tar->vy.f11,
						.m = tar->m
					};

					if(tar->state == CMC_BLOB_APPEARED)
					{
						if(engine->on_cb)
							buf_ptr = engine->on_cb(buf_ptr, end, &bev);
					}
					else // (tar->state == CMC_BLOB_EXISTED_DIRTY) || (tar->state == CMC_BLOB_EXISTED_STILLA)
					{
						if(engine->set_cb)
							buf_ptr = engine->set_cb(buf_ptr, end, &bev);
					}
				}

			if(engine->off_cb)
				for(i=0; i<I; i++)
				{
					CMC_Blob *tar = &cmc_old[i];
					CMC_Blob_Event bev = {
						.sid = tar->sid,
						.gid = tar->group->gid,
						.pid = tar->pid,
						.x = tar->x,
						.y = zero,
						.vx = 0.f, //FIXME
						.vy = 0.f, //FIXME
						.m = 0.f //FIXME
					};

					if(tar->state == CMC_BLOB_DISAPPEARED)
					{
						if(tar->y != zero)
							buf_ptr = engine->set_cb(buf_ptr, end, &bev);
						buf_ptr = engine->off_cb(buf_ptr, end, &bev);
					}
				}

			if(engine->end_cb)
				buf_ptr = engine->end_cb(buf_ptr, end, &fev);
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

	return buf_ptr;
}

void 
cmc_group_reset(void)
{
	uint16_t gid;
	for(gid=0; gid<GROUP_MAX; gid++)
	{
		CMC_Group *grp = &cmc_groups[gid];

		grp->gid = gid;
		grp->pid = 0;
		grp->x0 = 0.0;
		grp->x1 = 1.0;
		grp->m = CMC_NOSCALE;
	}

	// define two groups by default
	cmc_groups[0].pid = CMC_SOUTH;
	cmc_groups[1].pid = CMC_NORTH;

	cmc_group_update();
}

static void
cmc_engines_init(void)
{
	if(oscmidi_engine.init_cb)
		oscmidi_engine.init_cb();

	if(dummy_engine.init_cb)
		dummy_engine.init_cb();

	if(scsynth_engine.init_cb)
		scsynth_engine.init_cb();

	if(tuio2_engine.init_cb)
		tuio2_engine.init_cb();

	if(tuio1_engine.init_cb)
		tuio1_engine.init_cb();

	if(custom_engine.init_cb)
		custom_engine.init_cb();
}

void
cmc_group_update(void)
{
	uint16_t gid;
	for(gid=0; gid<GROUP_MAX; gid++)
	{
		if(!cmc_groups[gid].pid)
			break; // first non-responding group found
	}

	cmc_groups_n = gid;

	// reinitialize engines (e.g. oscmidi needs that for MPE mode)
	cmc_engines_init();
}

void
cmc_engines_update(void)
{
	cmc_engines_active = 0;

	if(config.oscmidi.enabled)
		engines[cmc_engines_active++] = &oscmidi_engine;

	if(config.dummy.enabled)
		engines[cmc_engines_active++] = &dummy_engine;

	if(config.scsynth.enabled)
		engines[cmc_engines_active++] = &scsynth_engine;

	if(config.tuio2.enabled)
		engines[cmc_engines_active++] = &tuio2_engine;

	if(config.tuio1.enabled)
		engines[cmc_engines_active++] = &tuio1_engine;

	if(config.custom.enabled)
		engines[cmc_engines_active++] = &custom_engine;

	engines[cmc_engines_active] = NULL;
}
