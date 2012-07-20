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
#include <config.h>

/*
static RTP_MIDI_List note_on [] = {
	{0x0, NOTE_ON}, // note on / channel 0
	{0x0, 0x7f}, // key
	{0x0, 0x7f}, // velocity

	{0x0, PITCH_BEND}, // pitch bend / channel 0
	{0x0, 0x7f}, // pitch bend LSB
	{0x0, 0x7f}, // pitch bend MSB

	{0x0 ,0xa0}, // control change / channel 0
	{0x0, VOLUME}, // volume
	{0x0, 0x1a}, // LSB

	{0x0, 0xa0}, // control change / channel 0
	{0x0, VOLUME}, // volume
	{0x0, 0x1a} // MSB
};

static RTP_MIDI_List note_off [] = {
	{0x0, NOTE_OFF}, // note on / channel 0
	{0x0, 0x7f}, // key
	{0x0, 0x00} // velocity
};

static RTP_MIDI_List note_set [] = {
	{0x0, PITCH_BEND}, // pitch bend / channel 0
	{0x0, 0x7f}, // pitch bend LSB
	{0x0, 0x7f}, // pitch bend MSB

	{0x0 ,0xa0}, // control change / channel 0
	{0x0, VOLUME}, // volume
	{0x0, 0x1a}, // LSB

	{0x0, 0xa0}, // control change / channel 0
	{0x0, VOLUME}, // volume
	{0x0, 0x1a} // MSB
};
*/

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
	cmc->_bitdepth = 1.0ur / bitdepth;

	cmc->thresh0 = th0;
	cmc->thresh1 = th1;
	cmc->thresh0_f = cmc->thresh0 * cmc->_bitdepth;
	cmc->_thresh0_f = 1.0ur - cmc->thresh0_f;

	cmc->d = 1.0ur / (ns-1);

	cmc->sensors = calloc (ns+2, sizeof (CMC_Sensor));
	cmc->old_blobs = calloc (mb, sizeof (CMC_Blob));
	cmc->new_blobs = calloc (mb, sizeof (CMC_Blob));

	for (i=0; i<ns+2; i++)
	{
		cmc->sensors[i].x = cmc->d * (i-1);
		cmc->sensors[i].v = 0x0;
	}

	cmc->matrix = calloc (mb, sizeof (fix31_t *));
	for (i=0; i<mb; i++)
		cmc->matrix[i] = calloc (mb, sizeof (fix31_t));

	cmc->tuio = tuio2_new (mb);
	cmc->rtpmidi_packet = rtpmidi_new ();

	cmc->groups = _cmc_group_new ();

	return cmc;
}

void
cmc_free (CMC *cmc)
{
	uint8_t i;

	_cmc_group_free (cmc->groups);
	rtpmidi_free (cmc->rtpmidi_packet);
	tuio2_free (cmc->tuio);
	for (i=0; i<cmc->max_blobs; i++)
		free (cmc->matrix[i]);
	free (cmc->matrix);
	free (cmc->new_blobs);
	free (cmc->old_blobs);
	free (cmc->sensors);
	free (cmc);
}

void
cmc_set (CMC* cmc, uint8_t i, uint16_t v, uint8_t n)
{
	cmc->sensors[i+1].v = v < cmc->thresh0 ? 0 : v;
	cmc->sensors[i+1].n = n;
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
		if ( (cmc->sensors[i].v > cmc->thresh0) && (cmc->sensors[i].v - cmc->sensors[i-1].v >= cmc->diff) && (cmc->sensors[i].v - cmc->sensors[i+1].v > cmc->diff) )
		{
			uint8_t I;
			uint8_t local_max = 0;
			for (I=2; I<3; I++)
			{
				if ( (i-I >= 0) && (cmc->sensors[i].v - cmc->sensors[i-I].v < cmc->diff) )
				{
					local_max = 1;
					break;
				}
				if ( (i+I <= cmc->n_sensors) && (cmc->sensors[i].v - cmc->sensors[i+I].v < cmc->diff) )
				{
					local_max = 1;
					break;
				}
			}
			if (local_max)
				continue;

			//fit parabola
			ufix32_t x1 = cmc->sensors[i].x;
			ufix32_t d = cmc->d;

			fix31_t y0 = cmc->sensors[i-1].v * cmc->_bitdepth;
			fix31_t y1 = cmc->sensors[i].v * cmc->_bitdepth;
			fix31_t y2 = cmc->sensors[i+1].v * cmc->_bitdepth;

			ufix32_t x = x1 + d/2*(y0 - y2) / (y0 - y1 + y2 - y1);
			ufix32_t y = (y0*0.5ur + y1*0.5ur + y2*0.5ur); // TODO is there a better formula

			// rescale according to threshold
			y -= cmc->thresh0_f;
			y /= cmc->_thresh0_f; // TODO division is expensive, solve differently

			if (x < 0.0ur) x = 0.0ur;
			if (x > 1.0ur) x = 1.0ur;
			if (y < 0.0ur) y = 0.0ur;
			if (y > 1.0ur) y = 1.0ur;

			cmc->new_blobs[cmc->J].sid = -1; // not assigned yet
			cmc->new_blobs[cmc->J].x = x;
			cmc->new_blobs[cmc->J].p = y;
			cmc->new_blobs[cmc->J].uid = cmc->sensors[i].n ? CMC_SOUTH : CMC_NORTH;
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
		for (i=0; i<cmc->I; i++) // old blobs
			for (j=0; j<cmc->J; j++) // new blobs
			{
				cmc->matrix[i][j] = (fix31_t)cmc->new_blobs[j].x - (fix31_t)cmc->old_blobs[i].x; // 1D
				cmc->matrix[i][j] = cmc->matrix[i][j] < 0.0r ? -cmc->matrix[i][j] : cmc->matrix[i][j]; //TODO how to to a fixed-point abs?
			}

		// associate new with old blobs
		uint8_t ptr;
		uint8_t low = cmc->J <= cmc->I ? cmc->J : cmc->I;
		for (ptr=0; ptr<low; ptr++)
		{
			fix31_t min = 1.0r;
			uint8_t a = 0, b = 0;

			for (i=0; i<cmc->I; i++) // old blobs
				for (j=0; j<cmc->J; j++) // new blobs
				{
					if (cmc->matrix[i][j] == -0.5r)
						continue;

					if (cmc->matrix[i][j] < min)
					{
						a = i;
						b = j;
						min = cmc->matrix[i][j];
					}
				}

			CMC_Blob *blb = &(cmc->old_blobs[a]);
			CMC_Blob *blb2 = &(cmc->new_blobs[b]);

			blb2->sid = blb->sid;
			blb2->group = blb->group;

			for (i=0; i<cmc->I; i++)
				cmc->matrix[i][b] = -0.5r;
			for (j=0; j<cmc->J; j++)
				cmc->matrix[a][j] = -0.5r;
		}

		// handle all new blobs
		if (cmc->J > cmc->I)
			for (j=0; j<cmc->J; j++)
				if (cmc->new_blobs[j].sid == -1)
				{
					if (cmc->new_blobs[j].above_thresh) // check whether it is above threshold for a new blob
						cmc->new_blobs[j].sid = ++(cmc->sid); // this is a new blob
					else
						cmc->new_blobs[j].ignore = 1;
					cmc->new_blobs[j].group = NULL;
				}
	}
	else if (!cmc->I && cmc->J)
	{
		for (j=0; j<cmc->J; j++)
		{
			if (cmc->new_blobs[j].above_thresh) // check whether it is above threshold for a new blob
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

		if (newJ != j)
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
cmc_write_rtpmidi (CMC *cmc, uint8_t *buf)
{
	uint16_t size;
	//TODO
	uint32_t timestamp = 0x0;

	//TODO
	rtpmidi_header_set (cmc->rtpmidi_packet, cmc->fid, timestamp);

	// note on
	//TODO
	//rtpmidi_list_set (cmc->rtpmidi_packet, &note_on[0], sizeof (note_on));
	
	size = rtpmidi_serialize (cmc->rtpmidi_packet, buf);
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
	if (cmc->n_groups >= config.cmc_max_groups)
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
