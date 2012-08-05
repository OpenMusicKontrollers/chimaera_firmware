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

#include <tube.h>
#include <chimutil.h>

#include "udp_private.h"

#include <string.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>

static gpio_dev *ss_dev;
static uint8_t ss_bit;
static uint8_t tmp_buf_o_ptr = 0;

static uint16_t SSIZE [] = {
	0x2000, // 8k for socket 0
	0x0800, // 2k for socket 1
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400 // 1k
};

static uint16_t RSIZE [] = {
	0x2000, // 8k for socket 0
	0x0800, // 2k for socket 1
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400, // 1k
	0x0400 // 1k
};

static uint16_t SBASE [] = {
	TX_BUF_BASE + 0x0000,
	TX_BUF_BASE + 0x2000,
	TX_BUF_BASE + 0x2800,
	TX_BUF_BASE + 0x2c00,
	TX_BUF_BASE + 0x3000,
	TX_BUF_BASE + 0x3400,
	TX_BUF_BASE + 0x3800,
	TX_BUF_BASE + 0x3c00
};

static uint16_t RBASE [] = {
	RX_BUF_BASE + 0x0000,
	RX_BUF_BASE + 0x2000,
	RX_BUF_BASE + 0x2800,
	RX_BUF_BASE + 0x2c00,
	RX_BUF_BASE + 0x3000,
	RX_BUF_BASE + 0x3400,
	RX_BUF_BASE + 0x3800,
	RX_BUF_BASE + 0x3c00
};

static uint16_t SMASK [] = {
	0x2000 - 1,
	0x0800 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1
};

static uint16_t RMASK [] = {
	0x2000 - 1,
	0x0800 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1,
	0x0400 - 1
};

inline void setSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 0);
}

inline void resetSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 1);
}

static uint16_t Sn_Tx_WR[8];

volatile uint8_t spi_dma_done;

static void
_spi_dma_irq (void)
{
	spi_dma_done = 1;
}

inline void
_spi_dma_run (uint16_t len)
{
	spi_dma_done = 0;

	dma_set_num_transfers (DMA1, DMA_CH5, len); // Tx
	dma_set_num_transfers (DMA1, DMA_CH4, len); // Rx

	dma_enable (DMA1, DMA_CH4); // Rx
	dma_enable (DMA1, DMA_CH5); // Tx
}

inline void
_spi_dma_block ()
{
	while (!spi_dma_done)
		;

	dma_disable (DMA1, DMA_CH5); // Tx
	dma_disable (DMA1, DMA_CH4); // Rx
}

void
_dma_write (uint16_t addr, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x80 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	memcpy (buf, dat, len);
	buf += len;

	setSS ();
	_spi_dma_run (buf - buf_o[tmp_buf_o_ptr]);
	_spi_dma_block ();

	// wait for SPI TXE==1
	while ( !(SPI2->regs->SR & SPI_SR_TXE) )
		;

	// wait for SPI BSY==0
	while ( (SPI2->regs->SR & SPI_SR_BSY) )
		;

	resetSS ();
}

uint8_t *
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

uint8_t *
_dma_write_inline (uint8_t *buf, uint16_t addr, uint16_t len)
{
	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x80 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	buf += len; // advance for the data size

	return buf;
}

void
_dma_write_nonblocking_in (uint8_t *buf)
{
	setSS ();
	_spi_dma_run (buf - buf_o[tmp_buf_o_ptr]);
}

void
_dma_write_nonblocking_out ()
{
	_spi_dma_block ();

	// wait for SPI TXE==1
	while ( !(SPI2->regs->SR & SPI_SR_TXE) )
		;

	// wait for SPI BSY==0
	while ( (SPI2->regs->SR & SPI_SR_BSY) )
		;

	resetSS ();
}

void
_dma_read (uint16_t addr, uint8_t *dat, uint16_t len)
{
	uint8_t *buf = buf_o[tmp_buf_o_ptr];
	uint16_t i;

	*buf++ = addr >> 8;
	*buf++ = addr & 0xFF;
	*buf++ = (0x00 | ((len & 0x7F00) >> 8));
	*buf++ = len & 0x00FF;
	
	memset (buf, 0x00, len);
	buf += len;

	setSS ();
	_spi_dma_run (buf-buf_o[tmp_buf_o_ptr]);
	_spi_dma_block ();
	resetSS ();

	memcpy (dat, &buf_i[4], len);
}

void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (CH_BASE + sock*CH_SIZE + addr, dat, len);
}

uint8_t *
_dma_write_sock_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	return _dma_write_append (buf, CH_BASE + sock*CH_SIZE + addr, dat, len);
}

void
_dma_write_sock_16 (uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint8_t flag;
	flag = dat >> 8;
	_dma_write_sock (sock, addr, &flag, 1);
	flag = dat & 0xFF;
	_dma_write_sock (sock, addr+1, &flag, 1);
}

uint8_t *
_dma_write_sock_16_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint8_t flag;
	flag = dat >> 8;
	buf = _dma_write_sock_append (buf, sock, addr, &flag, 1);
	flag = dat & 0xFF;
	return _dma_write_sock_append (buf, sock, addr+1, &flag, 1);
}

void
_dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read (CH_BASE + sock*CH_SIZE + addr, dat, len);
}

void
_dma_read_sock_16 (int8_t sock, uint16_t addr, uint16_t *dat)
{
	uint8_t flag;
	_dma_read_sock (sock, addr, &flag, 1);
	*dat = flag << 8;
	_dma_read_sock (sock, addr+1, &flag, 1);
	*dat += flag;
}

void
udp_init (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet, gpio_dev *dev, uint8_t bit)
{
	int status;

	ss_dev = dev;
	ss_bit = bit;

	// set up dma for SPI2RX
	spi2_rx_tube.tube_dst = buf_i;
	status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH4, DMA_PRIORITY_HIGH);
	dma_attach_interrupt (DMA1, DMA_CH4, _spi_dma_irq);

	// set up dma for SPI2TX
	spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
	status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH5, DMA_PRIORITY_HIGH);

	// init udp
	uint8_t sock;
	uint8_t flag;

	flag = 1 << RST;
	_dma_write (MR, &flag, 1);

	// initialize all socket memory TX and RX sizes to their corresponding sizes
  for (sock=0; sock<MAX_SOCK_NUM; sock++) {
		flag = SSIZE[sock] / 0x0400; // how many kB?
		if (flag == 0x10)
			flag = 0x0f; // special case
		_dma_write_sock (sock, SnTX_MS, &flag, 1); // TX_MEMSIZE
		flag = RSIZE[sock] / 0x0400; // how many kB?
		if (flag == 0x10) 
			flag = 0x0f; // special case
		_dma_write_sock (sock, SnRX_MS, &flag, 1); // RX_MEMSIZE
  }

	// set MAC address of device
	_dma_write (SHAR, mac, 6); //TODO make this configurable

	// set IP of device
	_dma_write (SIPR, ip, 4); //TODO make this configurable

	// set Gateway of device
	_dma_write (GAR, gateway, 4); //TODO make this configurable

	// set Subnet Mask of device
	_dma_write (SUBR, subnet, 4); //TODO make this configurable
}

void
udp_begin (uint8_t sock, uint16_t port)
{
	uint8_t flag;

	// close socket
	flag = SnCR_CLOSE;
	_dma_write_sock (sock, SnCR, &flag, 1);
	do _dma_read_sock (sock, SnSR, &flag, 1);
	while (flag != SnSR_CLOSED);

	// clear interrupt?
	_dma_write_sock (sock, SnIR, &flag, 1);

	// set socket mode to UDP
	flag = SnMR_UDP;
	_dma_write_sock (sock, SnMR, &flag, 1);

	// set outgoing port
	_dma_write_sock_16 (sock, SnPORT, port);

	// open socket
	flag = SnCR_OPEN;
	_dma_write_sock (sock, SnCR, &flag, 1);
	do _dma_read_sock (sock, SnSR, &flag, 1);
	while (flag != SnSR_UDP);

	// get write pointer
	_dma_read_sock_16 (sock, SnTX_WR, &Sn_Tx_WR[sock]);
}

void
udp_set_remote (uint8_t sock, uint8_t *ip, uint16_t port)
{
	// set remote ip
	_dma_write_sock (sock, SnDIPR, ip, 4);

	// set remote port
	_dma_write_sock_16 (sock, SnDPORT, port);
}

void
udp_send (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	if (udp_send_nonblocking (sock, buf_ptr, len))
		udp_send_block (sock);
}

uint8_t 
udp_send_nonblocking (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	if (len > (CHIMAERA_BUFSIZE - 16) ) // return when buffer exceeds given size
		return 0;

	// switch DMA memory source to right buffer
	tmp_buf_o_ptr = buf_ptr;
	spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
	int status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	uint8_t *buf = buf_o[buf_ptr];

	// wait until there is enough space left in buffer XXX we don't need this as we always send stuff immediately and do not accumulate data in the buffer
	/*
	uint16_t free;
	do
		_dma_read_sock_16 (sock, SnTX_FSR, &free);
	while (len > free);
	*/

	// move data to chip
	/* XXX we don't need this as we keep track of the write position with an array instead
	uint16_t ptr;
	_dma_read_sock_16 (sock, SnTX_WR, &ptr);
	*/
	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
	//uint16_t SBASE = TXBUF_BASE + sock*SSIZE; //TODO store this statically in an array per socket
  uint16_t dstAddr = offset + SBASE[sock];

  if ( (offset + len) > SSIZE[sock]) 
  {
    // Wrap around circular buffer
    uint16_t size = SSIZE[sock] - offset;
		buf = _dma_write_inline (buf, dstAddr, size);
		// we have to move the remaining 4 bytes to the right on the buffer to fill in the SPI address commands
		uint16_t i;
		for (i=len-1; i>=size; i--)
			buf[i+4] = buf[i]; //TODO check whether i+4 does not exceed CHIMAERA_BUF_SIZE
		buf = _dma_write_inline (buf, SBASE[sock], len-size);
  } 
  else
		buf = _dma_write_inline (buf, dstAddr, len);

  ptr += len;
	buf = _dma_write_sock_16_append (buf, sock, SnTX_WR, ptr);
	Sn_Tx_WR[sock] = ptr;

	// send data
	uint8_t flag;
	flag = SnCR_SEND;
	buf = _dma_write_sock_append (buf, sock, SnCR, &flag, 1);

	_dma_write_nonblocking_in (buf);

	return 1;
}

void 
udp_send_block (uint8_t sock)
{
	_dma_write_nonblocking_out ();

	uint8_t ir;
	uint8_t flag;
	do
	{
		_dma_read_sock (sock, SnIR, &ir, 1);
		if (ir & SnIR_TIMEOUT)
		{
			flag = SnIR_SEND_OK | SnIR_TIMEOUT;
			_dma_write_sock (sock, SnIR, &flag, 1);
		}
	} while ( (ir & SnIR_SEND_OK) != SnIR_SEND_OK);

	flag = SnIR_SEND_OK;
	_dma_write_sock (sock, SnIR, &flag, 1);
}

uint16_t
udp_available (uint8_t sock)
{
	uint16_t len;
	_dma_read_sock_16 (sock, SnRX_RSR, &len);
	return len;
}

//TODO write a nonblocking version for receiving, too
void
udp_receive (uint8_t sock, uint8_t *buf, uint16_t len)
{
	if (len > (CHIMAERA_BUFSIZE - 4) ) // return when buffer exceedes given size
		return;

	uint16_t ptr;
	_dma_read_sock_16 (sock, SnRX_RD, &ptr);

	uint16_t size;
	uint16_t src_mask;
	uint16_t src_ptr;
	//uint16_t RBASE = RXBUF_BASE + sock*RSIZE;

	src_mask = ptr & RMASK[sock];
	src_ptr = RBASE[sock] + src_mask;

	// read message
	if ( (src_mask + len) > RSIZE[sock])
	{
		size = RSIZE[sock] - src_mask;
		_dma_read (src_ptr, buf, size);
		_dma_read (RBASE[sock], buf+size, len-size);
	}
	else
		_dma_read (src_ptr, buf, len);

	ptr += len;
	_dma_write_sock_16 (sock, SnRX_RD, ptr);

	uint8_t flag;
	flag = SnCR_RECV;
	_dma_write_sock (sock, SnCR, &flag, 1);
}

void 
udp_dispatch (uint8_t sock, uint8_t *buf, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len))
{
	uint16_t len;

	if ( (len = udp_available (sock)) )
	{
		udp_receive (sock, buf, len);

		// separate concurrent UDP messages
		uint16_t remaining = len;
		while (remaining)
		{
			uint8_t *buf_in_ptr = buf + len - remaining;

			uint8_t *ip = buf_in_ptr;
			uint16_t port = (buf_in_ptr[4] << 8) | buf_in_ptr[5];
			uint16_t size = (buf_in_ptr[6] << 8) | buf_in_ptr[7];

			cb (ip, port, buf_in_ptr + UDP_HDR_SIZE, size);

			remaining -= UDP_HDR_SIZE + size;
		}
	}
}
