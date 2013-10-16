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
	uint8_t *buf = job->buf - WIZ_SEND_OFFSET;

	buf[0] = job->addr >> 8;
	buf[1] = job->addr & 0xFF;
	buf[2] = job->opmode;
}

__always_inline void
_dma_write (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = cntrl | W5500_CNTRL_PHASE_WRITE;
	
	memcpy (buf, dat, len);
	buf += len;

	uint16_t buflen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (buflen, WIZ_TX);
	while (!_spi_dma_block (WIZ_TX))
		_spi_dma_run (buflen, WIZ_TX);
	resetSS ();
}

__always_inline uint8_t *
_dma_write_append (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = cntrl | W5500_CNTRL_PHASE_WRITE;
	
	memcpy (buf, dat, len);
	buf += len;

	return buf;
}

__always_inline uint8_t *
_dma_write_inline (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = cntrl | W5500_CNTRL_PHASE_WRITE;
	
	buf += len; // advance for the data size

	return buf;
}

__always_inline void
_dma_read (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = cntrl | W5500_CNTRL_PHASE_READ;
	
	memset (buf, 0x00, len);
	buf += len;

	uint16_t buflen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (buflen, WIZ_TXRX);
	while (!_spi_dma_block (WIZ_TXRX))
		_spi_dma_run (buflen, WIZ_TXRX);
	resetSS ();

	memcpy (dat, &tmp_buf_i[WIZ_SEND_OFFSET], len);
}

__always_inline uint8_t *
_dma_read_append (uint8_t *buf, uint16_t addr, uint8_t cntrl, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = cntrl | W5500_CNTRL_PHASE_READ;
	
	memset (buf, 0x00, len);
	buf += len;

	return buf;
}

__always_inline void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (addr, W5500_socket_sel[sock].reg, dat, len);
}

__always_inline uint8_t *
_dma_write_sock_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	return _dma_write_append (buf, addr, W5500_socket_sel[sock].reg, dat, len);
}

__always_inline void
_dma_write_sock_16 (uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint16_t _dat = hton (dat);
	_dma_write_sock (sock, addr, (uint8_t *)&_dat, 2);
}

__always_inline uint8_t *
_dma_write_sock_16_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint16_t _dat = hton (dat);
	return _dma_write_sock_append (buf, sock, addr, (uint8_t *)&_dat, 2);
}

__always_inline void
_dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read (addr, W5500_socket_sel[sock].reg, dat, len);
}

__always_inline void
_dma_read_sock_16 (int8_t sock, uint16_t addr, uint16_t *dat)
{
	_dma_read_sock (sock, addr, (uint8_t*)dat, 2);
	*dat = hton (*dat);
}

void
wiz_init (gpio_dev *dev, uint8_t bit, uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM])
{
	int status;

	ss_dev = dev;
	ss_bit = bit;

	tmp_buf_i = buf_i_o;

	// set up dma for SPI2RX
	spi2_rx_tube.tube_dst = tmp_buf_i;
	status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	//dma_set_priority (DMA1, DMA_CH4, DMA_PRIORITY_HIGH);
	//nvic_irq_set_priority (NVIC_DMA_CH4, SPI_RX_DMA_PRIORITY);

	// set up dma for SPI2TX
	spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
	status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	//dma_set_priority (DMA1, DMA_CH5, DMA_PRIORITY_HIGH);
	//nvic_irq_set_priority (NVIC_DMA_CH5, SPI_TX_DMA_PRIORITY);

	// init udp
	uint_fast8_t sock;
	uint8_t flag;

	flag = WIZ_MR_RST;
	_dma_write (WIZ_MR, 0, &flag, 1);
	do {
		_dma_read (WIZ_MR, 0, &flag, 1);
	} while (flag & WIZ_MR_RST);

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

uint16_t
udp_receive_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	//TODO
	return 0;
}

uint_fast8_t
udp_receive_block (uint8_t sock, uint16_t size, uint16_t len)
{
	//TODO
	return 0;
}

uint_fast8_t 
udp_send_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	//TODO
	return 0;
}

uint_fast8_t 
udp_send_block (uint8_t sock)
{
	//TODO
	return 0;
}
