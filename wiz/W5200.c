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

#include <string.h>

#include "W5200_private.h"

#include <tube.h>
#include <netdef.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>
#include <libmaple/systick.h>

static uint16_t SBASE [WIZ_MAX_SOCK_NUM];
static uint16_t RBASE [WIZ_MAX_SOCK_NUM];

static uint16_t SMASK [WIZ_MAX_SOCK_NUM];
static uint16_t RMASK [WIZ_MAX_SOCK_NUM];

static const uint16_t SOCK_OFFSET [WIZ_MAX_SOCK_NUM] = {
	CH_BASE + 0*CH_SIZE,
	CH_BASE + 1*CH_SIZE,
	CH_BASE + 2*CH_SIZE,
	CH_BASE + 3*CH_SIZE,
	CH_BASE + 4*CH_SIZE,
	CH_BASE + 5*CH_SIZE,
	CH_BASE + 6*CH_SIZE,
	CH_BASE + 7*CH_SIZE,
};

inline __always_inline void
wiz_job_set_frame()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t *frm_tx = job->tx - WIZ_SEND_OFFSET;

	frm_tx[0] = job->addr >> 8;
	frm_tx[1] = job->addr & 0xFF;
	frm_tx[2] =(job->rw & WIZ_RX ? 0x00 : 0x80) | ((job->len & 0x7F00) >> 8);
	frm_tx[3] = job->len & 0x00FF;
}

inline __always_inline void
_dma_write_sock(uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write(SOCK_OFFSET[sock] + addr, 0, dat, len);
}

inline __always_inline void
_dma_read_sock(uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read(SOCK_OFFSET[sock] + addr, 0, dat, len);
}

void
wiz_sockets_set(uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM])
{
	uint_fast8_t sock;
	uint8_t flag;

	// initialize all socket memory TX and RX sizes to their corresponding sizes
  for(sock=0; sock<WIZ_MAX_SOCK_NUM; sock++)
	{
		// initialize tx registers
		SSIZE[sock] =(uint16_t)tx_mem[sock] * 0x0400;
		SMASK[sock] = SSIZE[sock] - 1;
		if(sock>0)
			SBASE[sock] = SBASE[sock-1] + SSIZE[sock-1];
		else
			SBASE[sock] = TX_BUF_BASE;

		flag = tx_mem[sock];
		if(flag == 0x10)
			flag = 0x0f; // special case
		_dma_write_sock(sock, WIZ_Sn_TX_MS, &flag, 1); // TX_MEMSIZE

		// initialize rx registers
		RSIZE[sock] =(uint16_t)rx_mem[sock] * 0x0400;
		RMASK[sock] = RSIZE[sock] - 1;
		if(sock>0)
			RBASE[sock] = RBASE[sock-1] + RSIZE[sock-1];
		else
			RBASE[sock] = RX_BUF_BASE;

		flag = rx_mem[sock];
		if(flag == 0x10) 
			flag = 0x0f; // special case
		_dma_write_sock(sock, WIZ_Sn_RX_MS, &flag, 1); // RX_MEMSIZE
  }
}

uint_fast8_t __CCM_TEXT__
udp_receive_nonblocking(uint8_t sock, uint8_t *i_buf, uint16_t len)
{
	if( (len == 0) || (len > CHIMAERA_BUFSIZE - 2*(WIZ_SEND_OFFSET + 3) ) )
		return 0;

	uint8_t *tmp_buf_o = buf_o2 + WIZ_SEND_OFFSET;
	uint8_t *tmp_buf_i = i_buf + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Rx_RD[sock];

	uint16_t offset = ptr & RMASK[sock];
	uint16_t dstAddr = offset + RBASE[sock];

	// read message
	if( (offset + len) > RSIZE[sock])
	{
		uint16_t size = RSIZE[sock] - offset;
		// read overflown part first to not overwrite buffer!
		wiz_job_add(RBASE[sock], len-size, tmp_buf_o, tmp_buf_i+size, 0, WIZ_RX);
		wiz_job_add(dstAddr, size, tmp_buf_o, tmp_buf_i, 0, WIZ_RX);
	}
	else
		wiz_job_add(dstAddr, len, tmp_buf_o, tmp_buf_i, 0, WIZ_RX);

  ptr += len;
	Sn_Rx_RD[sock] = ptr;

	uint8_t *flag = tmp_buf_o;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_RX_RD, 2, &flag[0], NULL, 0, WIZ_TX);

	// receive data
	flag[2] = WIZ_Sn_CR_RECV;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_CR, 1, &flag[2], NULL, 0, WIZ_TX);

	wiz_job_run_nonblocking();

	return 1;
}

void __CCM_TEXT__
udp_peek(uint8_t sock, uint8_t *i_buf, uint16_t len)
{
	uint8_t *tmp_buf_o = buf_o2 + WIZ_SEND_OFFSET;
	uint8_t *tmp_buf_i = i_buf + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Rx_RD[sock];

	uint16_t offset = ptr & RMASK[sock];
	uint16_t dstAddr = offset + RBASE[sock];

	// read message
	if( (offset + len) > RSIZE[sock])
	{
		uint16_t size = RSIZE[sock] - offset;
		// read overflown part first to not overwrite buffer!
		wiz_job_add(RBASE[sock], len-size, tmp_buf_o, tmp_buf_i+size, 0, WIZ_RX);
		wiz_job_add(dstAddr, size, tmp_buf_o, tmp_buf_i, 0, WIZ_RX);
	}
	else
		wiz_job_add(dstAddr, len, tmp_buf_o, tmp_buf_i, 0, WIZ_RX);

	wiz_job_run_nonblocking();
	wiz_job_run_block();
}

void __CCM_TEXT__
udp_skip(uint8_t sock, uint16_t len)
{
	uint8_t *tmp_buf_o = buf_o2 + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Rx_RD[sock];

  ptr += len;
	Sn_Rx_RD[sock] = ptr;

	uint8_t *flag = tmp_buf_o;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_RX_RD, 2, &flag[0], NULL, 0, WIZ_TX);

	// receive data
	flag[2] = WIZ_Sn_CR_RECV;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_CR, 1, &flag[2], NULL, 0, WIZ_TX);

	wiz_job_run_nonblocking();
	wiz_job_run_block();
}

uint_fast8_t  __CCM_TEXT__
udp_send_nonblocking(uint8_t sock, uint8_t *o_buf, uint16_t len)
{
	if( (len == 0) || (len > CHIMAERA_BUFSIZE - 2*WIZ_SEND_OFFSET - 3) )
		return 0;

	uint8_t *tmp_buf_o = o_buf + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
  uint16_t dstAddr = offset + SBASE[sock];

  if( (offset + len) > SSIZE[sock]) 
  {
    uint16_t size = SSIZE[sock] - offset;
		wiz_job_add(dstAddr, size, tmp_buf_o, NULL, 0, WIZ_TX);
		wiz_job_add(SBASE[sock], len-size, tmp_buf_o+size, NULL, 0, WIZ_TX);
  } 
  else
		wiz_job_add(dstAddr, len, tmp_buf_o, NULL, 0, WIZ_TX);

  ptr += len;
	Sn_Tx_WR[sock] = ptr;

	uint8_t *flag = tmp_buf_o+len;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_TX_WR, 2, &flag[0], NULL, 0, WIZ_TX);

	// send data
	flag[2] = WIZ_Sn_CR_SEND;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_CR, 1, &flag[2], NULL, 0, WIZ_TX);

	wiz_job_run_nonblocking();

	return 1;
}
