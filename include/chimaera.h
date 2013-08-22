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

#ifndef _CHIMAERA_H_
#define _CHIMAERA_H_

#include <stdint.h>

// hack to have access to PIN_MAP from plain C
#include <wirish/wirish_types.h>
extern const stm32_pin_info PIN_MAP [];
#include <board/board.h>

#define pin_set_mode(PIN, MODE) (gpio_set_mode (PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit, (MODE)))
#define pin_set_modef(PIN, MODE, FLAGS) (gpio_set_modef (PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit, (MODE), (FLAGS)))
#define pin_set_af(PIN, AF) (gpio_set_af (PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit, (AF)))
#define pin_write_bit(PIN, VAL) (gpio_write_bit (PIN_MAP[(PIN)].gpio_device, PIN_MAP[(PIN)].gpio_bit, (VAL)))

#define MUX_LENGTH 4
#define MUX_MAX 16

#if defined(SENSORS_48) // Mini
#	define ADC_LENGTH 3 // the number of connected sensor units
#	define ADC_DUAL_LENGTH 2 // the number of channels to be converted per ADC (1&2)
#	define ADC_SING_LENGTH 0 // the number of channels to be converted per ADC (3)
#elif defined(SENSORS_96) // Midi
#	define ADC_LENGTH 6
#	define ADC_DUAL_LENGTH 3
#	define ADC_SING_LENGTH 0
#elif defined(SENSORS_144) //Maxi
#	define ADC_LENGTH 9
#	define ADC_DUAL_LENGTH 4
#	define ADC_SING_LENGTH 1
#else
#	error "invalid number of sensors given to Make (-DSENSORS)"
#endif

#define SENSOR_N (MUX_MAX*ADC_LENGTH)
#define ADC_BITDEPTH 0xfff
#define ADC_HALF_BITDEPTH 0x7ff

#define BLOB_MAX 8
#define GROUP_MAX 8

#define OSC_ARGS_MAX 12

#if defined(MCU_STM32F303CC)
# define UDP_PWDN PB11
# define UDP_SS BOARD_SPI2_NSS_PIN
# define UDP_INT PA8
#elif defined(MCU_STM32F303CB)
# define UDP_PWDN PB12
# define UDP_SS PA9
# define UDP_INT PA10
#endif

#define CHIMAERA_BUFSIZE 0x400

#define ADC_DMA_PRIORITY 0x2
#define SPI_RX_DMA_PRIORITY 0x3
#define SPI_TX_DMA_PRIORITY 0x4
#define ADC_TIMER_PRIORITY 0x5
#define SNTP_TIMER_PRIORITY 0x6
#define CONFIG_TIMER_PRIORITY 0x7
#define TIMEOUT_TIMER_PRIORITY 0x8

#define ENGINE_MAX 5 // tuio, scsynth, oscmidi, dummy, rtpmidi

#define MAGIC 0x03

#define FACTORY_RESET_REG 1
#define FACTORY_RESET_VAL 666

#define EEPROM_SIZE 0x2000
#define EEPROM_CONFIG_OFFSET 0x0000
#define EEPROM_GROUP_OFFSET 0x0c00
#define EEPROM_RANGE_OFFSET 0x1000
#define EEPROM_RANGE_SIZE 0x0500
#define EEPROM_RANGE_MAX 2 // we have place for three slots: 0, 1, 2

// STM32F1 flash memory size (16bit)
#define FSIZE_BASE  ((const uint16_t *)0x1FFFF7CC)

// STM32F1 universal ID (96bit)
#define UID_BASE  ((const uint8_t *)0x1FFFF7AC)

extern uint_fast8_t buf_o_ptr;
extern uint8_t buf_o[2] [CHIMAERA_BUFSIZE]; // general purpose output buffer
extern uint8_t buf_i_o [];
extern uint8_t buf_i_i [CHIMAERA_BUFSIZE]; // general purpose input buffer

extern const float lookup_sqrt [];

extern timer_dev *adc_timer;
extern timer_dev *sntp_timer;
extern timer_dev *config_timer;

void cpp_setup ();

#endif
