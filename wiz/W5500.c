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

#include "W5500_private.h"

#include <tube.h>
#include <netdef.h>
#include <chimaera.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>
#include <libmaple/systick.h>

#define W5500_SOCKET_SEL_ENTRY(s) \
	[s] = { \
		.reg = (s*4)+1 << W5500_CNTRL_PHASE_BLOCK_SEL_SHIFT, \
		.tx_buf = (s*4)+2 << W5500_CNTRL_PHASE_BLOCK_SEL_SHIFT, \
		.rx_buf = (s*4)+3 << W5500_CNTRL_PHASE_BLOCK_SEL_SHIFT, \
	}

const W5500_Socket_Sel W5500_socket_sel [8] = {
	W5500_SOCKET_SEL_ENTRY(0),
	W5500_SOCKET_SEL_ENTRY(1),
	W5500_SOCKET_SEL_ENTRY(2),
	W5500_SOCKET_SEL_ENTRY(3),
	W5500_SOCKET_SEL_ENTRY(4),
	W5500_SOCKET_SEL_ENTRY(5),
	W5500_SOCKET_SEL_ENTRY(6),
	W5500_SOCKET_SEL_ENTRY(7),
};

__always_inline void
wiz_job_set_frame()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t *buf = job->tx - WIZ_SEND_OFFSET;

	buf[0] = job->addr >> 8;
	buf[1] = job->addr & 0xFF;
	buf[2] = job->opmode | (job->rw & WIZ_RX ? W5500_CNTRL_PHASE_READ : W5500_CNTRL_PHASE_WRITE);
}

__always_inline void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (addr, W5500_socket_sel[sock].reg, dat, len);
}

__always_inline void
_dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read (addr, W5500_socket_sel[sock].reg, dat, len);
}

void
wiz_sockets_set (uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM])
{
	uint8_t flag;
	uint_fast8_t sock;

	// initialize all socket memory TX and RX sizes to their corresponding sizes
  for (sock=0; sock<WIZ_MAX_SOCK_NUM; sock++)
	{
		// initialize tx registers
		SSIZE[sock] = (uint16_t)tx_mem[sock] * 0x0400;
		flag = tx_mem[sock];
		_dma_write_sock (sock, WIZ_Sn_TXBUF_SIZE, &flag, 1); // TX_MEMSIZE

		// initialize rx registers
		RSIZE[sock] = (uint16_t)rx_mem[sock] * 0x0400;
		flag = rx_mem[sock];
		_dma_write_sock (sock, WIZ_Sn_RXBUF_SIZE, &flag, 1); // RX_MEMSIZE
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

	// read message
	wiz_job_add(ptr, len, buf, tmp_buf_i, W5500_socket_sel[sock].rx_buf, WIZ_TXRX);

  ptr += len;
	Sn_Rx_RD[sock] = ptr;

	uint8_t *flag = buf+len;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(WIZ_Sn_RX_RD, 2, &flag[0], NULL, W5500_socket_sel[sock].reg, WIZ_TX);

	// send data
	flag[2] = WIZ_Sn_CR_RECV;
	wiz_job_add(WIZ_Sn_CR, 1, &flag[2], NULL, W5500_socket_sel[sock].reg, WIZ_TX);

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

	wiz_job_add(ptr, len, buf, NULL, W5500_socket_sel[sock].tx_buf, WIZ_TX);

  ptr += len;
	Sn_Tx_WR[sock] = ptr;

	uint8_t *flag = buf+len;
	flag[0] = ptr >> 8;
	flag[1] = ptr & 0xFF;
	wiz_job_add(WIZ_Sn_TX_WR, 2, &flag[0], NULL, W5500_socket_sel[sock].reg, WIZ_TX);

	// send data
	flag[2] = WIZ_Sn_CR_SEND;
	wiz_job_add(WIZ_Sn_CR, 1, &flag[2], NULL, W5500_socket_sel[sock].reg, WIZ_TX);

	wiz_job_run_nonblocking();

	return 1;
}
