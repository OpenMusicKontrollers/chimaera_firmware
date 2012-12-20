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

#ifndef _UDP_H_
#define _UDP_H_

#include <stdint.h>

#include <libmaple/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_HDR_SIZE 8
#define UDP_SEND_OFFSET 4

#define UDP_RESET 7 // Reset BIT
#define UDP_MAX_SOCK_NUM 8

void udp_init (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet, gpio_dev *dev, uint8_t bit, uint8_t tx_mem[UDP_MAX_SOCK_NUM], uint8_t rx_mem[UDP_MAX_SOCK_NUM]);
uint8_t udp_link_up ();

void udp_mac_set (uint8_t *mac);
void udp_ip_set (uint8_t *ip);
void udp_gateway_set (uint8_t *gateway);
void udp_subnet_set (uint8_t *subnet);

void udp_begin (uint8_t sock, uint16_t port, uint8_t multicast);
void udp_set_remote (uint8_t sock, uint8_t *ip, uint16_t port);
void udp_set_remote_har (uint8_t sock, uint8_t *har);
void udp_send (uint8_t sock, uint8_t ptr, uint16_t len);
uint8_t udp_send_nonblocking (uint8_t sock, uint8_t ptr, uint16_t len);
void udp_send_block (uint8_t sock);
void udp_probe (uint8_t sock);
uint16_t udp_available (uint8_t sock);
void udp_receive (uint8_t sock, uint8_t ptr, uint16_t len);
void udp_dispatch (uint8_t sock, uint8_t ptr, void (*cb) (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)); 

#ifdef __cplusplus
}
#endif

#endif
