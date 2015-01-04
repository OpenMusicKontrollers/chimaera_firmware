/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _W5200_PRIVATE_H_
#define _W5200_PRIVATE_H_

#include "wiz_private.h"

#define WIZ_MR		 									0x0000
#define WIZ_GAR		 									0x0001
#define WIZ_SUBR	 									0x0005
#define WIZ_SHAR	 									0x0009
#define WIZ_SIPR 	 									0x000F
#define WIZ_IR		 									0x0015
#define WIZ_SIMR	 									0x0016
#define WIZ_RTR		 									0x0017
#define WIZ_RCR		 									0x0019
#define WIZ_PATR	 									0x001C
#define WIZ_PTIMER 									0x0028
#define WIZ_PMAGIC 									0x0029
#define WIZ_INTLEVEL 								0x0030
#define WIZ_SIR    									0x0034
#define WIZ_PHYCFGR									0x0035
#define WIZ_IMR		 									0x0036

#define WIZ_MR_RST									(1U << 7) // reset
#define WIZ_MR_PB										(1U << 4) // ping block
#define WIZ_MR_PPPoE								(1U << 3) // PPPoE mode

#define WIZ_IR_CONFLICT							(1U << 7) // ARP conflict
#define WIZ_IR_PPPoE								(1U << 5) // PPPoE connection close

#define WIZ_PHYCFGR_LNK  						(1U << 5)
#define WIZ_PHYCFGR_PWDN 						(1U << 3)

#define WIZ_Sn_MR     							0x0000 // 1 Mode
#define WIZ_Sn_CR     							0x0001 // 1 Command
#define WIZ_Sn_IR     							0x0002 // 1 Interrupt
#define WIZ_Sn_SR     							0x0003 // 1 Status
#define WIZ_Sn_PORT   							0x0004 // 2 Source Port
#define WIZ_Sn_DHAR   							0x0006 // 6 Destination Hardw Addr
#define WIZ_Sn_DIPR   							0x000C // 4 Destination IP Addr
#define WIZ_Sn_DPORT  							0x0010 // 2 Destination Port
#define WIZ_Sn_MSSR   							0x0012 // 2 Max Segment Size
#define WIZ_Sn_PROTO  							0x0014 // 1 Protocol in IP RAW Mode
#define WIZ_Sn_TOS    							0x0015 // 1 IP TOS
#define WIZ_Sn_TTL    							0x0016 // 1 IP TTL
#define WIZ_Sn_RX_MS  							0x001E // 1 RX Memory size
#define WIZ_Sn_TX_MS  							0x001F // 1 TX Memory size
#define WIZ_Sn_TX_FSR 							0x0020 // 2 TX Free Size
#define WIZ_Sn_TX_RD  							0x0022 // 2 TX Read Pointer
#define WIZ_Sn_TX_WR  							0x0024 // 2 TX Write Pointer
#define WIZ_Sn_RX_RSR 							0x0026 // 2 RX Received Size
#define WIZ_Sn_RX_RD  							0x0028 // 2 RX Read Pointer
#define WIZ_Sn_RX_WR  							0x002A // 2 RX Write Pointer(supported?)
#define WIZ_Sn_IMR    							0x002C // 1 Interrupt mask
#define WIZ_Sn_FRAG   							0x003D // 2 Fragemnt offset

#define WIZ_Sn_MR_CLOSE  						0x00
#define WIZ_Sn_MR_TCP    						0x01
#define WIZ_Sn_MR_UDP    						0x02
#define WIZ_Sn_MR_IPRAW  						0x03
#define WIZ_Sn_MR_MACRAW 						0x04 // valid only on socket 0
#define WIZ_Sn_MR_PPPOE  						0x05 // valid only on socket 0
#define WIZ_Sn_MR_ND								(1U << 5)
#define WIZ_Sn_MR_MC								(1U << 5)
#define WIZ_Sn_MR_MF								(1U << 6)
#define WIZ_Sn_MR_MULTI  						(1U << 7)

#define WIZ_Sn_CR_OPEN      				0x01
#define WIZ_Sn_CR_LISTEN    				0x02
#define WIZ_Sn_CR_CONNECT   				0x04
#define WIZ_Sn_CR_DISCON    				0x08
#define WIZ_Sn_CR_CLOSE     				0x10
#define WIZ_Sn_CR_SEND      				0x20
#define WIZ_Sn_CR_SEND_MAC  				0x21
#define WIZ_Sn_CR_SEND_KEEP 				0x22
#define WIZ_Sn_CR_RECV      				0x40

//#define WIZ_Sn_IR_PRECV						(1U << 7) // valid only on socket 0 & PPPoE
//#define WIZ_Sn_IR_PFAIL						(1U << 6) // valid only on socket 0 & PPPoE
//#define WIZ_Sn_IR_PNEXT						(1U << 5) // valid only on socket 0 & PPPoE
//#define WIZ_Sn_IR_SEND_OK					(1U << 4)
//#define WIZ_Sn_IR_TIMEOUT					(1U << 3)
//#define WIZ_Sn_IR_RECV						(1U << 2)
//#define WIZ_Sn_IR_DISCON					(1U << 1)
//#define WIZ_Sn_IR_CON							(1U << 0)

#define WIZ_Sn_SR_CLOSED 						0x00
#define WIZ_Sn_SR_SOCK_ARP					0x01
#define WIZ_Sn_SR_INIT   						0x13
#define WIZ_Sn_SR_LISTEN						0x14
#define WIZ_Sn_SR_SYNSENT						0x15
#define WIZ_Sn_SR_SCNRECV						0x16
#define WIZ_Sn_SR_ESTABLISHED				0x17
#define WIZ_Sn_SR_FIN_WAIT					0x18
#define WIZ_Sn_SR_CLOSING						0x1A
#define WIZ_Sn_SR_TIME_WAIT					0x1B
#define WIZ_Sn_SR_CLOSE_WAIT				0x1C
#define WIZ_Sn_SR_LAST_ACK					0x1D
#define WIZ_Sn_SR_UDP    						0x22
#define WIZ_Sn_SR_IPRAW    					0x32
#define WIZ_Sn_SR_MACRAW 						0x42
#define WIZ_Sn_SR_PPPoE	 						0x5F

#define CH_BASE 							0x4000
#define	CH_SIZE 							0x0100

#define TX_BUF_BASE 					0x8000
#define RX_BUF_BASE 					0xC000

#endif // _W5200_PRIVATE_H_
