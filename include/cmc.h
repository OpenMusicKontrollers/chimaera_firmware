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

#ifndef _CMC_H_
#define _CMC_H_

#include <stdint.h>
#include <stdlib.h>

#include <nosc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMC_NORTH 0x80
#define CMC_SOUTH 0x100
#define CMC_BOTH (CMC_NORTH | CMC_SOUTH)

typedef struct _CMC CMC;

typedef void (*CMC_Send_Cb) (void *data, uint8_t *buf, uint16_t len);

CMC *cmc_new (uint8_t ns, uint8_t mb, uint16_t bitdepth, uint16_t df, uint16_t th0, uint16_t th1);
void cmc_free (CMC *cmc);

void cmc_set (CMC *cmc, uint8_t i, uint16_t v, uint8_t n);
void cmc_set_array (CMC *cmc, int16_t raw[16][10], uint8_t order[16][10], uint8_t mux_min, uint8_t mux_max, uint8_t adc_len);
uint8_t cmc_process (CMC *cmc);
uint16_t cmc_write_tuio2 (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf);
uint16_t cmc_write_rtpmidi (CMC *cmc, uint8_t *buf);
uint16_t cmc_dump (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf);
uint16_t cmc_dump_partial (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf, uint8_t s0, uint8_t s1);
uint16_t cmc_dump_first (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf);
uint16_t cmc_dump_unit (CMC *cmc, timestamp64u_t timestamp, uint8_t *buf, uint8_t unit);

void cmc_group_clear (CMC *cmc);
uint8_t cmc_group_add (CMC *cmc, uint16_t tid, uint16_t uid, float x0, float x1);
uint8_t cmc_group_set (CMC *cmc, uint16_t tid, uint16_t uid, float x0, float x1);

#ifdef __cplusplus
}
#endif

#endif
