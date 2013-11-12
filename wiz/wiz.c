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

#if WIZ_CHIP == W5200
#	include "W5200_private.h"
#elif WIZ_CHIP == W5500
#	include "W5500_private.h"
#endif

#include <tube.h>
#include <netdef.h>
#include <chimaera.h>
#include <chimutil.h>
#include <config.h>

#include <libmaple/dma.h>
#include <libmaple/spi.h>

Wiz_Job wiz_jobs [WIZ_MAX_JOB_NUM];
volatile uint_fast8_t wiz_jobs_todo = 0;
volatile uint_fast8_t wiz_jobs_done;

const uint8_t wiz_nil_ip [] = {0x00, 0x00, 0x00, 0x00};
const uint8_t wiz_nil_mac [] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t wiz_broadcast_mac [] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

gpio_dev *ss_dev;
uint8_t ss_bit;

Wiz_IRQ_Cb irq_cb = NULL;
Wiz_IRQ_Cb irq_socket_cb [WIZ_MAX_SOCK_NUM];

uint16_t SSIZE [WIZ_MAX_SOCK_NUM];
uint16_t RSIZE [WIZ_MAX_SOCK_NUM];

uint16_t Sn_Tx_WR[WIZ_MAX_SOCK_NUM];
uint16_t Sn_Rx_RD[WIZ_MAX_SOCK_NUM];

uint8_t buf_o2 [WIZ_SEND_OFFSET + 6] __attribute__((aligned(4)));
uint8_t buf_i2 [WIZ_SEND_OFFSET + 6] __attribute__((aligned(4)));

__always_inline void
_dma_write (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len)
{
	uint8_t *tmp_buf_o = buf_o2 + WIZ_SEND_OFFSET;
	
	memcpy (tmp_buf_o, dat, len);

	wiz_job_add(addr, len, tmp_buf_o, NULL, cntrl, WIZ_TX);

	wiz_job_run_nonblocking();
	wiz_job_run_block();
}

__always_inline void
_dma_read (uint16_t addr, uint8_t cntrl, uint8_t *dat, uint16_t len)
{
	uint8_t *tmp_buf_o = buf_o2 + WIZ_SEND_OFFSET;
	uint8_t *tmp_buf_i = buf_i2 + WIZ_SEND_OFFSET;
	
	// more efficient than WIZ_RX for such short reads
	wiz_job_add(addr, len, tmp_buf_o, tmp_buf_i, cntrl, WIZ_TXRX);

	wiz_job_run_nonblocking();
	wiz_job_run_block();

	memcpy (dat, tmp_buf_i, len);
}

__always_inline void
_dma_write_sock_16 (uint8_t sock, uint16_t addr, uint16_t dat)
{
	uint16_t _dat = hton (dat);
	_dma_write_sock (sock, addr, (uint8_t *)&_dat, 2);
}

__always_inline void
_dma_read_sock_16 (int8_t sock, uint16_t addr, uint16_t *dat)
{
	_dma_read_sock (sock, addr, (uint8_t*)dat, 2);
	*dat = hton (*dat);
}

static void
_wiz_rx_irq()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t isr_rx = dma_get_isr_bits(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB);
	uint8_t isr_tx = dma_get_isr_bits(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);
	uint8_t spi_sr = WIZ_SPI_BAS->SR;
	uint_fast8_t rx_err = 0;

	// check for errors in DMA and SPI status registers
	rx_err |= isr_rx & DMA_ISR_TEIF; // RX DMA transfer error
	rx_err |= isr_tx & DMA_ISR_TEIF; // TX DMA transfer error
	rx_err |= spi_sr & SPI_SR_OVR; // SPI overrun error
	rx_err |= spi_sr & SPI_SR_MODF; // SPI mode fault

	dma_clear_isr_bits(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);
	dma_clear_isr_bits(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB);

	dma_disable(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB); // Tx
	dma_disable(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB); // Rx

	if( !rx_err && (isr_rx & DMA_ISR_TCIF) ) // no error and Rx DMA transfer complete
	{
		if( (job->rw & WIZ_TX) || job->rx_hdr_sent)
		{
			wiz_jobs_done++;
			resetSS();
		}
		else
			job->rx_hdr_sent = 1;
	}

	if(wiz_jobs_done < wiz_jobs_todo)
		wiz_job_run_single();
	else
		wiz_jobs_todo = 0;
}

static void
_wiz_tx_irq()
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];
	uint8_t isr_tx = dma_get_isr_bits(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);
	uint8_t spi_sr = WIZ_SPI_BAS->SR;
	uint_fast8_t tx_err = 0;

	// check for errors in DMA and SPI status registers
	tx_err |= isr_tx & DMA_ISR_TEIF; // Tx DMA transfer error
	tx_err |= spi_sr & SPI_SR_MODF; // SPI mode fault

	dma_clear_isr_bits(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);
	dma_disable(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB); // Tx

	if( !tx_err && (isr_tx & DMA_ISR_TCIF) ) // no error and Tx DMA transfer complete
	{
		do {
			spi_sr = WIZ_SPI_BAS->SR;
		} while (spi_sr & SPI_SR_FTLVL); // TX FIFO not empty

		do {
			spi_sr = WIZ_SPI_BAS->SR;
		} while (spi_sr & SPI_SR_BSY); // wait for BSY==0

		do {
			//spi_sr = WIZ_SPI_BAS->DR;
			spi_sr = spi_rx_reg(WIZ_SPI_DEV);
			spi_sr = WIZ_SPI_BAS->SR;
		} while ( (spi_sr & SPI_SR_FRLVL) || (spi_sr & SPI_SR_RXNE) || (spi_sr & SPI_SR_OVR) ); // empty buffer and clear OVR flag

		wiz_jobs_done++;
	}

	resetSS();

	if(wiz_jobs_done < wiz_jobs_todo)
		wiz_job_run_single();
	else
		wiz_jobs_todo = 0;
}

void
wiz_job_add(uint16_t addr, uint16_t len, uint8_t *tx, uint8_t *rx, uint8_t opmode, uint8_t rw)
{
	Wiz_Job *job = &wiz_jobs[wiz_jobs_todo];
	job->addr = addr;
	job->len = len;
	job->tx = tx;
	job->rx = rx;
	job->opmode = opmode;
	job->rw = rw;
	job->rx_hdr_sent = 0;

	wiz_jobs_todo++;
}

__always_inline void
wiz_job_run_single()
{
	uint8_t *frm_rx = NULL;
	uint8_t *frm_tx = NULL;
	Wiz_Job *job = &wiz_jobs[wiz_jobs_done];

	if(!job->rx_hdr_sent)
		wiz_job_set_frame(); //TODO move into switch statement?

	uint16_t len2 = job->len + WIZ_SEND_OFFSET;

	switch(job->rw)
	{
		case WIZ_TX:

			dma_channel_regs(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB)->CCR |= DMA_CCR_TCIE | DMA_CCR_TEIE;
			dma_attach_interrupt(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, _wiz_tx_irq);

			dma_detach_interrupt(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB);
			
			spi_rx_dma_disable(WIZ_SPI_DEV); // disable RX DMA on WIZ_SPI_DEV

			setSS();

			frm_tx = job->tx - WIZ_SEND_OFFSET;
			spi_tx_dma_enable(WIZ_SPI_DEV); // enable TX DMA on WIZ_SPI_DEV
			spi_tx_tube.tube_dst = frm_tx;
			dma_set_mem_addr(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, spi_tx_tube.tube_dst);
			dma_set_num_transfers(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, len2); // Tx
			dma_enable(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB); // Tx

			break;

		case WIZ_RX:
			dma_channel_regs(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB)->CCR |= DMA_CCR_TCIE | DMA_CCR_TEIE;
			dma_attach_interrupt(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, _wiz_rx_irq);
			
			dma_detach_interrupt(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);

			if(!job->rx_hdr_sent)
			{
				setSS();
				spi_tx_dma_enable(WIZ_SPI_DEV); // enable TX DMA on WIZ_SPI_DEV

				len2 = WIZ_SEND_OFFSET; // only send header
				frm_rx = job->rx - WIZ_SEND_OFFSET;
			}
			else // job->rx_hdr_sent
			{
				// SS is already set
				spi_tx_dma_disable(WIZ_SPI_DEV); // disable TX DMA on WIZ_SPI_DEV

				len2 -= WIZ_SEND_OFFSET;
				frm_rx = job->rx;
			}

			spi_rx_dma_enable(WIZ_SPI_DEV); // enable RX DMA on WIZ_SPI_DEV
			spi_rx_tube.tube_dst = frm_rx;
			dma_set_mem_addr(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, spi_rx_tube.tube_dst);
			dma_set_num_transfers(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, len2); // Rx
			dma_enable(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB); // Rx

			if(!job->rx_hdr_sent) //TODO merge with upper conditional
			{
				frm_tx = job->tx - WIZ_SEND_OFFSET;
				spi_tx_tube.tube_dst = frm_tx;
				dma_set_mem_addr(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, spi_tx_tube.tube_dst);
				dma_set_num_transfers(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, len2); // Tx
				dma_enable(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB); // Tx
			}

			break;

		case WIZ_TXRX:
			dma_channel_regs(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB)->CCR |= DMA_CCR_TCIE | DMA_CCR_TEIE;
			dma_attach_interrupt(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, _wiz_rx_irq);
			
			dma_detach_interrupt(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB);

			setSS();

			frm_rx = job->rx - WIZ_SEND_OFFSET;
			spi_rx_dma_enable(WIZ_SPI_DEV); // enable RX DMA on WIZ_SPI_DEV
			spi_rx_tube.tube_dst = frm_rx;
			dma_set_mem_addr(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, spi_rx_tube.tube_dst);
			dma_set_num_transfers(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, len2); // Rx
			dma_enable(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB); // Rx

			frm_tx = job->tx - WIZ_SEND_OFFSET;
			spi_tx_dma_enable(WIZ_SPI_DEV); // enable TX DMA on WIZ_SPI_DEV
			spi_tx_tube.tube_dst = frm_tx;
			dma_set_mem_addr(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, spi_tx_tube.tube_dst);
			dma_set_num_transfers(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, len2); // Tx
			dma_enable(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB); // Tx

			break;
	}
}

__always_inline void
wiz_job_run_nonblocking()
{
	wiz_jobs_done = 0;
	if(wiz_jobs_done < wiz_jobs_todo)
		wiz_job_run_single();
}

__always_inline void
wiz_job_run_block()
{
	while(wiz_jobs_todo)
		; // wait until all jobs are done
}

void
wiz_init (gpio_dev *dev, uint8_t bit, uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM])
{
	int status;

	ss_dev = dev;
	ss_bit = bit;

	// set up dma for SPI RX
	spi_rx_tube.tube_dst = BUF_I_BASE(buf_i_ptr);
	status = dma_tube_cfg (WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, &spi_rx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, DMA_PRIORITY_HIGH);
	//nvic_irq_set_priority (NVIC_DMA_CH4, SPI_RX_DMA_PRIORITY);

	// set up dma for SPI TX
	spi_tx_tube.tube_dst = BUF_O_BASE(buf_o_ptr);
	status = dma_tube_cfg (WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, &spi_tx_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, DMA_PRIORITY_HIGH);
	//nvic_irq_set_priority (NVIC_DMA_CH5, SPI_TX_DMA_PRIORITY);

	// attach interrupt
	//dma_attach_interrupt(WIZ_SPI_TX_DMA_DEV, WIZ_SPI_TX_DMA_TUB, _wiz_tx_irq);
	//dma_attach_interrupt(WIZ_SPI_RX_DMA_DEV, WIZ_SPI_RX_DMA_TUB, _wiz_rx_irq);

	// init udp
	uint8_t flag;

	flag = WIZ_MR_RST;
	_dma_write (WIZ_MR, 0, &flag, 1);
	do {
		_dma_read (WIZ_MR, 0, &flag, 1);
	} while (flag & WIZ_MR_RST);

	wiz_sockets_set(tx_mem, rx_mem);
}

uint_fast8_t
wiz_link_up ()
{
	uint8_t physr;
	_dma_read (WIZ_PHYCFGR, 0, &physr, 1);
	return ( (physr & WIZ_PHYCFGR_LNK) == WIZ_PHYCFGR_LNK);
}

void
wiz_mac_set (uint8_t *mac)
{
	_dma_write (WIZ_SHAR, 0, mac, 6);
}

void
wiz_ip_set (uint8_t *ip)
{
	_dma_write (WIZ_SIPR, 0, ip, 4);
}

void
wiz_gateway_set (uint8_t *gateway)
{
	_dma_write (WIZ_GAR, 0, gateway, 4);
}

void
wiz_subnet_set (uint8_t *subnet)
{
	_dma_write (WIZ_SUBR, 0, subnet, 4);
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

void
udp_receive_block (uint8_t sock)
{
	wiz_job_run_block();
}

void
udp_send_block (uint8_t sock)
{
	wiz_job_run_block();

	uint8_t ir;
	uint8_t flag;
	do
	{
		_dma_read_sock (sock, WIZ_Sn_IR, &ir, 1);
		if (ir & WIZ_Sn_IR_TIMEOUT) // ARPto occured, SEND failed
		{
			flag = WIZ_Sn_IR_SEND_OK | WIZ_Sn_IR_TIMEOUT; // set SEND_OK flag and clear TIMEOUT flag
			_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);
		}
	} while ( (ir & WIZ_Sn_IR_SEND_OK) != WIZ_Sn_IR_SEND_OK);

	flag = WIZ_Sn_IR_SEND_OK; // clear SEND_OK flag
	_dma_write_sock (sock, WIZ_Sn_IR, &flag, 1);
}

void
udp_send (uint8_t sock, uint8_t *o_buf, uint16_t len)
{
	if(udp_send_nonblocking(sock, o_buf, len))
		udp_send_block(sock);
}

__always_inline uint16_t
udp_available (uint8_t sock)
{
	uint16_t len;
	_dma_read_sock_16 (sock, WIZ_Sn_RX_RSR, &len);
	return len;
}

void
udp_receive (uint8_t sock, uint8_t *i_buf, uint16_t len)
{
	if(udp_receive_nonblocking(sock, i_buf, len))
		udp_receive_block(sock);
}

void 
udp_dispatch (uint8_t sock, uint8_t *i_buf, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len))
{
	uint16_t len;

	while ( (len = udp_available (sock)) )
	{
		// read UDP header
		udp_receive (sock, i_buf, 8);

		uint8_t *tmp_buf_i_ptr = i_buf + WIZ_SEND_OFFSET;

		uint8_t ip[4];
		memcpy(ip, tmp_buf_i_ptr, 4);
		uint16_t port = (tmp_buf_i_ptr[4] << 8) | tmp_buf_i_ptr[5];
		uint16_t size = (tmp_buf_i_ptr[6] << 8) | tmp_buf_i_ptr[7];

		// read UDP payload
		udp_receive (sock, i_buf, size);

		cb (ip, port, tmp_buf_i_ptr, size);
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
macraw_dispatch (uint8_t sock, uint8_t *i_buf, Wiz_MACRAW_Dispatch_Cb cb, void *user_data)
{
	uint16_t len;

	while ( (len = macraw_available (sock)) )
	{
		// receive packet MACRAW size
		macraw_receive (sock, i_buf, 2);
			
		uint8_t *tmp_buf_i_ptr = i_buf + WIZ_SEND_OFFSET;
		uint16_t size = (tmp_buf_i_ptr[0] << 8) | tmp_buf_i_ptr[1];

		// receive payload
		macraw_receive (sock, i_buf, size-2);
			
		cb (tmp_buf_i_ptr, size-2, user_data);
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
		_dma_read(WIZ_IR, 0, &ir, 1);
		irq_cb(ir);
		_dma_write(WIZ_IR, 0, &ir, 1); // clear main IRQ flags
	}

	// socket interrupts
	_dma_read(WIZ_SIR, 0, &ir2, 1);
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

	_dma_write(WIZ_IMR, 0, &mask , 1); // set mask
}

void
wiz_irq_unset()
{
	irq_cb = NULL;

	uint8_t mask = 0;
	_dma_write(WIZ_IMR, 0, &mask , 1); // clear mask
}

void
wiz_socket_irq_set(uint8_t socket, Wiz_IRQ_Cb cb, uint8_t mask)
{
	irq_socket_cb[socket] = cb;

	uint8_t mask2;
	_dma_read(WIZ_SIMR, 0, &mask2, 1);
	mask2 |= (1U << socket); // enable socket IRQs
	_dma_write(WIZ_SIMR, 0, &mask2, 1);

	_dma_write_sock(socket, WIZ_Sn_IMR, &mask, 1); // set mask
}

void
wiz_socket_irq_unset(uint8_t socket)
{
	irq_socket_cb[socket] = NULL;

	uint8_t mask2;
	_dma_read(WIZ_SIMR, 0, &mask2, 1);
	mask2 &= ~(1U << socket); // disable socket IRQs
	_dma_write(WIZ_SIMR, 0, &mask2, 1);

	uint8_t mask = 0;
	_dma_write_sock(socket, WIZ_Sn_IMR, &mask, 1); // clear mask
}
