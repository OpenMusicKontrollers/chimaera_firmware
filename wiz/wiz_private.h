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

#ifndef WIZ_PRIVATE_H
#define WIZ_PRIVATE_H

#include <wiz.h>

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
#define IR2    0x0034
#define PHYSR  0x0035

#define PHYSR_LINK  (1U << 5)
#define PHYSR_PWDN  (1U << 3)

#define SnMR_CLOSE  0x00
#define SnMR_TCP    0x01
#define SnMR_UDP    0x02
#define SnMR_IPRAW  0x03
#define SnMR_MACRAW 0x04
#define SnMR_PPPOE  0x05
#define SnMR_ND_MC	(1U << 5)
#define SnMR_MF			(1U << 6)
#define SnMR_MULTI  (1U << 7)

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
#define SnSR_MACRAW 0x42

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

typedef struct _ARP_Payload ARP_Payload;
typedef struct _MACRAW_Header MACRAW_Header;

struct _ARP_Payload {
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t oper;
	uint8_t sha [6];
	uint8_t spa [4];
	uint8_t tha [6];
	uint8_t tpa [4];
} __attribute((packed,aligned(2)));

struct _MACRAW_Header {
	uint8_t dst_mac [6];
	uint8_t src_mac [6];
	uint16_t type;
} __attribute((packed,aligned(2)));

#define MACRAW_HEADER_SIZE sizeof(MACRAW_Header)
#define ARP_PAYLOAD_SIZE sizeof(ARP_Payload)

#define MACRAW_TYPE_ARP 0x0806

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4 0x0800
#define ARP_HLEN_ETHERNET 0x06
#define ARP_PLEN_IPV4 0x04

#define ARP_OPER_REQUEST 0x0001
#define ARP_OPER_REPLY 0x0002

#define WIZ_TX 0b01
#define WIZ_RX 0b10
#define WIZ_SENDRECV WIZ_TX | WIZ_RX
//#define WIZ_SENDONLY WIZ_TX FIXME
#define WIZ_SENDONLY WIZ_SENDRECV

#endif // UDP_PRIVATE_H
