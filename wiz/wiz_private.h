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
void wiz_job_set_frame(void);
void wiz_job_run_single(void);
void wiz_job_run_nonblocking(void);
void wiz_job_run_block(void);

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
