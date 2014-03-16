/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#ifndef _W5500_PRIVATE_H_
#define _W5500_PRIVATE_H_

#include "wiz_private.h"

#define W5500_CNTRL_PHASE_BLOCK_SEL_SHIFT					3
#define W5500_CNTRL_PHASE_READ_WRITE_BIT					2

#define W5500_CNTRL_PHASE_BLOCK_SEL_COMMON_REG		(0 << W5500_CNTRL_PHASE_BLOCK_SEL_SHIFT)

#define W5500_CNTRL_PHASE_READ										(0 << W5500_CNTRL_PHASE_READ_WRITE_BIT)
#define W5500_CNTRL_PHASE_WRITE										(1 << W5500_CNTRL_PHASE_READ_WRITE_BIT)

#define W5500_CNTRL_PHASE_OP_MODE_N_BYTE					0
#define W5500_CNTRL_PHASE_OP_MODE_1_BYTE					1
#define W5500_CNTRL_PHASE_OP_MODE_2_BYTE					2
#define W5500_CNTRL_PHASE_OP_MODE_4_BYTE					3

typedef struct _W5500_Socket_Sel W5500_Socket_Sel;

struct _W5500_Socket_Sel {
	uint8_t reg;
	uint8_t tx_buf;
	uint8_t rx_buf;
};

#define WIZ_MR								0x0000	// mode
#define WIZ_GAR								0x0001	// gateway address
#define WIZ_SUBR							0x0005	// subnet mask address
#define WIZ_SHAR							0x0009	// source hardware address
#define WIZ_SIPR							0x000F	// source IP address
#define WIZ_INTLEVEL					0x0013	// interrupt low level timer
#define WIZ_IR								0x0015	// interrupt register
#define WIZ_IMR								0x0016	// interrupt mask register
#define WIZ_SIR								0x0017	// socket interrupt register
#define WIZ_SIMR							0x0018	// socket interrupt mask register
#define WIZ_RTR								0x0019	// retry time
#define WIZ_RCR								0x001B	// retry count
#define WIZ_PTIMER						0x001C	// PPP LCP request time
#define WIZ_PMAGIC						0x001D	// PPP LCP magic number
#define WIZ_PHAR							0x001E	// PPP destination MAC address
#define WIZ_PSID							0x0024	// PPP session identification
#define WIZ_PMRU							0x0026	// PPP maximum segment size
#define WIZ_UIPR							0x0028	// unreachable IP address
#define WIZ_UPORTR						0x002C	// unreachable port
#define WIZ_PHYCFGR						0x002E	// PHY configuration
#define WIZ_VERSIONR 					0x0039	// chip version

#define WIZ_MR_RST						(1U << 7) // reset
#define WIZ_MR_WOL						(1U << 5) // enable/disable wake on LAN
#define WIZ_MR_PB							(1U << 4)	// enable/disable ping block mode
#define WIZ_MR_PPPOE					(1U << 3)	// enable/disable PPPoE mode
#define WIZ_MR_FARP						(1U << 1)	// enable/disable force ARP

#define WIZ_IR_CONFLICT				(1U << 7)	// IP conflict
#define WIZ_IR_UNREACH				(1U << 6)	// destination unreachable
#define WIZ_IR_PPPOE					(1U << 5)	// PPPoE connection close
#define WIZ_IR_MP							(1U << 4)	// magic packet(wake on LAN)

#define WIZ_PHYCFGR_RST				(1U << 7)	// PHY reset
#define WIZ_PHYCFGR_OPMD_HW		(0U << 6)	// configure PHY operation mode with hardware pins
#define WIZ_PHYCFGR_OPMD_SW		(1U << 6)	// configure PHY operation mode with software
#define WIZ_PHYCFGR_OPMDC_SHIFT	3
#define WIZ_PHYCFGR_OPMDC			(0x7 << WIZ_PHYCFGR_OPMDC_SHIFT)
#define WIZ_PHYCFGR_DPX				(1U << 2)	// duplex status
#define WIZ_PHYCFGR_SPD				(1U << 1)	// speed status
#define WIZ_PHYCFGR_LNK				(1U << 0)	// link status

#define WIZ_Sn_MR							0x0000	// mode
#define WIZ_Sn_CR							0x0001	// command register
#define WIZ_Sn_IR							0x0002	// interrupt register
#define WIZ_Sn_SR							0x0003	// status register
#define WIZ_Sn_PORT						0x0004	// source port
#define WIZ_Sn_DHAR						0x0006	// destination hardware address
#define WIZ_Sn_DIPR						0x000C	// destination IP address
#define WIZ_Sn_DPORT					0x0010	// destination port
#define WIZ_Sn_MSSR						0x0012	// maximum segment size
#define WIZ_Sn_TOS						0x0015	// TOS
#define WIZ_Sn_TTL						0x0016	// TTL
#define WIZ_Sn_RXBUF_SIZE			0x001E	// receive buffer size
#define WIZ_Sn_TXBUF_SIZE			0x001F	// transmit buffer size
#define WIZ_Sn_TX_FSR					0x0020	// TX free size
#define WIZ_Sn_TX_RD					0x0022	// TX read pointer
#define WIZ_Sn_TX_WR					0x0024	// TX write pointer
#define WIZ_Sn_RX_RSR					0x0026	// RX received size
#define WIZ_Sn_RX_RD					0x0028	// RX read pointer
#define WIZ_Sn_RX_WR					0x002A	// RX write pointer
#define WIZ_Sn_IMR						0x002C	// interrupt mask register
#define WIZ_Sn_FRAG						0x002D	// fragment offset in IP header
#define WIZ_Sn_KPALVTR				0x002F	// keep alive timer

#define WIZ_Sn_MR_MULTI				(1U << 7)	// enable/disable multicast in UDP mode
#define WIZ_Sn_MR_MF					(1U << 7)	// enalbe/disable MAC filter in MACRAW mode 
#define WIZ_Sn_MR_BCASTB			(1U << 6)	// enalbe/disable broadcast blocking in UDP/MACRAW modes
#define WIZ_Sn_MR_ND					(1U << 5)
#define WIZ_Sn_MR_MC					(1U << 5)
#define WIZ_Sn_MR_MMB					(1U << 5)
#define WIZ_Sn_MR_UCASTB			(1U << 4)
#define WIZ_Sn_MR_MIP6B				(1U << 4)
#define WIZ_Sn_MR_CLOSE				0x00
#define WIZ_Sn_MR_TCP					0x01
#define WIZ_Sn_MR_UDP					0x02
#define WIZ_Sn_MR_MACRAW			0x04

#define WIZ_Sn_CR_OPEN  			0x01
#define WIZ_Sn_CR_LISTEN 			0x02
#define WIZ_Sn_CR_CONNECT			0x04
#define WIZ_Sn_CR_DISCON 			0x08
#define WIZ_Sn_CR_CLOSE  			0x10
#define WIZ_Sn_CR_SEND   			0x20
#define WIZ_Sn_CR_SEND_MAC 		0x21
#define WIZ_Sn_CR_SEND_KEEP 	0x22
#define WIZ_Sn_CR_RECV    		0x40

//#define WIZ_Sn_IR_SEND_OK			(1U << 4)
//#define WIZ_Sn_IR_TIMEOUT			(1U << 3)
//#define WIZ_Sn_IR_RECV				(1U << 2)
//#define WIZ_Sn_IR_DISCON			(1U << 1)
//#define WIZ_Sn_IR_CON					(1U << 0)

#define WIZ_Sn_SR_CLOSED 			0x00
#define WIZ_Sn_SR_INIT   			0x13
#define WIZ_Sn_SR_LISTEN			0x14
#define WIZ_Sn_SR_ESTABLISHED	0x17
#define WIZ_Sn_SR_CLOSE_WAIT	0x1C
#define WIZ_Sn_SR_UDP    			0x22
#define WIZ_Sn_SR_MACRAW 			0x42

#define WIZ_Sn_SR_SYNSENT			0x15
#define WIZ_Sn_SR_SYNRECV			0x16
#define WIZ_Sn_SR_FIN_WAIT		0x18
#define WIZ_Sn_SR_CLOSING			0x1A
#define WIZ_Sn_SR_TIME_WAIT		0x1B
#define WIZ_Sn_SR_LAST_ACK		0x1D

#endif // _W5500_PRIVATE_H_
