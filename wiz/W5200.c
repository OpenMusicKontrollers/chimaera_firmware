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

__always_inline void
wiz_job_set_frame()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t *buf = job->buf - WIZ_SEND_OFFSET;

	buf[0] = job->addr >> 8;
	buf[1] = job->addr & 0xFF;
	buf[2] = (job->rw & WIZ_TX ? 0x80 : 0x00) | ((job->len & 0x7F00) >> 8);
	buf[3] = job->len & 0x00FF;
}

__always_inline void
_dma_write (uint16_t addr, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x80 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
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
_dma_write_append (uint8_t *buf, uint16_t addr, uint8_t *dat, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x80 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	memcpy (buf, dat, len);
	buf += len;

	return buf;
}

__always_inline uint8_t *
_dma_write_inline (uint8_t *buf, uint16_t addr, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x80 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	buf += len; // advance for the data size

	return buf;
}

__always_inline void
_dma_read (uint16_t addr, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x00 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
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
_dma_read_append (uint8_t *buf, uint16_t addr, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x00 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	memset (buf, 0x00, len);
	buf += len;

	return buf;
}

__always_inline void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (CH_BASE + sock*CH_SIZE + addr, dat, len);
}

__always_inline uint8_t *
_dma_write_sock_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	return _dma_write_append (buf, CH_BASE + sock*CH_SIZE + addr, dat, len);
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
	_dma_read (CH_BASE + sock*CH_SIZE + addr, dat, len);
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
	_dma_write (WIZ_MR, &flag, 1);
	do {
		_dma_read (WIZ_MR, &flag, 1);
	} while (flag & WIZ_MR_RST);

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

uint16_t
udp_receive_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	if (len > (CHIMAERA_BUFSIZE - 4) ) // return when buffer exceedes given size
		return;

	if (tmp_buf_o_ptr != buf_ptr)
	{
		tmp_buf_o_ptr = buf_ptr;

		spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
		dma_set_mem_addr(DMA1, DMA_CH5, spi2_tx_tube.tube_dst);
		//int status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
		//ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	}

	tmp_buf_i = buf_i_i;

	spi2_rx_tube.tube_dst = tmp_buf_i;
	dma_set_mem_addr(DMA1, DMA_CH4, spi2_rx_tube.tube_dst);
	//int status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	//ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	uint16_t ptr = Sn_Rx_RD[sock];

	uint16_t size = 0;
	uint16_t offset = ptr & RMASK[sock];
	uint16_t dstAddr = offset + RBASE[sock];

	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	// read message
	if ( (offset + len) > RSIZE[sock])
	{
		size = RSIZE[sock] - offset;
		buf = _dma_read_append (buf, dstAddr, size);
		buf = _dma_read_append (buf, RBASE[sock], len-size);
	}
	else
		buf = _dma_read_append (buf, dstAddr, len);

	//TODO what if something fails when receiving?
	ptr += len;
	Sn_Rx_RD[sock] = ptr;
	buf = _dma_write_sock_16_append (buf, sock, WIZ_Sn_RX_RD, ptr);

	uint8_t flag;
	flag = WIZ_Sn_CR_RECV;
	buf = _dma_write_sock_append (buf, sock, WIZ_Sn_CR, &flag, 1);

	_dma_read_nonblocking_in (buf);

	return size;
}

uint_fast8_t
udp_receive_block (uint8_t sock, uint16_t size, uint16_t len)
{
	uint_fast8_t success = _dma_read_nonblocking_out ();

	if (!success)
	{
		// or do something else?
		_dma_read_sock_16 (sock, WIZ_Sn_RX_RD, &Sn_Rx_RD[sock]);
		return success;
	}

	// remove 4byte padding between chunks
	if (size)
		memmove (tmp_buf_i + WIZ_SEND_OFFSET + size, tmp_buf_i + WIZ_SEND_OFFSET + size + WIZ_SEND_OFFSET, len-size);

	// switch back to RECV buffer
	tmp_buf_i = buf_i_o;

	spi2_rx_tube.tube_dst = tmp_buf_i;
	dma_set_mem_addr(DMA1, DMA_CH4, spi2_rx_tube.tube_dst);
	//int status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	//ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	return success;
}

uint_fast8_t 
udp_send_nonblocking (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	if ( !len || (len > CHIMAERA_BUFSIZE - 2*WIZ_SEND_OFFSET) || (len > SSIZE[sock]) ) // return when buffer exceeds given size
		return 0;

	// switch DMA memory source to right buffer, input buffer is on SEND by default
	if (tmp_buf_o_ptr != buf_ptr)
	{
		tmp_buf_o_ptr = buf_ptr;

		spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
		dma_set_mem_addr(DMA1, DMA_CH5, spi2_tx_tube.tube_dst);
		//int status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
		//ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	}

	uint8_t *buf = buf_o[buf_ptr];

	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
  uint16_t dstAddr = offset + SBASE[sock];

  if ( (offset + len) > SSIZE[sock]) 
  {
    // Wrap around circular buffer
    uint16_t size = SSIZE[sock] - offset;
		buf = _dma_write_inline (buf, dstAddr, size);
		memmove (buf + WIZ_SEND_OFFSET, buf, len-size); // add 4byte padding between chunks
		buf = _dma_write_inline (buf, SBASE[sock], len-size);
  } 
  else
		buf = _dma_write_inline (buf, dstAddr, len);

  ptr += len;
	Sn_Tx_WR[sock] = ptr;
	buf = _dma_write_sock_16_append (buf, sock, WIZ_Sn_TX_WR, ptr);

	// send data
	uint8_t flag;
	flag = WIZ_Sn_CR_SEND;
	buf = _dma_write_sock_append (buf, sock, WIZ_Sn_CR, &flag, 1);

	return _dma_write_nonblocking_in (buf);
}

uint_fast8_t 
udp_send_block (uint8_t sock)
{
	uint_fast8_t success = _dma_write_nonblocking_out ();

	if (!success)
	{
		// or do something else?
		_dma_read_sock_16 (sock, WIZ_Sn_TX_WR, &Sn_Tx_WR[sock]);
		return success;
	}

	uint8_t ir;
	uint8_t flag;
	do
	{
		_dma_read_sock (sock, WIZ_Sn_IR, &ir, 1);
		if (ir & WIZ_Sn_IR_TIMEOUT) // ARPto occured, SEND failed
		{
			flag = WIZ_Sn_IR_SEND_OK | WIZ_Sn_IR_TIMEOUT; // set SEND_OK flag and clear TIMEOUT flag
			_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);
			success = 0;
		}
	} while ( (ir & WIZ_Sn_IR_SEND_OK) != WIZ_Sn_IR_SEND_OK);

	flag = WIZ_Sn_IR_SEND_OK; // clear SEND_OK flag
	_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);

	return success;
}
