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

#define SPI_CMD_SIZE 4

#define INPUT_BUF_SEND 0
#define INPUT_BUF_RECV 1

static gpio_dev *ss_dev;
static uint8_t ss_bit;
static uint8_t tmp_buf_o_ptr = 0;
static uint8_t tmp_buf_i_ptr = INPUT_BUF_SEND;

uint16_t SSIZE [UDP_MAX_SOCK_NUM];
uint16_t RSIZE [UDP_MAX_SOCK_NUM];

uint16_t SBASE [UDP_MAX_SOCK_NUM];
uint16_t RBASE [UDP_MAX_SOCK_NUM];

uint16_t SMASK [UDP_MAX_SOCK_NUM];
uint16_t RMASK [UDP_MAX_SOCK_NUM];

inline void setSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 0);
}

inline void resetSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 1);
}

static uint16_t Sn_Tx_WR[8];

inline void
_spi_dma_run (uint16_t len)
{
	dma_set_num_transfers (DMA1, DMA_CH5, len); // Tx
	dma_set_num_transfers (DMA1, DMA_CH4, len); // Rx

	dma_enable (DMA1, DMA_CH4); // Rx
	dma_enable (DMA1, DMA_CH5); // Tx
}

inline uint8_t
_spi_dma_block ()
{
	uint8_t ret = 1;
	uint8_t isr_rx;
	uint8_t isr_tx;
	uint8_t spi_sr;
	do
	{
		isr_rx = dma_get_isr_bits (DMA1, DMA_CH4);
		isr_tx = dma_get_isr_bits (DMA1, DMA_CH5);
		spi_sr = SPI2_BASE->SR;

		// check for errors in DMA and SPI status registers

		if (isr_rx & 0x8) // RX DMA transfer error
			ret = 0;

		if (isr_tx & 0x8) // TX DMA transfer error
			ret = 0;

		if (spi_sr & SPI_SR_OVR) // SPI overrun error
			ret = 0;

		if (spi_sr & SPI_SR_MODF) // SPI mode fault
			ret = 0;
	} while (!(isr_rx & 0x2) && ret); // RX DMA transfer complete

	dma_clear_isr_bits (DMA1, DMA_CH4);

	dma_disable (DMA1, DMA_CH5); // Tx
	dma_disable (DMA1, DMA_CH4); // Rx

	return ret;
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

	uint16_t buflen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (buflen);
	while (!_spi_dma_block ())
		_spi_dma_run (buflen);

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

static uint16_t nonblocklen;

void
_dma_write_nonblocking_in (uint8_t *buf)
{
	nonblocklen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (nonblocklen);
}

void
_dma_write_nonblocking_out ()
{
	while (!_spi_dma_block ())
		_spi_dma_run (nonblocklen);
	resetSS ();
}

void
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
	_spi_dma_run (buflen);
	while (!_spi_dma_block ())
		_spi_dma_run (buflen);
	resetSS ();

	memcpy (dat, &buf_i[tmp_buf_i_ptr][SPI_CMD_SIZE], len);
}

uint8_t *
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
udp_init (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet, gpio_dev *dev, uint8_t bit, uint8_t tx_mem[UDP_MAX_SOCK_NUM], uint8_t rx_mem[UDP_MAX_SOCK_NUM])
{
	int status;

	ss_dev = dev;
	ss_bit = bit;

	// set up dma for SPI2RX
	spi2_rx_tube.tube_dst = buf_i[tmp_buf_i_ptr];
	status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	//dma_set_priority (DMA1, DMA_CH4, DMA_PRIORITY_HIGH);
	//dma_attach_interrupt (DMA1, DMA_CH4, _spi_dma_irq);
	//nvic_irq_set_priority (NVIC_DMA_CH4, SPI_RX_DMA_PRIORITY);

	// set up dma for SPI2TX
	spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
	status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	//dma_set_priority (DMA1, DMA_CH5, DMA_PRIORITY_HIGH);
	//nvic_irq_set_priority (NVIC_DMA_CH5, SPI_TX_DMA_PRIORITY);

	// init udp
	uint8_t sock;
	uint8_t flag;

	flag = 1 << UDP_RESET;
	_dma_write (MR, &flag, 1);

	// initialize all socket memory TX and RX sizes to their corresponding sizes
  for (sock=0; sock<UDP_MAX_SOCK_NUM; sock++)
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
		_dma_write_sock (sock, SnTX_MS, &flag, 1); // TX_MEMSIZE

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
		_dma_write_sock (sock, SnRX_MS, &flag, 1); // RX_MEMSIZE
  }

	udp_mac_set (mac);
	udp_ip_set (ip);
	udp_gateway_set (gateway);
	udp_subnet_set (subnet);
}

void
udp_mac_set (uint8_t *mac)
{
	_dma_write (SHAR, mac, 6);
}

void
udp_ip_set (uint8_t *ip)
{
	_dma_write (SIPR, ip, 4);
}

void
udp_gateway_set (uint8_t *gateway)
{
	_dma_write (GAR, gateway, 4);
}

void
udp_subnet_set (uint8_t *subnet)
{
	_dma_write (SUBR, subnet, 4);
}

void
udp_begin (uint8_t sock, uint16_t port, uint8_t multicast)
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
	if (multicast)
		flag = SnMR_UDP | SnMR_MULTI;
	else
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
udp_set_remote_har (uint8_t sock, uint8_t *har)
{
	// remote hardware address, e.g. needed for UDP multicast
	_dma_write_sock (sock, SnDHAR, har, 6);
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
	if (len > (CHIMAERA_BUFSIZE - 2*UDP_SEND_OFFSET) ) // return when buffer exceeds given size
		return 0;

	// switch DMA memory source to right buffer, input buffer is on SEND by default
	if (tmp_buf_o_ptr != buf_ptr)
	{
		tmp_buf_o_ptr = buf_ptr;

		spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
		int status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
		ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	}

	uint8_t *buf = buf_o[buf_ptr];

	// wait until there is enough space left in buffer
	/* XXX we don't need this as we always send stuff immediately and do not accumulate data in the buffer
	uint16_t frei;
	do
		_dma_read_sock_16 (sock, SnTX_FSR, &frei);
	while (len > frei);
	*/

	// move data to chip
	/* XXX we don't need this as we keep track of the write position with an array instead
	uint16_t ptr;
	_dma_read_sock_16 (sock, SnTX_WR, &ptr);
	*/
	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
  uint16_t dstAddr = offset + SBASE[sock];

  if ( (offset + len) > SSIZE[sock]) 
  {
    // Wrap around circular buffer
    uint16_t size = SSIZE[sock] - offset;
		buf = _dma_write_inline (buf, dstAddr, size);
		// we have to move the remaining data 4 bytes to the right on the buffer to fill in the SPI address commands
		uint16_t i;
		for (i=len-size; i>0; i--)
			buf[i - 1 + UDP_SEND_OFFSET] = buf[i-1];
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
udp_probe (uint8_t sock)
{
	// send data
	uint8_t flag;
	flag = SnCR_SEND;
	_dma_write_sock (sock, SnCR, &flag, 1);

	uint8_t ir;
	do
	{
		_dma_read_sock (sock, SnIR, &ir, 1);
		if (ir & SnIR_TIMEOUT) // ARPto occured, SEND failed
		{
			flag = SnIR_SEND_OK | SnIR_TIMEOUT;
			_dma_write_sock (sock, SnIR, &flag, 1);
		}
	} while ( (ir & SnIR_SEND_OK) != SnIR_SEND_OK);

	flag = SnIR_SEND_OK;
	_dma_write_sock (sock, SnIR, &flag, 1);
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
		if (ir & SnIR_TIMEOUT) // ARPto occured, SEND failed
		{
			flag = SnIR_SEND_OK | SnIR_TIMEOUT;
			_dma_write_sock (sock, SnIR, &flag, 1);
		}
	} while ( (ir & SnIR_SEND_OK) != SnIR_SEND_OK);

	flag = SnIR_SEND_OK;
	_dma_write_sock (sock, SnIR, &flag, 1);

	//FIXME remove stuff below
	// update write pointer
	_dma_read_sock_16 (sock, SnTX_WR, &Sn_Tx_WR[sock]);

	// wait until everything is sent to be ready for next data
	uint16_t frei;
	do
		_dma_read_sock_16 (sock, SnTX_FSR, &frei);
	while (frei != SSIZE[sock]);
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
udp_receive (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	if (len > (CHIMAERA_BUFSIZE - 4) ) // return when buffer exceedes given size
		return;

	if (tmp_buf_o_ptr != buf_ptr)
	{
		tmp_buf_o_ptr = buf_ptr;

		spi2_tx_tube.tube_dst = buf_o[tmp_buf_o_ptr];
		int status = dma_tube_cfg (DMA1, DMA_CH5, &spi2_tx_tube);
		ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	}

	tmp_buf_i_ptr = INPUT_BUF_RECV;

	spi2_rx_tube.tube_dst = buf_i[tmp_buf_i_ptr];
	int status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	uint16_t ptr;
	_dma_read_sock_16 (sock, SnRX_RD, &ptr); // TODO store this offline

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

	_dma_write_nonblocking_in (buf);
	_dma_write_nonblocking_out ();

	if (size)
		memcpy (buf_i[tmp_buf_i_ptr] + SPI_CMD_SIZE + len - size, buf_i[tmp_buf_i_ptr] + SPI_CMD_SIZE + len - size + SPI_CMD_SIZE, size);

	// switch back to RECV buffer
	tmp_buf_i_ptr = INPUT_BUF_SEND;

	spi2_rx_tube.tube_dst = buf_i[tmp_buf_i_ptr];
	status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	ptr += len;
	_dma_write_sock_16 (sock, SnRX_RD, ptr);

	uint8_t flag;
	flag = SnCR_RECV;
	_dma_write_sock (sock, SnCR, &flag, 1);
}

void 
udp_dispatch (uint8_t sock, uint8_t ptr, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len))
{
	uint16_t len;

	if ( (len = udp_available (sock)) )
	{
		udp_receive (sock, ptr, len);

		// separate concurrent UDP messages
		uint16_t remaining = len;
		while (remaining > 0)
		{
			uint8_t *buf_in_ptr = buf_i[INPUT_BUF_RECV] + SPI_CMD_SIZE + len - remaining;

			uint8_t ip [4];
			ip[0] = buf_in_ptr[0];
			ip[1] = buf_in_ptr[1];
			ip[2] = buf_in_ptr[2];
			ip[3] = buf_in_ptr[3];
			uint16_t port = (buf_in_ptr[4] << 8) | buf_in_ptr[5];
			uint16_t size = (buf_in_ptr[6] << 8) | buf_in_ptr[7];

			cb ((uint8_t *)&ip, port, buf_in_ptr + UDP_HDR_SIZE, size);

			remaining -= UDP_HDR_SIZE + size;
		}
	}
}
