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

#ifndef WIZ_PRIVATE_H_
#define WIZ_PRIVATE_H_

#include <wiz.h>

enum {
	WIZ_TX		= 0b01,
	WIZ_RX		= 0b10,
	WIZ_TXRX	= 0b11
};

typedef struct _Wiz_Job Wiz_Job;

struct _Wiz_Job {
	uint16_t addr;
	uint16_t len;
	uint8_t *buf;
	uint8_t opmode; // only for W5500
	uint8_t rw;
};

void wiz_job_clear();
void wiz_job_add(uint16_t addr, uint16_t len, uint8_t *buf, uint8_t opmode, uint8_t rw);
void wiz_job_set_frame();
void wiz_job_run_single();
void wiz_job_run_nonblocking();
void wiz_job_run_block();

#define WIZ_MAX_JOB_NUM 8

extern Wiz_Job wiz_jobs [];
volatile uint_fast8_t wiz_jobs_todo;
volatile uint_fast8_t wiz_jobs_done;

extern const uint8_t wiz_nil_ip [];
extern const uint8_t wiz_nil_mac [];
extern const uint8_t wiz_broadcast_mac [];

extern gpio_dev *ss_dev;
extern uint8_t ss_bit;
#define setSS()		gpio_write_bit (ss_dev, ss_bit, 0)
#define resetSS()	gpio_write_bit (ss_dev, ss_bit, 1)

extern uint_fast8_t tmp_buf_o_ptr;
extern uint8_t *tmp_buf_i;
#define INPUT_BUF_SEND 0
#define INPUT_BUF_RECV 1

extern uint16_t nonblocklen;

extern Wiz_IRQ_Cb irq_cb;
extern Wiz_IRQ_Cb irq_socket_cb [WIZ_MAX_SOCK_NUM];

extern uint16_t SSIZE [WIZ_MAX_SOCK_NUM];
extern uint16_t RSIZE [WIZ_MAX_SOCK_NUM];

extern uint16_t Sn_Tx_WR[WIZ_MAX_SOCK_NUM];
extern uint16_t Sn_Rx_RD[WIZ_MAX_SOCK_NUM];

void _spi_dma_run (uint16_t len, uint8_t io_flags);
uint_fast8_t _spi_dma_block (uint8_t io_flags);

uint_fast8_t _dma_write_nonblocking_in (uint8_t *buf);
uint_fast8_t _dma_write_nonblocking_out ();
uint_fast8_t _dma_read_nonblocking_in (uint8_t *buf);
uint_fast8_t _dma_read_nonblocking_out ();

void _dma_write (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len);
uint8_t * _dma_write_append (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len);
uint8_t * _dma_write_inline (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint16_t len);
void _dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len);
uint8_t * _dma_write_sock_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len);
void _dma_write_sock_16 (uint8_t sock, uint16_t addr, uint16_t dat);
uint8_t * _dma_write_sock_16_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint16_t dat);

void _dma_read (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len);
uint8_t * _dma_read_append (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint16_t len);
void _dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len);
void _dma_read_sock_16 (int8_t sock, uint16_t addr, uint16_t *dat);

#endif
