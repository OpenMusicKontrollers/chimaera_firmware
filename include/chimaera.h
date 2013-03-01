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

#ifndef _CHIMAERA_H_
#define _CHIMAERA_H_

#include <stdint.h>

#include <netdef.h>
#include <nosc.h>
#include <cmc.h>

// ugly hack to have access to PIN_MAP from plain C
#include <wirish/wirish_types.h>
extern const stm32_pin_info PIN_MAP [];
#include <board/board.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUX_LENGTH 4
#define MUX_MAX 16
#define ADC_LENGTH 9 // the number of channels to be converted per ADC  
#define ADC_DUAL_LENGTH 5 // the number of channels to be converted per ADC 

#define ADC_BITDEPTH 0xfff
#define ADC_HALF_BITDEPTH 0x7ff

#define SENSOR_N (MUX_MAX*ADC_LENGTH)
#define BLOB_MAX 8
#define GROUP_MAX 16

#define OSC_ARGS_MAX 12

#define ADC_CR1_DUALMOD_BIT 16

#define UDP_PWDN 0
#define UDP_INT 27

#define CHIMAERA_BUFSIZE 0x280 //FIXME increase to 0x400

#define ADC_DMA_PRIORITY 0x2
#define SPI_RX_DMA_PRIORITY 0x3
#define SPI_TX_DMA_PRIORITY 0x4
#define ADC_TIMER_PRIORITY 0x5
#define SNTP_TIMER_PRIORITY 0x6
#define CONFIG_TIMER_PRIORITY 0x7
#define TIMEOUT_TIMER_PRIORITY 0x8

#define ENGINE_N 3 // tuio, dump, scsynth

#define MAGIC 0x03

#define FACTORY_RESET_REG 1
#define FACTORY_RESET_VAL 666

#define EEPROM_CONFIG_OFFSET 0x0000
#define EEPROM_GROUP_OFFSET_DATA 0x00e0
#define EEPROM_GROUP_OFFSET_SIZE EEPROM_GROUP_OFFSET_DATA-0x1
#define EEPROM_RANGE_OFFSET 0x01a0
#define EEPROM_RANGE_SIZE 0x0a20 // sizeof(ADC_Range)*144
#define EEPROM_RANGE_MAX 2 // we have palce for three slots: 0, 1 and 2

// STM32F1 flash memory size (16bit)
#define FSIZE_BASE  ((const uint16_t *)0x1FFFF7E0)

// STM32F1 universal ID (96bit)
#define UID_BASE  ((const uint8_t *)0x1FFFF7E8)

extern uint8_t buf_o_ptr;
extern uint8_t buf_o[2] [CHIMAERA_BUFSIZE]; // general purpose output buffer
extern uint8_t buf_i_o [];
extern uint8_t buf_i_i [CHIMAERA_BUFSIZE]; // general purpose input buffer

extern uint8_t calibrating;

#ifdef __cplusplus
}
#endif

#endif
