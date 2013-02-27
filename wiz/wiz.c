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

#include "wiz_private.h"

#include <string.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>
#include <libmaple/systick.h>

#define SPI_CMD_SIZE 4

#define INPUT_BUF_SEND 0
#define INPUT_BUF_RECV 1

static gpio_dev *ss_dev;
static uint8_t ss_bit;
static uint8_t tmp_buf_o_ptr = 0;
static uint8_t tmp_buf_i_ptr = INPUT_BUF_SEND;

static uint16_t SSIZE [WIZ_MAX_SOCK_NUM];
static uint16_t RSIZE [WIZ_MAX_SOCK_NUM];

static uint16_t SBASE [WIZ_MAX_SOCK_NUM];
static uint16_t RBASE [WIZ_MAX_SOCK_NUM];

static uint16_t SMASK [WIZ_MAX_SOCK_NUM];
static uint16_t RMASK [WIZ_MAX_SOCK_NUM];

inline void setSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 0);
}

inline void resetSS ()
{
	gpio_write_bit (ss_dev, ss_bit, 1);
}

static uint16_t Sn_Tx_WR[8];
static uint16_t Sn_Rx_RD[8];

inline void
_spi_dma_run (uint16_t len, uint8_t io_flags)
{
	// when only sending, we don't need to read RX
	if (io_flags & WIZ_RX)
	{
		dma_set_num_transfers (DMA1, DMA_CH4, len); // Rx
		dma_enable (DMA1, DMA_CH4); // Rx
	}

	if (io_flags & WIZ_TX)
	{
		dma_set_num_transfers (DMA1, DMA_CH5, len); // Tx
		dma_enable (DMA1, DMA_CH5); // Tx
	}
}

inline uint8_t
_spi_dma_block (uint8_t io_flags)
{
	uint8_t ret = 1;
	uint8_t isr_rx;
	uint8_t isr_tx;
	uint8_t spi_sr;

	if (io_flags == WIZ_SENDRECV)
	{
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
		dma_clear_isr_bits (DMA1, DMA_CH5);

		dma_disable (DMA1, DMA_CH5); // Tx
		dma_disable (DMA1, DMA_CH4); // Rx
	}
	else if (io_flags == WIZ_SENDONLY)
	{
		do
		{
			isr_tx = dma_get_isr_bits (DMA1, DMA_CH5);
			spi_sr = SPI2_BASE->SR;

			if (isr_tx & 0x8) // TX DMA transfer error
				ret = 0;

			// SPI_SR_OVR is set when not reading RX

			if (spi_sr & SPI_SR_MODF)
				ret = 0;
		} while (!(isr_tx & 0x2) && !(spi_sr & SPI_SR_TXE) && (spi_sr & SPI_SR_BSY) && ret); // TX DMA transfer complete

		dma_clear_isr_bits (DMA1, DMA_CH5);
		dma_disable (DMA1, DMA_CH5); // Tx
	}

	return ret;
}

inline void
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
	_spi_dma_run (buflen, WIZ_SENDONLY);
	while (!_spi_dma_block (WIZ_SENDONLY))
		_spi_dma_run (buflen, WIZ_SENDONLY);

	resetSS ();
}

inline uint8_t *
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

inline uint8_t *
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

inline void
_dma_write_nonblocking_in (uint8_t *buf)
{
	nonblocklen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (nonblocklen, WIZ_SENDONLY);
}

inline void
_dma_write_nonblocking_out ()
{
	while (!_spi_dma_block (WIZ_SENDONLY))
		_spi_dma_run (nonblocklen, WIZ_SENDONLY);
	resetSS ();
}

inline void
_dma_read_nonblocking_in (uint8_t *buf)
{
	nonblocklen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (nonblocklen, WIZ_SENDRECV);
}

inline void
_dma_read_nonblocking_out ()
{
	while (!_spi_dma_block (WIZ_SENDRECV))
		_spi_dma_run (nonblocklen, WIZ_SENDRECV);
	resetSS ();
}

inline void
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
	_spi_dma_run (buflen, WIZ_SENDRECV);
	while (!_spi_dma_block (WIZ_SENDRECV))
		_spi_dma_run (buflen, WIZ_SENDRECV);
	resetSS ();

	memcpy (dat, &buf_i[tmp_buf_i_ptr][SPI_CMD_SIZE], len);
}

inline uint8_t *
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

inline void
_dma_write_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_write (CH_BASE + sock*CH_SIZE + addr, dat, len);
}

inline uint8_t *
_dma_write_sock_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	return _dma_write_append (buf, CH_BASE + sock*CH_SIZE + addr, dat, len);
}

inline void
_dma_write_sock_16 (uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint16_t _dat = hton (dat);
	_dma_write_sock (sock, addr, (uint8_t *)&_dat, 2);
}

inline uint8_t *
_dma_write_sock_16_append (uint8_t *buf, uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint16_t _dat = hton (dat);
	return _dma_write_sock_append (buf, sock, addr, (uint8_t *)&_dat, 2);
}

inline void
_dma_read_sock (uint8_t sock, uint16_t addr, uint8_t *dat, uint16_t len)
{
	// transform relative socket registry address to absolute registry address
	_dma_read (CH_BASE + sock*CH_SIZE + addr, dat, len);
}

inline void
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

	flag = 1 << WIZ_RESET;
	_dma_write (MR, &flag, 1);

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
}

uint8_t
wiz_link_up ()
{
	uint8_t physr;
	_dma_read (PHYSR, &physr, 1);
	return ( (physr & PHYSR_LINK) == PHYSR_LINK);
}

void
wiz_mac_set (uint8_t *mac)
{
	_dma_write (SHAR, mac, 6);
}

void
wiz_ip_set (uint8_t *ip)
{
	_dma_write (SIPR, ip, 4);
}

void
wiz_gateway_set (uint8_t *gateway)
{
	_dma_write (GAR, gateway, 4);
}

void
wiz_subnet_set (uint8_t *subnet)
{
	_dma_write (SUBR, subnet, 4);
}

void wiz_comm_set (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet)
{
	wiz_mac_set (mac);
	wiz_ip_set (ip);
	wiz_gateway_set (gateway);
	wiz_subnet_set (subnet);
}

void
udp_end (uint8_t sock)
{
	uint8_t flag;

	// close socket
	flag = SnCR_CLOSE;
	_dma_write_sock (sock, SnCR, &flag, 1);
	do _dma_read_sock (sock, SnSR, &flag, 1);
	while (flag != SnSR_CLOSED);
}

void
udp_begin (uint8_t sock, uint16_t port, uint8_t multicast)
{
	uint8_t flag;

	// first close socket
	udp_end (sock);

	// clear socket interrupt register
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
	while (flag != SnSR_UDP)
		;

	// get write pointer
	_dma_read_sock_16 (sock, SnTX_WR, &Sn_Tx_WR[sock]);

	// get read pointer
	_dma_read_sock_16 (sock, SnRX_RD, &Sn_Rx_RD[sock]);
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

uint8_t
udp_send (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	uint8_t success = 0;
	if (udp_send_nonblocking (sock, buf_ptr, len))
		success = udp_send_block (sock);
	return success;
}

uint8_t 
udp_send_nonblocking (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	if ( !len || (len > CHIMAERA_BUFSIZE - 2*WIZ_SEND_OFFSET) || (len > SSIZE[sock]) ) // return when buffer exceeds given size
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

	uint16_t ptr = Sn_Tx_WR[sock];
  uint16_t offset = ptr & SMASK[sock];
  uint16_t dstAddr = offset + SBASE[sock];

  if ( (offset + len) > SSIZE[sock]) 
  {
    // Wrap around circular buffer
    uint16_t size = SSIZE[sock] - offset;
		buf = _dma_write_inline (buf, dstAddr, size);
		memmove (buf + SPI_CMD_SIZE, buf, len-size); // add 4byte padding between chunks
		buf = _dma_write_inline (buf, SBASE[sock], len-size);
  } 
  else
		buf = _dma_write_inline (buf, dstAddr, len);

  ptr += len;
	Sn_Tx_WR[sock] = ptr;
	buf = _dma_write_sock_16_append (buf, sock, SnTX_WR, ptr);

	// send data
	uint8_t flag;
	flag = SnCR_SEND;
	buf = _dma_write_sock_append (buf, sock, SnCR, &flag, 1);

	_dma_write_nonblocking_in (buf);

	return 1;
}

uint8_t 
udp_send_block (uint8_t sock)
{
	_dma_write_nonblocking_out ();

	uint8_t success = 1;
	uint8_t ir;
	uint8_t flag;
	do
	{
		_dma_read_sock (sock, SnIR, &ir, 1);
		if (ir & SnIR_TIMEOUT) // ARPto occured, SEND failed
		{
			flag = SnIR_SEND_OK | SnIR_TIMEOUT; // set SEND_OK flag and clear TIMEOUT flag
			_dma_write_sock (sock, SnIR, &flag, 1);
			success = 0;
		}
	} while ( (ir & SnIR_SEND_OK) != SnIR_SEND_OK);

	flag = SnIR_SEND_OK; // clear SEND_OK flag
	_dma_write_sock (sock, SnIR, &flag, 1);

	return success;
}

uint16_t
udp_available (uint8_t sock)
{
	uint16_t len;
	_dma_read_sock_16 (sock, SnRX_RSR, &len);
	return len;
}

void
udp_receive (uint8_t sock, uint8_t buf_ptr, uint16_t len)
{
	uint16_t wrap = udp_receive_nonblocking (sock, buf_ptr, len);
	udp_receive_block (sock, wrap, len);
}

uint16_t
udp_receive_nonblocking (uint8_t sock, uint8_t buf_ptr, uint16_t len)
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
	buf = _dma_write_sock_16_append (buf, sock, SnRX_RD, ptr);

	uint8_t flag;
	flag = SnCR_RECV;
	buf = _dma_write_sock_append (buf, sock, SnCR, &flag, 1);

	_dma_read_nonblocking_in (buf);

	return size;
}

void
udp_receive_block (uint8_t sock, uint16_t size, uint16_t len)
{
	_dma_read_nonblocking_out ();

	// remove 4byte padding between chunks
	if (size)
		memmove (buf_i[tmp_buf_i_ptr] + SPI_CMD_SIZE + size, buf_i[tmp_buf_i_ptr] + SPI_CMD_SIZE + size + SPI_CMD_SIZE, len-size);

	// switch back to RECV buffer
	tmp_buf_i_ptr = INPUT_BUF_SEND;

	spi2_rx_tube.tube_dst = buf_i[tmp_buf_i_ptr];
	int status = dma_tube_cfg (DMA1, DMA_CH4, &spi2_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
}

void
_wiz_advance_read_ptr (uint8_t sock, uint16_t len)
{
	uint16_t ptr = Sn_Rx_RD[sock];

	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	ptr += len;
	Sn_Rx_RD[sock] = ptr;
	buf = _dma_write_sock_16_append (buf, sock, SnRX_RD, ptr);

	uint8_t flag;
	flag = SnCR_RECV;
	buf = _dma_write_sock_append (buf, sock, SnCR, &flag, 1);

	_dma_read_nonblocking_in (buf);
	_dma_read_nonblocking_out ();
}

void 
udp_dispatch (uint8_t sock, uint8_t ptr, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len))
{
	uint16_t len;

	if ( (len = udp_available (sock)) )
	{
		// we cannot handle compound packets if they are too long, ignore them by now, FIXME try to read them separately
		if ( (len > CHIMAERA_BUFSIZE) || (len > RSIZE[sock]) )
		{
			//debug_str ("udp message too long to dispatch");
			//debug_int32 (len);
			_wiz_advance_read_ptr (sock, len);
			return;
		}

		udp_receive (sock, ptr, len);

		// separate concurrent UDP messages
		uint16_t remaining = len;
		while (remaining > 0)
		{
			uint8_t *buf_in_ptr = buf_i[INPUT_BUF_RECV] + SPI_CMD_SIZE + len - remaining;

			uint8_t *ip = &buf_in_ptr[0];
			uint16_t port = (buf_in_ptr[4] << 8) | buf_in_ptr[5];
			uint16_t size = (buf_in_ptr[6] << 8) | buf_in_ptr[7];

			cb (ip, port, buf_in_ptr + WIZ_UDP_HDR_SIZE, size);

			remaining -= WIZ_UDP_HDR_SIZE + size;
		}
	}
}

void
macraw_begin (uint8_t sock, uint8_t mac_filter)
{
	uint8_t flag;

	// first close socket
	macraw_end (sock);

	// clear socket interrupt register
	_dma_write_sock (sock, SnIR, &flag, 1);

	// set socket mode to MACRAW
	if (mac_filter) // only look for packages addressed to our MAC or broadcast
		flag = SnMR_MACRAW | SnMR_MF;
	else
		flag = SnMR_MACRAW;
	_dma_write_sock (sock, SnMR, &flag, 1);

	// open socket
	flag = SnCR_OPEN;
	_dma_write_sock (sock, SnCR, &flag, 1);
	do _dma_read_sock (sock, SnSR, &flag, 1);
	while (flag != SnSR_MACRAW)
		;

	// get write pointer
	_dma_read_sock_16 (sock, SnTX_WR, &Sn_Tx_WR[sock]);

	// get read pointer
	_dma_read_sock_16 (sock, SnRX_RD, &Sn_Rx_RD[sock]);
}

void
macraw_end (uint8_t sock)
{
	udp_end (sock);
}

uint8_t
macraw_send (uint8_t sock, uint8_t ptr, uint16_t len)
{
	return udp_send (sock, ptr, len);
}

uint16_t
macraw_available (uint8_t sock)
{
	return udp_available (sock);
}

void
macraw_receive (uint8_t sock, uint8_t ptr, uint16_t len)
{
	udp_receive (sock, ptr, len);
}

void macraw_dispatch (uint8_t sock, uint8_t ptr, MACRAW_Dispatch_Cb cb, void *user_data)
{
	uint16_t len;

	if ( (len = macraw_available (sock)) )
	{
		// we cannot handle compound packets if they are too long, ignore them by now, FIXME try to read them separately
		if ( (len > CHIMAERA_BUFSIZE) || (len > RSIZE[sock]) )
		{
			//debug_str ("udp message too long to dispatch");
			//debug_int32 (len);
			_wiz_advance_read_ptr (sock, len);
			return;
		}

		macraw_receive (sock, ptr, len);

		// separate concurrent MACRAW packets
		uint16_t remaining = len;
		while (remaining > 0)
		{
			uint8_t *buf_in_ptr = buf_i[INPUT_BUF_RECV] + SPI_CMD_SIZE + len - remaining;
			uint16_t size = (buf_in_ptr[0] << 8) | buf_in_ptr[1];

			//debug_str ("macraw_dispatch"); // FIXME  remove debug
			//debug_int32 (len);
			//debug_int32 (size);
			cb (buf_in_ptr+2, size-2, user_data);

			//remaining -= 2 + size + 4; // -(packet_info + packet_size + CRC)
			remaining -= size; // -(packet_info + packet_size + CRC)
		}
	}
}
