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

#ifndef _DMA_UDP_H_
#define _DMA_UDP_H_

#include <stdint.h>
#include <gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_HDR_SIZE 8

void dma_udp_init (uint8_t *mac, uint8_t *ip, uint8_t *gateway, uint8_t *subnet, gpio_dev *dev, uint8_t bit);
void dma_udp_begin (uint8_t sock, uint16_t port);
void dma_udp_set_remote (uint8_t sock, uint8_t *ip, uint16_t port);
void dma_udp_send (uint8_t sock, uint8_t *dat, uint16_t len);
uint16_t dma_udp_available (uint8_t sock);
void dma_udp_receive (uint8_t sock, uint8_t *buf, uint16_t len);

void dma_udp_send_nonblocking_1 (uint8_t sock, uint8_t *dat, uint16_t len);
void dma_udp_send_nonblocking_2 (uint8_t sock, uint8_t *dat, uint16_t len);
void dma_udp_send_nonblocking_3 (uint8_t sock, uint8_t *dat, uint16_t len);
void dma_udp_send_nonblocking_4 (uint8_t sock, uint8_t *dat, uint16_t len);
void dma_udp_send_nonblocking_5 (uint8_t sock, uint8_t *dat, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
