/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#ifndef _WIZ_PRIVATE_H_
#define _WIZ_PRIVATE_H_

#include <wiz.h>

enum {
	WIZ_TX		= 1,
	WIZ_RX		= 2,
	WIZ_TXRX	= 3
};

typedef struct _Wiz_Job Wiz_Job;

struct _Wiz_Job {
	uint16_t addr;
	uint16_t len;
	uint8_t *tx;
	uint8_t *rx;
	uint8_t opmode; // only for W5500
	uint8_t rw;
	uint8_t rx_hdr_sent;
};

void wiz_job_add(uint16_t addr, uint16_t len, uint8_t *tx, uint8_t *rx, uint8_t opmode, uint8_t rw);
void wiz_job_set_frame();
void wiz_job_run_single();
void wiz_job_run_nonblocking();
void wiz_job_run_block();

#define WIZ_MAX_JOB_NUM 8

extern Wiz_Job wiz_jobs [WIZ_MAX_JOB_NUM];
extern volatile uint_fast8_t wiz_jobs_todo;
extern volatile uint_fast8_t wiz_jobs_done;

extern gpio_dev *ss_dev;
extern uint8_t ss_bit;
#define setSS()		gpio_write_bit(ss_dev, ss_bit, 0)
#define resetSS()	gpio_write_bit(ss_dev, ss_bit, 1)

extern Wiz_IRQ_Cb irq_cb;
extern Wiz_IRQ_Cb irq_socket_cb [WIZ_MAX_SOCK_NUM];

extern uint16_t SSIZE [WIZ_MAX_SOCK_NUM];
extern uint16_t RSIZE [WIZ_MAX_SOCK_NUM];

extern uint16_t Sn_Tx_WR[WIZ_MAX_SOCK_NUM];
extern uint16_t Sn_Rx_RD[WIZ_MAX_SOCK_NUM];

extern uint8_t buf_o2 [];
extern uint8_t buf_i2 [];

void _dma_write(uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len);
void _dma_write_sock(uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len);
void _dma_write_sock_16(uint8_t sock, uint16_t addr, uint16_t dat);

void _dma_read(uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len);
void _dma_read_sock(uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len);
void _dma_read_sock_16(int8_t sock, uint16_t addr, uint16_t *dat);

#endif // _WIZ_PRIVATE_H_
