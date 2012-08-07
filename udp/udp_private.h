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

#ifndef UDP_PRIVATE_H
#define UDP_PRIVATE_H

#include <udp.h>

#define MR		 0x0000
#define GAR		 0x0001
#define SUBR	 0x0005
#define SHAR	 0x0009
#define SIPR 	 0x000F
#define IR		 0x0015
#define IMR		 0x0016
#define RTR		 0x0017
#define RCR		 0x0019
#define PATR	 0x001C
#define PTIMER 0x0028
#define PMAGIC 0x0029

#define SnMR_CLOSE  0x00
#define SnMR_TCP    0x01
#define SnMR_UDP    0x02
#define SnMR_IPRAW  0x03
#define SnMR_MACRAW 0x04
#define SnMR_PPPOE  0x05
#define SnMR_ND     0x20
#define SnMR_MULTI  0x80

#define SnCR_OPEN      0x01
#define SnCR_LISTEN    0x02
#define SnCR_CONNECT   0x04
#define SnCR_DISCON    0x08
#define SnCR_CLOSE     0x10
#define SnCR_SEND      0x20
#define SnCR_SEND_MAC  0x21
#define SnCR_SEND_KEEP 0x22
#define SnCR_RECV      0x40

#define SnSR_CLOSED 0x00
#define SnSR_INIT   0x13
#define SnSR_UDP    0x22

#define SnMR     0x0000 // 1 Mode
#define SnCR     0x0001 // 1 Command
#define SnIR     0x0002 // 1 Interrupt
#define SnSR     0x0003 // 1 Status
#define SnPORT   0x0004 // 2 Source Port
#define SnDHAR   0x0006 // 6 Destination Hardw Addr
#define SnDIPR   0x000C // 4 Destination IP Addr
#define SnDPORT  0x0010 // 2 Destination Port
#define SnMSSR   0x0012 // 2 Max Segment Size
#define SnPROTO  0x0014 // 1 Protocol in IP RAW Mode
#define SnTOS    0x0015 // 1 IP TOS
#define SnTTL    0x0016 // 1 IP TTL
#define SnTX_FSR 0x0020 // 2 TX Free Size
#define SnTX_RD  0x0022 // 2 TX Read Pointer
#define SnTX_WR  0x0024 // 2 TX Write Pointer
#define SnRX_RSR 0x0026 // 2 RX Free Size
#define SnRX_RD  0x0028 // 2 RX Read Pointer
#define SnRX_WR  0x002A // 2 RX Write Pointer (supported?)
#define SnRX_MS  0x001E // 1 RX Memory size
#define SnTX_MS  0x001F // 1 TX Memory size

#define SnIR_SEND_OK 0x10
#define SnIR_TIMEOUT 0x08
#define SnIR_RECV    0x04
#define SnIR_DISCON  0x02
#define SnIR_CON     0x01

#define CH_BASE 0x4000
#define	CH_SIZE 0x0100

#define TX_BUF_BASE 0x8000
#define RX_BUF_BASE 0xC000

#endif // UDP_PRIVATE_H
