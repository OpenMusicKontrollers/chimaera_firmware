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

#ifndef WIZ_H_
#define WIZ_H_

#include <stdint.h>

#include <libmaple/gpio.h>

#define WIZ_UDP_HDR_SIZE 8
#define WIZ_SEND_OFFSET 4

#define WIZ_MAX_SOCK_NUM 8

typedef void (*UDP_Dispatch_Cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len);

void wiz_init (gpio_dev *dev, uint8_t bit, uint8_t tx_mem[WIZ_MAX_SOCK_NUM], uint8_t rx_mem[WIZ_MAX_SOCK_NUM]);
uint8_t wiz_link_up ();

void wiz_mac_set (uint8_t *mac);
void wiz_ip_set (uint8_t *ip);
void wiz_gateway_set (uint8_t *gateway);
void wiz_subnet_set (uint8_t *subnet);
void wiz_comm_set (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet);

/* 
 * UDP
 */

void udp_begin (uint8_t sock, uint16_t port, uint8_t multicast);
void udp_end (uint8_t sock);

void udp_update_read_write_pointers (uint8_t sock);
void udp_reset_read_write_pointers (uint8_t sock);

void udp_set_remote (uint8_t sock, uint8_t *ip, uint16_t port);
void udp_set_remote_har (uint8_t sock, uint8_t *har);

uint8_t udp_send (uint8_t sock, uint8_t ptr, uint16_t len);
uint8_t udp_send_nonblocking (uint8_t sock, uint8_t ptr, uint16_t len);
uint8_t udp_send_block (uint8_t sock);

uint16_t udp_available (uint8_t sock);

uint8_t udp_receive (uint8_t sock, uint8_t ptr, uint16_t len);
uint16_t udp_receive_nonblocking (uint8_t sock, uint8_t ptr, uint16_t len);
uint8_t udp_receive_block (uint8_t sock, uint16_t wrap, uint16_t len);

void udp_dispatch (uint8_t sock, uint8_t ptr, UDP_Dispatch_Cb cb);

/*
 * MACRAW
 */

typedef struct _MACRAW_Header MACRAW_Header;

struct _MACRAW_Header {
	uint8_t dst_mac [6];
	uint8_t src_mac [6];
	uint16_t type;
} __attribute((packed,aligned(2)));

#define MACRAW_HEADER_SIZE sizeof(MACRAW_Header)
#define ARP_PAYLOAD_SIZE sizeof(ARP_Payload)

typedef void (*MACRAW_Dispatch_Cb) (uint8_t *buf, uint16_t len, void *data);

void macraw_begin (uint8_t sock, uint8_t mac_filter);
void macraw_end (uint8_t sock);
uint8_t macraw_send (uint8_t sock, uint8_t ptr, uint16_t len);
uint16_t macraw_available (uint8_t sock);
uint8_t macraw_receive (uint8_t sock, uint8_t ptr, uint16_t len);
void macraw_dispatch (uint8_t sock, uint8_t ptr, MACRAW_Dispatch_Cb cb, void *data);

#endif
