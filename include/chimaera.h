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

#ifndef _CHIMAERA_H_
#define _CHIMAERA_H_

#include <stdint.h>

// hack to have access to PIN_MAP from plain C
#include <wirish/wirish_types.h>
extern const stm32_pin_info PIN_MAP [];
#include <board/board.h>

//#define BENCHMARK

#define GROUP_MAX 8

#define pin_set_mode(PIN, MODE)(gpio_set_mode(PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit,(MODE)))
#define pin_set_modef(PIN, MODE, FLAGS)(gpio_set_modef(PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit,(MODE),(FLAGS)))
#define pin_set_af(PIN, AF)(gpio_set_af(PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit,(AF)))
#define pin_write_bit(PIN, VAL)(gpio_write_bit(PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit,(VAL)))
#define pin_read_bit(PIN)(gpio_read_bit(PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit))

#define MUX_MAX 16

#if SENSOR_N == 16
#	define ADC_LENGTH 1 // the number of connected sensor units
#	define ADC_DUAL_LENGTH 0 // the number of channels to be converted per ADC(1&2)
#	define ADC_SING_LENGTH 1 // the number of channels to be converted per ADC(3)
#elif SENSOR_N == 32
#	define ADC_LENGTH 2
#	define ADC_DUAL_LENGTH 1
#	define ADC_SING_LENGTH 0
#elif SENSOR_N == 48
#	define ADC_LENGTH 3
#	define ADC_DUAL_LENGTH 1
#	define ADC_SING_LENGTH 1
#elif SENSOR_N == 64
#	define ADC_LENGTH 4
#	define ADC_DUAL_LENGTH 2
#	define ADC_SING_LENGTH 0
#elif SENSOR_N == 80
#	define ADC_LENGTH 5
#	define ADC_DUAL_LENGTH 2
#	define ADC_SING_LENGTH 1
#elif SENSOR_N == 96
#	define ADC_LENGTH 6
#	define ADC_DUAL_LENGTH 3
#	define ADC_SING_LENGTH 0
#elif SENSOR_N == 112
#	define ADC_LENGTH 7
#	define ADC_DUAL_LENGTH 3
#	define ADC_SING_LENGTH 1
#elif SENSOR_N == 128
#	define ADC_LENGTH 8
#	define ADC_DUAL_LENGTH 4
#	define ADC_SING_LENGTH 0
#elif SENSOR_N == 144
#	define ADC_LENGTH 9
#	define ADC_DUAL_LENGTH 4
#	define ADC_SING_LENGTH 1
#elif SENSOR_N == 160
#	define ADC_LENGTH 10
#	define ADC_DUAL_LENGTH 4
#	define ADC_SING_LENGTH 2
#else
#	error "invalid number of sensors given to Make(-DSENSOR_N)" SENSOR_N
#endif

#define ADC_UNUSED_LENGTH (10 - ADC_LENGTH)

#define ADC_BITDEPTH 0xfff
#define ADC_HALF_BITDEPTH 0x7ff

#define BLOB_MAX 8

#define CHIMAERA_BUFSIZE 0x500

#define ADC_DMA_PRIORITY 0x2
#define SPI_RX_DMA_PRIORITY 0x3
#define SPI_TX_DMA_PRIORITY 0x4
#define ADC_TIMER_PRIORITY 0x5
#define SYNC_TIMER_PRIORITY 0x6
#define CONFIG_TIMER_PRIORITY 0x7
#define DHCPC_TIMER_PRIORITY 0x8
#define MDNS_TIMER_PRIORITY 0x9

#define BOOT_MODE_REG 0xF

#define RESET_MODE_REG 1
typedef enum _Reset_Mode {
	RESET_MODE_FLASH_SOFT		= 0,
	RESET_MODE_FLASH_HARD		= 1,
	RESET_MODE_SYSTEM_FLASH	= 2,
} Reset_Mode;
	
extern Reset_Mode reset_mode;

#define EEPROM_SIZE 0x2000
#define EEPROM_CONFIG_OFFSET 0x0000
#define EEPROM_RANGE_OFFSET 0x1000
#define EEPROM_RANGE_SIZE 0x0510
#define EEPROM_RANGE_MAX 2 // we have place for three slots: 0, 1, 2

// WIZnet interfacing
#if REVISION == 3

#	define MUX_LENGTH 4

#	define WIZ_SPI_DEV SPI2
#	define WIZ_SPI_BAS SPI2_BASE

#	define WIZ_SPI_NSS_PIN BOARD_SPI2_NSS_PIN
#	define WIZ_SPI_SCK_PIN BOARD_SPI2_SCK_PIN
#	define WIZ_SPI_MISO_PIN BOARD_SPI2_MISO_PIN
#	define WIZ_SPI_MOSI_PIN BOARD_SPI2_MOSI_PIN

#	define WIZ_SPI_RX_DMA_DEV DMA1
#	define WIZ_SPI_RX_DMA_TUB DMA_CH4
#	define WIZ_SPI_RX_DMA_SRC DMA_REQ_SRC_SPI2_RX

#	define WIZ_SPI_TX_DMA_DEV DMA1
#	define WIZ_SPI_TX_DMA_TUB DMA_CH5
#	define WIZ_SPI_TX_DMA_SRC DMA_REQ_SRC_SPI2_TX

#	define UDP_PWDN PB12
#	define UDP_SS PA9
#	define UDP_INT PA10

#	define SOFT_RESET PA14
#	define CHIM_LED_PIN PB10

#	define EEPROM_DEV I2C1

#elif REVISION == 4

#	define MUX_LENGTH 2

#	define WIZ_SPI_DEV SPI1
#	define WIZ_SPI_BAS SPI1_BASE

#	define WIZ_SPI_NSS_PIN BOARD_SPI3_NSS_PIN
#	define WIZ_SPI_SCK_PIN BOARD_SPI3_SCK_PIN
#	define WIZ_SPI_MISO_PIN BOARD_SPI3_MISO_PIN
#	define WIZ_SPI_MOSI_PIN BOARD_SPI3_MOSI_PIN

#	define WIZ_SPI_RX_DMA_DEV DMA1
#	define WIZ_SPI_RX_DMA_TUB DMA_CH2
#	define WIZ_SPI_RX_DMA_SRC DMA_REQ_SRC_SPI1_RX

#	define WIZ_SPI_TX_DMA_DEV DMA1
#	define WIZ_SPI_TX_DMA_TUB DMA_CH3
#	define WIZ_SPI_TX_DMA_SRC DMA_REQ_SRC_SPI1_TX

#	define UDP_SS BOARD_SPI3_NSS_PIN
#	define UDP_INT PA14

#	define SOFT_RESET PA8
#	define CHIM_LED_PIN PB2

#	define EEPROM_DEV I2C2

#endif

// STM32F3 flash memory size(16bit)
#define FSIZE_BASE ((const uint16_t *)0x1FFFF7CC)

// STM32F3 universal ID(96bit)
#define UID_BASE ((const uint8_t *)0x1FFFF7AC)

extern uint_fast8_t buf_o_ptr;
extern const uint_fast8_t buf_i_ptr;

extern uint8_t buf_o[2] [CHIMAERA_BUFSIZE]; // general purpose output buffer
extern uint8_t buf_i[1] [CHIMAERA_BUFSIZE]; // general purpose input buffer

#define BUF_O_BASE(ptr)(buf_o[ptr])
#define BUF_I_BASE(ptr)(buf_i[ptr])

#define BUF_O_OFFSET(ptr)(buf_o[ptr] + WIZ_SEND_OFFSET)
#define BUF_I_OFFSET(ptr)(buf_i[ptr] + WIZ_SEND_OFFSET)

//FIXME conditional compilation
//#define BUF_O_MAX(ptr)(buf_o[ptr] + CHIMAERA_BUFSIZE - 2*(WIZ_SEND_OFFSET-3)) // WIZ5200
#define BUF_O_MAX(ptr)(buf_o[ptr] + CHIMAERA_BUFSIZE - (WIZ_SEND_OFFSET-3)) // WIZ5500

#define adc_timer TIMER1
#define NVIC_ADC_TIMER	NVIC_TIMER1_CC

#define sync_timer TIMER2
#define NVIC_SYNC_TIMER NVIC_TIMER2

#define ptp_timer TIMER3
#define NVIC_PTP_TIMER NVIC_TIMER3

#define mdns_timer TIMER4
#define NVIC_MDNS_TIMER NVIC_TIMER4

#define dhcpc_timer TIMER15
#define NVIC_DHCP_TIMER NVIC_TIMER1_BRK_TIMER15

void cpp_setup(void);

#endif // _CHIMIAERA_H_
