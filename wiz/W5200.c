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

#include <string.h>

#include "W5200_private.h"

#include <tube.h>
#include <netdef.h>
#include <chimaera.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>
#include <libmaple/systick.h>

uint16_t SBASE [WIZ_MAX_SOCK_NUM];
uint16_t RBASE [WIZ_MAX_SOCK_NUM];

uint16_t SMASK [WIZ_MAX_SOCK_NUM];
uint16_t RMASK [WIZ_MAX_SOCK_NUM];

const uint16_t SOCK_OFFSET [WIZ_MAX_SOCK_NUM] = {
	CH_BASE + 0*CH_SIZE,
	CH_BASE + 1*CH_SIZE,
	CH_BASE + 2*CH_SIZE,
	CH_BASE + 3*CH_SIZE,
	CH_BASE + 4*CH_SIZE,
	CH_BASE + 5*CH_SIZE,
	CH_BASE + 6*CH_SIZE,
	CH_BASE + 7*CH_SIZE,
};

__always_inline void
wiz_job_set_frame()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t *frm_tx = job->tx - WIZ_SEND_OFFSET;

	frm_tx[0] = job->addr >> 8;
	frm_tx[1] = job->addr & 0xFF;
	frm_tx[2] = (job->rw & WIZ_RX ? 0x00 : 0x80) | ((job->len & 0x7F00) >> 8);
	frm_tx[3] = job->len & 0x00FF;
}

__always_inline void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (SOCK_OFFSET[sock] + addr, 0, dat, len);
}

__always_inline void
_dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read (SOCK_OFFSET[sock] + addr, 0, dat, len);
}

void
wiz_sockets_set (uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM])
{
	uint_fast8_t sock;
	uint8_t flag;

	// initialize all socket memory TX and RX sizes to their corresponding sizes
  for (sock=0; sock<WIZ_MAX_SOCK_NUM; sock++)
	{
		// initialize tx registers
		SSIZE[sock] = (uint16_t)tx_mem[sock] * 0x0400;
		SMASK[sock] = SSIZE[sock] - 1;
		if (sock>0)
			SBASE[sock] = SBASE[sock-1] + SSIZE[sock-1];
		else
			SBASE[sock] = TX_BUF_BASE;

		flag = tx_mem[sock];
		if (flag == 0x10)
			flag = 0x0f; // special case
		_dma_write_sock (sock, WIZ_Sn_TX_MS, &flag, 1); // TX_MEMSIZE

		// initialize rx registers
		RSIZE[sock] = (uint16_t)rx_mem[sock] * 0x0400;
		RMASK[sock] = RSIZE[sock] - 1;
		if (sock>0)
			RBASE[sock] = RBASE[sock-1] + RSIZE[sock-1];
		else
			RBASE[sock] = RX_BUF_BASE;

		flag = rx_mem[sock];
		if (flag == 0x10) 
			flag = 0x0f; // special case
		_dma_write_sock (sock, WIZ_Sn_RX_MS, &flag, 1); // RX_MEMSIZE
  }
}

uint_fast8_t
udp_receive_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	if( (len == 0) || (len > CHIMAERA_BUFSIZE + 2*WIZ_SEND_OFFSET + 3) )
		return 0;

	if (tmp_buf_o_ptr != buf_ptr)
		tmp_buf_o_ptr = buf_ptr;

	uint8_t *buf = buf_o[tmp_buf_o_ptr] + WIZ_SEND_OFFSET;
	tmp_buf_i = buf_i_i + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Rx_RD[sock];

	uint16_t offset = ptr & RMASK[sock];
	uint16_t dstAddr = offset + RBASE[sock];

	// read message
	if ( (offset + len) > RSIZE[sock])
	{
		uint16_t size = RSIZE[sock] - offset;
		// read overflown part first to not overwrite buffer!
		wiz_job_add(RBASE[sock], len-size, buf+size, tmp_buf_i+size, 0, WIZ_TXRX);
		wiz_job_add(dstAddr, size, buf, tmp_buf_i, 0, WIZ_TXRX);
	}
	else
		wiz_job_add(dstAddr, len, buf, tmp_buf_i, 0, WIZ_TXRX);

  ptr += len;
	Sn_Rx_RD[sock] = ptr;

	uint8_t *flag = buf+len;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_RX_RD, 2, &flag[0], NULL, 0, WIZ_TX);

	// send data
	flag[2] = WIZ_Sn_CR_RECV;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_CR, 1, &flag[2], NULL, 0, WIZ_TX);

	wiz_job_run_nonblocking();

	return 1;
}

uint_fast8_t 
udp_send_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	if( (len == 0) || (len > CHIMAERA_BUFSIZE + 2*WIZ_SEND_OFFSET + 3) )
		return 0;

	// switch DMA memory source to right buffer, input buffer is on SEND by default
	if (tmp_buf_o_ptr != buf_ptr)
		tmp_buf_o_ptr = buf_ptr;

	uint8_t *buf = buf_o[buf_ptr] + WIZ_SEND_OFFSET;

	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
  uint16_t dstAddr = offset + SBASE[sock];

  if ( (offset + len) > SSIZE[sock]) 
  {
    uint16_t size = SSIZE[sock] - offset;
		wiz_job_add(dstAddr, size, buf, NULL, 0, WIZ_TX);
		wiz_job_add(SBASE[sock], len-size, buf+size, NULL, 0, WIZ_TX);
  } 
  else
		wiz_job_add(dstAddr, len, buf, NULL, 0, WIZ_TX);

  ptr += len;
	Sn_Tx_WR[sock] = ptr;

	uint8_t *flag = buf+len;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_TX_WR, 2, &flag[0], NULL, 0, WIZ_TX);

	// send data
	flag[2] = WIZ_Sn_CR_SEND;
	wiz_job_add(SOCK_OFFSET[sock] + WIZ_Sn_CR, 1, &flag[2], NULL, 0, WIZ_TX);

	wiz_job_run_nonblocking();

	return 1;
}
