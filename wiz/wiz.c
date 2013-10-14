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

#if defined(WIZ_CHIP_W5200)
#	include "W5200_private.h"
#elif defined(WIZ_CHIP_W5500)
#	include "W5500_private.h"
#endif

#include <tube.h>
#include <netdef.h>
#include <chimaera.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>
#include <libmaple/systick.h>

Wiz_Job wiz_jobs [WIZ_MAX_JOB_NUM];
volatile uint_fast8_t wiz_jobs_todo;
volatile uint_fast8_t wiz_jobs_done;

const uint8_t wiz_nil_ip [] = {0x00, 0x00, 0x00, 0x00};
const uint8_t wiz_nil_mac [] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t wiz_broadcast_mac [] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

gpio_dev *ss_dev;
uint8_t ss_bit;
uint_fast8_t tmp_buf_o_ptr = 0;
uint8_t *tmp_buf_i = buf_i_o;

uint16_t nonblocklen;

Wiz_IRQ_Cb irq_cb = NULL;
Wiz_IRQ_Cb irq_socket_cb [WIZ_MAX_SOCK_NUM];

uint16_t SSIZE [WIZ_MAX_SOCK_NUM];
uint16_t RSIZE [WIZ_MAX_SOCK_NUM];

uint16_t Sn_Tx_WR[WIZ_MAX_SOCK_NUM];
uint16_t Sn_Rx_RD[WIZ_MAX_SOCK_NUM];

static void
_wiz_job_irq() //FIXME handle SPI and DMA errors
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_todo++];
	uint8_t isr_rx;
	uint8_t isr_tx;
	uint8_t spi_sr;

	if(job->rw == WIZ_TXRX)
	{
		isr_rx = dma_get_isr_bits(DMA1, DMA_CH4);
		isr_tx = dma_get_isr_bits(DMA1, DMA_CH5);

		if(isr_rx & DMA_ISR_TCIF) // RX DMA transfer complete
		{
			dma_clear_isr_bits(DMA1, DMA_CH5);
			dma_clear_isr_bits(DMA1, DMA_CH4);

			dma_disable(DMA1, DMA_CH5); // Tx
			dma_disable(DMA1, DMA_CH4); // Rx

			resetSS();

			if(++wiz_jobs_done < wiz_jobs_todo)
				return wiz_job_run_single();
		}
	}
	else if(job->rw & WIZ_TX)
	{
		isr_tx = dma_get_isr_bits(DMA1, DMA_CH5);

		if(isr_tx & DMA_ISR_TCIF) // Tx DMA transfer complete
		{
			do {
				spi_sr = SPI2_BASE->SR;
			} while (spi_sr & SPI_SR_FTLVL); // TX FIFO not empty

			do {
				spi_sr = SPI2_BASE->SR;
			} while (spi_sr & SPI_SR_BSY); // wait for BSY==0

			do
			{
				//spi_sr = SPI2_BASE->DR;
				spi_sr = spi_rx_reg(SPI2);
				spi_sr = SPI2_BASE->SR;
			} while ( (spi_sr & SPI_SR_FRLVL) || (spi_sr & SPI_SR_RXNE) || (spi_sr & SPI_SR_OVR) ); // empty buffer and clear OVR flag

			dma_clear_isr_bits(DMA1, DMA_CH5);

			dma_disable(DMA1, DMA_CH5); // Tx

			resetSS();

			if(++wiz_jobs_done < wiz_jobs_todo)
				return wiz_job_run_single();
		}
	}
}

void
wiz_job_clear()
{
	wiz_jobs_todo = 0;
}

void
wiz_job_add(uint16_t addr, uint16_t len, uint8_t *buf, uint8_t opmode, uint8_t rw)
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_todo++];
	job->addr = addr;
	job->len = len;
	job->buf = buf;
	job->opmode = opmode;
	job->rw = rw;
}

void
wiz_job_run_single()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];

	wiz_job_set_frame();

	uint16_t len2 = job->len + WIZ_SEND_OFFSET;

	setSS();

	if(job->rw & WIZ_RX)
	{
		spi_rx_dma_enable(SPI2); // enable RX DMA on SPI2
		//dma_set_mem_address(DMA1, DMA_CH4, job->buf - WIZ_SEND_OFFSET); TODO?
		dma_set_num_transfers(DMA1, DMA_CH4, len2); // Rx
		dma_enable(DMA1, DMA_CH4); // Rx
	}
	else
		spi_rx_dma_disable(SPI2); // disable RX DMA on SPI2

	if(job->rw & WIZ_TX)
	{
		spi_tx_dma_enable(SPI2); // enable TX DMA on SPI2
		dma_set_mem_address(DMA1, DMA_CH5, job->buf - WIZ_SEND_OFFSET);
		dma_set_num_transfers(DMA1, DMA_CH5, len2); // Tx
		dma_enable(DMA1, DMA_CH5); // Tx
	}
	else
		spi_tx_dma_disable(SPI2); // disable TX DMA on SPI2
}

void
wiz_job_run_nonblocking()
{
	if(!wiz_jobs_todo)
		return;

	dma_attach_interrupt(DMA1, DMA_CH5, _wiz_job_irq);
	wiz_jobs_done = 0;
	wiz_job_run_single();
}

void
wiz_job_run_block()
{
	while(wiz_jobs_done < wiz_jobs_todo)
		;

	dma_detach_interrupt(DMA1, DMA_CH5);
}

void
_spi_dma_run (uint16_t len, uint8_t io_flags)
{
	if (io_flags & WIZ_RX)
	{
		spi_rx_dma_enable (SPI2); // Enables RX DMA on SPI2
		dma_set_num_transfers (DMA1, DMA_CH4, len); // Rx

		dma_enable (DMA1, DMA_CH4); // Rx
	}
	else
		spi_rx_dma_disable (SPI2);

	if (io_flags & WIZ_TX)
	{
		spi_tx_dma_enable (SPI2); // Enables TX DMA on SPI2
		dma_set_num_transfers (DMA1, DMA_CH5, len); // Tx
		dma_enable (DMA1, DMA_CH5); // Tx
	}
	else
		spi_tx_dma_disable (SPI2);
}

uint_fast8_t
_spi_dma_block (uint8_t io_flags)
{
	uint_fast8_t ret = 1;
	uint8_t isr_rx;
	uint8_t isr_tx;
	uint16_t spi_sr;

	if (io_flags == WIZ_TXRX)
	{
		do
		{
			isr_rx = dma_get_isr_bits (DMA1, DMA_CH4);
			isr_tx = dma_get_isr_bits (DMA1, DMA_CH5);
			spi_sr = SPI2_BASE->SR;

			// check for errors in DMA and SPI status registers
			if (isr_rx & DMA_ISR_TEIF) // RX DMA transfer error
				ret = 0;

			if (isr_tx & DMA_ISR_TEIF) // TX DMA transfer error
				ret = 0;

			if (spi_sr & SPI_SR_OVR) // SPI overrun error
				ret = 0;

			if (spi_sr & SPI_SR_MODF) // SPI mode fault
				ret = 0;
		} while (!(isr_rx & DMA_ISR_TCIF) && ret); // RX DMA transfer complete

		dma_clear_isr_bits (DMA1, DMA_CH4);
		dma_clear_isr_bits (DMA1, DMA_CH5);

		dma_disable (DMA1, DMA_CH5); // Tx
		dma_disable (DMA1, DMA_CH4); // Rx
	}
	else if (io_flags & WIZ_TX)
	{
		do
		{
			isr_tx = dma_get_isr_bits (DMA1, DMA_CH5);
			spi_sr = SPI2_BASE->SR;

			// check for errors in DMA and SPI status registers
			if (isr_tx & DMA_ISR_TEIF) // TX DMA transfer error
				ret = 0;

			if (spi_sr & SPI_SR_MODF) // SPI mode fault
				ret = 0;
		} while (!(isr_tx & DMA_ISR_TCIF) && ret); // TX DMA transfer complete

		do {
			spi_sr = SPI2_BASE->SR;
		} while ( (spi_sr & SPI_SR_FTLVL) && ret); // TX FIFO not empty

		do {
			spi_sr = SPI2_BASE->SR;
		} while ( (spi_sr & SPI_SR_BSY) && ret); // wait for BSY==0

		do
		{
			//spi_sr = SPI2_BASE->DR;
			spi_sr = spi_rx_reg(SPI2);
			spi_sr = SPI2_BASE->SR;
		} while ( (spi_sr & SPI_SR_FRLVL) || (spi_sr & SPI_SR_RXNE) || (spi_sr & SPI_SR_OVR) ); // empty buffer and clear OVR flag

		dma_clear_isr_bits (DMA1, DMA_CH5);

		dma_disable (DMA1, DMA_CH5); // Tx
	}

	return ret;
}

__always_inline uint_fast8_t
_dma_write_nonblocking_in (uint8_t *buf)
{
	nonblocklen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (nonblocklen, WIZ_TX);
	return 1;
}

__always_inline uint_fast8_t
_dma_write_nonblocking_out ()
{
	uint_fast8_t res = _spi_dma_block (WIZ_TX);
	resetSS ();
	return res;
}

__always_inline uint_fast8_t
_dma_read_nonblocking_in (uint8_t *buf)
{
	nonblocklen = buf - buf_o[tmp_buf_o_ptr];
	setSS ();
	_spi_dma_run (nonblocklen, WIZ_TXRX);
	return 1;
}

__always_inline uint_fast8_t
_dma_read_nonblocking_out ()
{
	uint_fast8_t res = _spi_dma_block (WIZ_TXRX);
	resetSS ();
	return res;
}

//FIXME move

uint_fast8_t
wiz_link_up ()
{
	uint8_t physr;
	_dma_read (WIZ_PHYCFGR, &physr, 1);
	return ( (physr & WIZ_PHYCFGR_LNK) == WIZ_PHYCFGR_LNK);
}

void
wiz_mac_set (uint8_t *mac)
{
	_dma_write (WIZ_SHAR, mac, 6);
}

void
wiz_ip_set (uint8_t *ip)
{
	_dma_write (WIZ_SIPR, ip, 4);
}

void
wiz_gateway_set (uint8_t *gateway)
{
	_dma_write (WIZ_GAR, gateway, 4);
}

void
wiz_subnet_set (uint8_t *subnet)
{
	_dma_write (WIZ_SUBR, subnet, 4);
}

void
wiz_comm_set (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet)
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
	flag = WIZ_Sn_CR_CLOSE;
	_dma_write_sock (sock, WIZ_Sn_CR, &flag, 1);
	do _dma_read_sock (sock, WIZ_Sn_SR, &flag, 1);
	while (flag != WIZ_Sn_SR_CLOSED);
}

void
udp_begin (uint8_t sock, uint16_t port, uint_fast8_t multicast)
{
	uint8_t flag;

	// first close socket
	udp_end (sock);

	// clear socket interrupt register
	_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);

	// set socket mode to UDP
	if (multicast)
		flag = WIZ_Sn_MR_UDP | WIZ_Sn_MR_MULTI;
	else
		flag = WIZ_Sn_MR_UDP;
	_dma_write_sock (sock, WIZ_Sn_MR, &flag, 1);

	// set outgoing port
	_dma_write_sock_16 (sock, WIZ_Sn_PORT, port);

	// open socket
	flag = WIZ_Sn_CR_OPEN;
	_dma_write_sock (sock, WIZ_Sn_CR, &flag, 1);
	do
		_dma_read_sock (sock, WIZ_Sn_SR, &flag, 1);
	while (flag != WIZ_Sn_SR_UDP);

	udp_update_read_write_pointers (sock);
}

__always_inline void
udp_update_read_write_pointers (uint8_t sock)
{
	// get write pointer
	_dma_read_sock_16 (sock, WIZ_Sn_TX_WR, &Sn_Tx_WR[sock]);

	// get read pointer
	_dma_read_sock_16 (sock, WIZ_Sn_RX_RD, &Sn_Rx_RD[sock]);
}

__always_inline void
udp_reset_read_write_pointers (uint8_t sock)
{
	_dma_read_sock_16 (sock, WIZ_Sn_TX_RD, &Sn_Tx_WR[sock]);
	_dma_write_sock_16 (sock, WIZ_Sn_TX_WR, Sn_Tx_WR[sock]);

	_dma_read_sock_16 (sock, WIZ_Sn_RX_WR, &Sn_Rx_RD[sock]);
	_dma_write_sock_16 (sock, WIZ_Sn_RX_RD, Sn_Rx_RD[sock]);
}

uint_fast8_t
wiz_is_broadcast(uint8_t *ip)
{
	uint32_t *ip32 = (uint32_t *)ip;
	return *ip32 == 0xffffffff;
}

uint_fast8_t
wiz_is_multicast(uint8_t *ip)
{
	return (ip[0] & 0b11110000) == 0b11100000; // is it of the form: 0b1110xxxx?
}

void
udp_set_remote (uint8_t sock, uint8_t *ip, uint16_t port)
{
	if(wiz_is_multicast(ip))
	{
		uint8_t multicast_mac [6];

		multicast_mac[0] = 0x01;
		multicast_mac[1] = 0x00;
		multicast_mac[2] = 0x5e;

		multicast_mac[3] = ip[1] & 0x7f;
		multicast_mac[4] = ip[2] & 0xff;
		multicast_mac[5] = ip[3] & 0xff;

		udp_set_remote_har(sock, multicast_mac);
	}

	// set remote ip
	_dma_write_sock (sock, WIZ_Sn_DIPR, ip, 4);

	// set remote port
	_dma_write_sock_16 (sock, WIZ_Sn_DPORT, port);
}

void 
udp_set_remote_har (uint8_t sock, uint8_t *har)
{
	// remote hardware address, e.g. needed for UDP multicast
	_dma_write_sock (sock, WIZ_Sn_DHAR, har, 6);
}

uint_fast8_t
udp_send (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	uint_fast8_t success = 0;
	if (udp_send_nonblocking (sock, buf_ptr, len))
		success = udp_send_block (sock);
	return success;
}

__always_inline uint16_t
udp_available (uint8_t sock)
{
	uint16_t len;
	_dma_read_sock_16 (sock, WIZ_Sn_RX_RSR, &len);
	return len;
}

uint_fast8_t
udp_receive (uint8_t sock, uint_fast8_t buf_ptr, uint16_t len)
{
	uint16_t wrap = udp_receive_nonblocking (sock, buf_ptr, len);
	return udp_receive_block (sock, wrap, len);
}

__always_inline void
_wiz_advance_read_ptr (uint8_t sock, uint16_t len)
{
	uint16_t ptr = Sn_Rx_RD[sock];

	uint8_t *buf = buf_o[tmp_buf_o_ptr];

	ptr += len;
	Sn_Rx_RD[sock] = ptr;
	buf = _dma_write_sock_16_append (buf, sock, WIZ_Sn_RX_RD, ptr);

	uint8_t flag;
	flag = WIZ_Sn_CR_RECV;
	buf = _dma_write_sock_append (buf, sock, WIZ_Sn_CR, &flag, 1);

	_dma_read_nonblocking_in (buf);
	_dma_read_nonblocking_out ();
}

void 
udp_dispatch (uint8_t sock, uint_fast8_t ptr, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len))
{
	uint16_t len;

	while ( (len = udp_available (sock)) )
	{
		// read UDP header
		if (!udp_receive (sock, ptr, 8))
			return;

		uint8_t *buf_in_ptr = buf_i_i + WIZ_SEND_OFFSET;

		uint8_t ip[4];
		memcpy(ip, buf_in_ptr, 4);
		uint16_t port = (buf_in_ptr[4] << 8) | buf_in_ptr[5];
		uint16_t size = (buf_in_ptr[6] << 8) | buf_in_ptr[7];

		// read UDP payload
		if (!udp_receive (sock, ptr, size))
			return;

		cb (ip, port, buf_in_ptr, size);
	}
}

void
macraw_begin (uint8_t sock, uint_fast8_t mac_filter)
{
	uint8_t flag;

	// first close socket
	macraw_end (sock);

	// clear socket interrupt register
	_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);

	// set socket mode to MACRAW
	if (mac_filter) // only look for packages addressed to our MAC or broadcast
		flag = WIZ_Sn_MR_MACRAW | WIZ_Sn_MR_MF;
	else
		flag = WIZ_Sn_MR_MACRAW;
	_dma_write_sock (sock, WIZ_Sn_MR, &flag, 1);

	// open socket
	flag = WIZ_Sn_CR_OPEN;
	_dma_write_sock (sock, WIZ_Sn_CR, &flag, 1);
	do _dma_read_sock (sock, WIZ_Sn_SR, &flag, 1);
	while (flag != WIZ_Sn_SR_MACRAW)
		;

	// get write pointer
	_dma_read_sock_16 (sock, WIZ_Sn_TX_WR, &Sn_Tx_WR[sock]);

	// get read pointer
	_dma_read_sock_16 (sock, WIZ_Sn_RX_RD, &Sn_Rx_RD[sock]);
}

void
macraw_end (uint8_t sock)
{
	udp_end (sock);
}

uint_fast8_t
macraw_send (uint8_t sock, uint_fast8_t ptr, uint16_t len)
{
	return udp_send (sock, ptr, len);
}

__always_inline uint16_t
macraw_available (uint8_t sock)
{
	return udp_available (sock);
}

uint_fast8_t
macraw_receive (uint8_t sock, uint_fast8_t ptr, uint16_t len)
{
	return udp_receive (sock, ptr, len);
}

void
macraw_dispatch (uint8_t sock, uint_fast8_t ptr, Wiz_MACRAW_Dispatch_Cb cb, void *user_data)
{
	uint16_t len;

	while ( (len = macraw_available (sock)) )
	{
		// receive packet MACRAW size
		if (!macraw_receive (sock, ptr, 2))
			return;
			
		uint8_t *buf_in_ptr = buf_i_i + WIZ_SEND_OFFSET;
		uint16_t size = (buf_in_ptr[0] << 8) | buf_in_ptr[1];

		// receive payload
		if (!macraw_receive (sock, ptr, size-2))
			return;
			
		cb (buf_in_ptr, size-2, user_data);
	}
}

void
wiz_irq_handle (void)
{
	uint8_t ir;
	uint8_t ir2;
	uint_fast8_t sock;

	// main interrupt
	if(irq_cb)
	{
		_dma_read(WIZ_IR, &ir, 1);
		irq_cb(ir);
		_dma_write(WIZ_IR, &ir, 1); // clear main IRQ flags
	}

	// socket interrupts
	_dma_read(WIZ_SIR, &ir2, 1);
	for(sock=0; sock<WIZ_MAX_SOCK_NUM; sock++)
	{
		if( (1U << sock) & ir2) // there was an IRQ for this socket
			if(irq_socket_cb[sock])
			{
				uint8_t sock_ir;
				_dma_read_sock(sock, WIZ_Sn_IR, &sock_ir, 1); // get socket IRQ
				irq_socket_cb[sock](sock_ir);
				_dma_write_sock(sock, WIZ_Sn_IR, &sock_ir, 1); // clear socket IRQ flags (this automatically clears WIZ_SIR[sock]
			}
	}
}

void
wiz_irq_set(Wiz_IRQ_Cb cb, uint8_t mask)
{
	irq_cb = cb;

	_dma_write(WIZ_IMR, &mask , 1); // set mask
}

void
wiz_irq_unset()
{
	irq_cb = NULL;

	uint8_t mask = 0;
	_dma_write(WIZ_IMR, &mask , 1); // clear mask
}

void
wiz_socket_irq_set(uint8_t socket, Wiz_IRQ_Cb cb, uint8_t mask)
{
	irq_socket_cb[socket] = cb;

	uint8_t mask2;
	_dma_read(WIZ_SIMR, &mask2, 1);
	mask2 |= (1U << socket); // enable socket IRQs
	_dma_write(WIZ_SIMR, &mask2, 1);

	_dma_write_sock(socket, WIZ_Sn_IMR, &mask, 1); // set mask
}

void
wiz_socket_irq_unset(uint8_t socket)
{
	irq_socket_cb[socket] = NULL;

	uint8_t mask2;
	_dma_read(WIZ_SIMR, &mask2, 1);
	mask2 &= ~(1U << socket); // disable socket IRQs
	_dma_write(WIZ_SIMR, &mask2, 1);

	uint8_t mask = 0;
	_dma_write_sock(socket, WIZ_Sn_IMR, &mask, 1); // clear mask
}
