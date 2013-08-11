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

/*
 * std lib headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * libmaple headers
 */
#include <libmaple/i2c.h> // I2C for eeprom
#include <libmaple/spi.h> // SPI for w5200
#include <libmaple/adc.h> // analog to digital converter
#include <libmaple/dma.h> // direct memory access
#include <libmaple/bkp.h> // backup register
#include <libmaple/dsp.h> // SIMD instructions

/*
 * include chimaera custom libraries
 */
#include <chimaera.h>
#include <chimutil.h>
#include <eeprom.h>
#include <sntp.h>
#include <config.h>
#include <tube.h>
#include <tuio2.h>
#include <scsynth.h>
#include <dump.h>
#include <ipv4ll.h>
#include <mdns.h>
#include <dns_sd.h>
#include <dhcpc.h>
#include <arp.h>
#include <rtpmidi.h>
#include <oscmidi.h>
#include <dummy.h>
#include <wiz.h>
#include <calibration.h>

uint8_t mux_sequence [MUX_LENGTH] = {PB5, PB4, PB3, PA15}; // digital out pins to switch MUX channels

#if defined(SENSORS_48) // Mini
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA4}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_order [ADC_LENGTH] = { 2, 0, 1 };
#elif defined(SENSORS_96) // Midi
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {}; // analog input pins read out by the ADC3
uint8_t adc_order [ADC_LENGTH] = { 5, 2, 4, 1, 3, 0 };
#elif defined(SENSORS_144) // Maxi
uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB0}; // analog input pins read out by the ADC3
uint8_t adc_order [ADC_LENGTH] = { 8, 4, 7, 3, 6, 2, 5, 1, 0 };
#endif

uint8_t adc1_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc2_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc3_raw_sequence [ADC_SING_LENGTH];

int16_t adc12_raw[2][MUX_MAX*ADC_DUAL_LENGTH*2] __attribute__((aligned(4))); // the dma temporary data array.
int16_t adc3_raw[2][MUX_MAX*ADC_SING_LENGTH] __attribute__((aligned(4)));

int16_t adc_sum[SENSOR_N] __CCM__;
int16_t adc_rela[SENSOR_N] __CCM__;
int16_t adc_swap[SENSOR_N] __CCM__;

uint8_t mux_order [MUX_MAX] = { 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x4, 0x5, 0x6, 0x7, 0x3, 0x2, 0x1, 0x0 };
uint8_t order12 [MUX_MAX*ADC_DUAL_LENGTH*2] __CCM__;
uint8_t order3 [MUX_MAX*ADC_SING_LENGTH] __CCM__;

volatile uint_fast8_t adc12_dma_done = 0;
volatile uint_fast8_t adc12_dma_err = 0;
volatile uint_fast8_t adc3_dma_done = 0;
volatile uint_fast8_t adc3_dma_err = 0;
volatile uint_fast8_t adc_time_up = 1;
volatile uint_fast8_t adc_raw_ptr = 1;
volatile uint_fast8_t mux_counter = MUX_MAX;
volatile uint_fast8_t sntp_should_request = 1; // send first request at boot
volatile uint_fast8_t sntp_should_listen = 0;
volatile uint_fast8_t mdns_should_listen = 0;
volatile uint_fast8_t config_should_listen = 0;

nOSC_Timestamp now;

static void
adc_timer_irq ()
{
	adc_time_up = 1;
	timer_pause (adc_timer);
}

static void
sntp_timer_irq ()
{
	sntp_should_request = 1;
}

static void
config_timer_irq ()
{
	config_should_listen = 1;
}

static void
mdns_timer_irq ()
{
	mdns_should_listen = 1;
}

void
__irq_adc1_2 ()
//adc1_2_irq (adc_callback_data *data)
{
	ADC1->regs->ISR |= ADC_ISR_EOS;
	//ADC1->regs->ISR |= data->irq_flags; // clear flags

	pin_write_bit (mux_sequence[0], mux_counter & 0b0001);
	pin_write_bit (mux_sequence[1], mux_counter & 0b0010);
	pin_write_bit (mux_sequence[2], mux_counter & 0b0100);
	pin_write_bit (mux_sequence[3], mux_counter & 0b1000);

	if (mux_counter < MUX_MAX)
	{
		mux_counter++;

		ADC1->regs->CR |= ADC_CR_ADSTART; // start master(ADC1) and slave(ADC2) conversion
#if (ADC_SING_LENGTH > 0)
		ADC3->regs->CR |= ADC_CR_ADSTART;
#endif
	}
}

void
__irq_adc3 ()
//adc3_irq (adc_callback_data *data)
{
	ADC3->regs->ISR |= ADC_ISR_EOS;
	//ADC3->regs->ISR |= data->irq_flags; // clear flags
}

static void
adc12_dma_irq ()
{
	uint8_t isr = dma_get_isr_bits (DMA1, DMA_CH1);
	dma_clear_isr_bits (DMA1, DMA_CH1);

	if (isr & 0x8)
		adc12_dma_err = 1;

	adc12_dma_done = 1;
}

static void
adc3_dma_irq ()
{
	uint8_t isr = dma_get_isr_bits (DMA2, DMA_CH5);
	dma_clear_isr_bits (DMA2, DMA_CH5);

	if (isr & 0x8)
		adc3_dma_err = 1;

	adc3_dma_done = 1;
}

static __always_inline void
adc_dma_run()
{
	adc12_dma_done = 0;
	adc3_dma_done = 0;
	mux_counter = 0;
	__irq_adc1_2 ();
}

static __always_inline void
adc_dma_block()
{ 
#if (ADC_SING_LENGTH > 0)
	while ( !adc12_dma_done || !adc3_dma_done )
		;
#else
	while ( !adc12_dma_done )
		;
#endif
	adc_raw_ptr ^= 1;
}

//TODO move to config.c
static void
config_bndl_start_cb (nOSC_Timestamp timestamp)
{
	//TODO only for testing
	debug_str ("bndl_start");
	debug_timestamp (timestamp);
}

static void
config_bndl_end_cb ()
{
	//TODO only for testing
	debug_str ("bndl_end");
}

static void
config_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	if (config.comm.subnet_check)
	{
		//TODO make a prober function out of this and put it into chimutil
		uint_fast8_t i;
		for (i=0; i<4; i++)
			if ( (ip[i] & config.comm.subnet[i]) != (config.comm.ip[i] & config.comm.subnet[i]) )
			{
				debug_str ("sender IP not part of subnet");
				return; // IP not part of same subnet as chimaera -> ignore message
			}
	}

	nosc_method_dispatch ((nOSC_Method *)config_serv, buf, len, config_bndl_start_cb, config_bndl_end_cb);
}

static void
sntp_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	if (config.comm.subnet_check)
	{
		uint_fast8_t i;
		for (i=0; i<4; i++)
			if ( (ip[i] & config.comm.subnet[i]) != (config.comm.ip[i] & config.comm.subnet[i]) )
			{
				debug_str ("sender IP not part of subnet");
				return; // IP not part of same subnet as chimaera -> ignore message
			}
	}

	sntp_timestamp_refresh (&now, NULL);
	sntp_dispatch (buf, now);

	sntp_should_listen = 0;
}

static void
mdns_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	mdns_dispatch (buf, len);
}

static void
adc_fill (uint_fast8_t raw_ptr)
{
	uint_fast8_t i;
	uint_fast8_t pos;
	int16_t *raw12 = adc12_raw[raw_ptr];
	int16_t *raw3 = adc3_raw[raw_ptr];
	uint16_t *qui = range.qui;
	uint32_t *rela_vec32 = (uint32_t *)adc_rela;
	uint32_t *sum_vec32 = (uint32_t *)adc_sum;
	uint32_t *qui_vec32 = (uint32_t *)range.qui;
	uint32_t *swap_vec32 = (uint32_t *)adc_swap;

	uint32_t zero = 0UL;

	if (config.movingaverage.enabled)
		for (i=0; i<SENSOR_N/2; i++)
		{
			uint32_t mean = __shadd16 (sum_vec32[i], zero); // mean = sum / 2
			mean = __shadd16 (mean, zero); // mean /= 2
			mean = __shadd16 (mean, zero); // mean /= 2
			sum_vec32[i] = __ssub16 (sum_vec32[i], mean); // sum -= mean
		}

	for (i=0; i<MUX_MAX*ADC_DUAL_LENGTH*2; i++)
	{
		pos = order12[i];
		adc_rela[pos] = raw12[i];
	}

#if (ADC_SING_LENGTH > 0)
	for (i=0; i<MUX_MAX*ADC_SING_LENGTH; i++)
	{
		pos = order3[i];
		adc_rela[pos] = raw3[i];
	}
#endif

	if (config.movingaverage.enabled)
	{
		if (config.dump.enabled)
			for (i=0; i<SENSOR_N/2; i++)
			{
				uint32_t rela;
				rela = __ssub16 (rela_vec32[i], qui_vec32[i]); // SIMD sub

				sum_vec32[i] = __sadd16 (sum_vec32[i], rela); // sum += rela
				rela = __shadd16 (sum_vec32[i], zero); // rela = sum /2
				rela = __shadd16 (rela, zero); // rela /= 2
				rela = __shadd16 (rela, zero); // rela /= 2
				rela_vec32[i] = rela;

				swap_vec32[i] = __rev16 (rela); // SIMD hton
			}
		else // !config.dump.enabled
			for (i=0; i<SENSOR_N/2; i++)
			{
				uint32_t rela;
				rela = __ssub16 (rela_vec32[i], qui_vec32[i]); // SIMD sub

				sum_vec32[i] = __sadd16 (sum_vec32[i], rela); // sum += rela
				rela = __shadd16 (sum_vec32[i], zero); // rela = sum /2
				rela = __shadd16 (rela, zero); // rela /= 2
				rela = __shadd16 (rela, zero); // rela /= 2
				rela_vec32[i] = rela;
			}
	}
	else // !config.movingaverage.enabled
	{
		if (config.dump.enabled)
			for (i=0; i<SENSOR_N/2; i++)
			{
				rela_vec32[i] = __ssub16 (rela_vec32[i], qui_vec32[i]); // SIMD sub
				swap_vec32[i] = __rev16 (rela_vec32[i]); // SIMD hton
			}
		else // !config.dump.enabled
			for (i=0; i<SENSOR_N/2; i++)
			{
				rela_vec32[i] = __ssub16 (rela_vec32[i], qui_vec32[i]); // SIMD sub
			}
	}
}

void
loop ()
{
	nOSC_Item nest_bndl [ENGINE_MAX];
	char nest_fmt [ENGINE_MAX+1];

	uint_fast8_t send_status = 0;
	uint_fast8_t cmc_job = 0;
	uint_fast16_t cmc_len = 0;
	uint_fast16_t len = 0;

	uint_fast8_t first = 1;
	nOSC_Timestamp offset;

//#define BENCHMARK
#ifdef BENCHMARK
	Stop_Watch sw_adc_fill = {.id = "adc_fill", .thresh=2000};
	Stop_Watch sw_output_send = {.id = "output_send", .thresh=2000};
	Stop_Watch sw_tuio_process = {.id = "tuio_process", .thresh=2000};
	Stop_Watch sw_output_serialize = {.id = "output_serialize", .thresh=2000};
	Stop_Watch sw_output_block = {.id = "output_block", .thresh=2000};
#endif // BENCHMARK

	while (1) // endless loop
	{
		if (config.rate)
		{
			adc_time_up = 0;
			timer_resume (adc_timer);
		}

		adc_dma_run();

		if (first) // in the first round there is no data
		{
			adc_dma_block();
			first = 0;
			continue;
		}

		if (calibrating)
			range_calibrate (adc12_raw[adc_raw_ptr], adc3_raw[adc_raw_ptr], order12, order3, adc_sum, adc_rela);
		else if (config.output.socket.enabled)
		{
			uint_fast8_t job = 0;

#ifdef BENCHMARK
			stop_watch_start (&sw_output_send);
#endif
			if (cmc_job) // start nonblocking sending of last cycles tuio output
				send_status = udp_send_nonblocking (config.output.socket.sock, !buf_o_ptr, cmc_len);

		// fill adc_rela
#ifdef BENCHMARK
			stop_watch_start (&sw_adc_fill);
#endif
			adc_fill (adc_raw_ptr);

			sntp_timestamp_refresh (&now, &offset);

			if (config.dump.enabled)
			{
				dump_update (now); // 6us
				nosc_item_bundle_set (nest_bndl, job++, dump_bndl, offset, dump_fmt);
			}
		
			if (config.tuio.enabled || config.scsynth.enabled || config.oscmidi.enabled || config.dummy.enabled || config.rtpmidi.enabled) // put all blob based engine flags here, e.g. TUIO, RTPMIDI, Kraken, SuperCollider, ...
			{
#ifdef BENCHMARK
				stop_watch_start (&sw_tuio_process);
#endif
				uint_fast8_t blobs = cmc_process (now, adc_rela, engines); // touch recognition of current cycle

				if (blobs) // was there any update?
				{
					if (config.tuio.enabled)
						nosc_item_bundle_set (nest_bndl, job++, tuio2_bndl, offset, tuio2_fmt);

					if (config.scsynth.enabled)
						nosc_item_bundle_set (nest_bndl, job++, scsynth_bndl, scsynth_timestamp, scsynth_fmt);

					if (config.oscmidi.enabled)
						nosc_item_bundle_set (nest_bndl, job++, oscmidi_bndl, oscmidi_timestamp, oscmidi_fmt);

					if (config.dummy.enabled)
						nosc_item_bundle_set (nest_bndl, job++, dummy_bndl, dummy_timestamp, dummy_fmt);

					if (config.rtpmidi.enabled) //FIXME we cannot run RTP-MIDI and OSC output at the same time
						cmc_len = rtpmidi_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);

					// if (config.kraken.enabled)
				}
			}

			if (job)
			{
#ifdef BENCHMARK
				stop_watch_start (&sw_output_serialize);
#endif
				if (job > 1)
				{
					memset (nest_fmt, nOSC_BUNDLE, job);
					nest_fmt[job] = nOSC_TERM;

					cmc_len = nosc_bundle_serialize (nest_bndl, nOSC_IMMEDIATE, nest_fmt, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
				}
				else // job == 1, there's no need to send a nested bundle in this case
				{
					nOSC_Item *first = &nest_bndl[0];
					cmc_len = nosc_bundle_serialize (first->bundle.bndl, first->bundle.tt, first->bundle.fmt, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
				}
			}

			if (config.rtpmidi.enabled && cmc_len) //FIXME we cannot run RTP-MIDI and OSC output at the same time
				job = 1;

#ifdef BENCHMARK
				stop_watch_start (&sw_output_block);
#endif
			if (cmc_job && send_status) // block for end of sending of last cycles tuio output
				udp_send_block (config.output.socket.sock); //FIXME check return status

			if (job) // switch output buffer
				buf_o_ptr ^= 1;

			cmc_job = job;

#ifdef BENCHMARK
			stop_watch_stop (&sw_output_send);
			stop_watch_stop (&sw_adc_fill);
			stop_watch_stop (&sw_tuio_process);
			stop_watch_stop (&sw_output_serialize);
			stop_watch_stop (&sw_output_block);
#endif
		}

		// run osc config server
		if (config.config.socket.enabled && config_should_listen)
		{
			udp_dispatch (config.config.socket.sock, buf_o_ptr, config_cb);
			config_should_listen = 0;
		}
		
		// run sntp client
		if (config.sntp.socket.enabled)
		{
			// listen for sntp request answer
			if (sntp_should_listen)
			{
				udp_dispatch (config.sntp.socket.sock, buf_o_ptr, sntp_cb);
			}

			// send sntp request
			if (sntp_should_request)
			{
				sntp_should_request = 0;
				sntp_should_listen = 1;

				sntp_timestamp_refresh (&now, NULL);
				len = sntp_request (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET], now);
				udp_send (config.sntp.socket.sock, buf_o_ptr, len);
			}
		}

		// run ZEROCONF server
		if (config.mdns.socket.enabled && mdns_should_listen)
		{
			udp_dispatch (config.mdns.socket.sock, buf_o_ptr, mdns_cb);
			mdns_should_listen = 0;
		}

		adc_dma_block();

		if (config.rate)
			while (!adc_time_up)
				;
	} // endless loop
}

void
adc_timer_reconfigure ()
{
	// this scheme is goof for rates in the range of 20-2000+
	uint16_t prescaler = 100-1;
	uint16_t reload = 72e6 / (config.rate * 100);
	uint16_t compare = reload;

	timer_set_prescaler (adc_timer, prescaler);
	timer_set_reload (adc_timer, reload);
	timer_set_mode (adc_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare (adc_timer, TIMER_CH1, compare);
	timer_attach_interrupt (adc_timer, TIMER_CH1, adc_timer_irq);
	timer_generate_update (adc_timer);

	nvic_irq_set_priority (NVIC_TIMER1_CC, ADC_TIMER_PRIORITY);
}

void 
sntp_timer_reconfigure ()
{
	uint16_t prescaler = 0xffff; 
	uint16_t reload = 72e6 / 0xffff * config.sntp.tau;
	uint16_t compare = reload;

	timer_set_prescaler (sntp_timer, prescaler);
	timer_set_reload (sntp_timer, reload);
	timer_set_mode (sntp_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare (sntp_timer, TIMER_CH1, compare);
	timer_attach_interrupt (sntp_timer, TIMER_CH1, sntp_timer_irq);
	timer_generate_update (sntp_timer);

	nvic_irq_set_priority (NVIC_TIMER2, SNTP_TIMER_PRIORITY);
}

void
config_timer_reconfigure ()
{
	uint16_t prescaler = 72e6 / 0xffff;
	uint16_t reload = 0xffff / config.config.rate;
	uint16_t compare = reload;

	timer_set_prescaler (config_timer, prescaler);
	timer_set_reload (config_timer, reload);

	timer_set_mode (config_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare (config_timer, TIMER_CH1, compare);
	timer_attach_interrupt (config_timer, TIMER_CH1, config_timer_irq);

	timer_set_mode (config_timer, TIMER_CH2, TIMER_OUTPUT_COMPARE);
	timer_set_compare (config_timer, TIMER_CH2, compare);
	timer_attach_interrupt (config_timer, TIMER_CH2, mdns_timer_irq);

	timer_generate_update (config_timer);

	nvic_irq_set_priority (NVIC_TIMER3, CONFIG_TIMER_PRIORITY);
}

void
setup ()
{
	uint_fast8_t i;
	uint_fast8_t p;

	pin_set_modef (BOARD_BUTTON_PIN, GPIO_MODE_INPUT, GPIO_MODEF_PUPD_NONE);
	pin_set_modef (BOARD_LED_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit (BOARD_LED_PIN, 1);

	// setup pins to switch the muxes
	for (i=0; i<MUX_LENGTH; i++)
		pin_set_modef (mux_sequence[i], GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);

	// setup nalog input pins
	for (i=0; i<ADC_DUAL_LENGTH; i++)
	{
		pin_set_modef (adc1_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);
		pin_set_modef (adc2_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);
	}
	for (i=0; i<ADC_SING_LENGTH; i++)
		pin_set_modef (adc3_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);

	// SPI for W5200
	spi_init(SPI2);
	spi_data_size(SPI2, SPI_DATA_SIZE_8_BIT);
	spi_master_enable(SPI2, SPI_CR1_BR_PCLK_DIV_2, SPI_MODE_0,
													SPI_CR1_BIDIMODE_2_LINE | SPI_FRAME_MSB | SPI_CR1_SSM | SPI_CR1_SSI);
	spi_config_gpios(SPI2, 1,
		PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit,
		PIN_MAP[BOARD_SPI2_SCK_PIN].gpio_device, PIN_MAP[BOARD_SPI2_SCK_PIN].gpio_bit, PIN_MAP[BOARD_SPI2_MISO_PIN].gpio_bit, PIN_MAP[BOARD_SPI2_MOSI_PIN].gpio_bit);
	pin_set_af(BOARD_SPI2_NSS_PIN, GPIO_AF_0); // we want to handle NSS by software
	pin_set_modef(BOARD_SPI2_NSS_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP); // we want to handle NSS by software
	pin_write_bit(BOARD_SPI2_NSS_PIN, 1);

	pin_set_modef(UDP_SS, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(UDP_SS, 1);

	pin_set_modef(UDP_PWDN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(UDP_PWDN, 0);

	pin_set_modef (UDP_INT, GPIO_MODE_INPUT, GPIO_PUPDR_NOPUPD);
	/*
	exti_attach_interrupt((exti_num)(PIN_MAP[UDP_INT].gpio_bit),
		gpio_exti_port(PIN_MAP[UDP_INT].gpio_device),
		wiz_irq,
		EXTI_FALLING);
	*/

	// systick 
	systick_disable ();
	systick_init ((SYSTICK_RELOAD_VAL + 1)/10 - 1); // systick every 100us = every 7200 cycles on a maple mini @72MHz

	// create EUI-32 and EUI-48 from unique identifier
	eui_init ();

	// initialize random number generator based on EUI_32
	uint32_t *seed = (uint32_t *)&EUI_32[0];
	srand (*seed);

	// init eeprom for I2C1
	eeprom_init (I2C1);
	eeprom_slave_init (eeprom_24LC64, I2C1, 0b000);
	eeprom_slave_init (eeprom_24AA025E48, I2C1, 0b001);

	// load configuration from eeprom
	/* FIXME
	uint16_t bkp_val = bkp_read (FACTORY_RESET_REG); //FIXME does not yet work
	if (bkp_val == FACTORY_RESET_VAL)
		bkp_init (); // clear backup register
	else
		config_load ();
	*/

	// rebuild engines stack
	cmc_engines_update ();

	// read MAC from MAC EEPROM
	uint8_t MAC [6];
	if (!config.comm.locally)
		eeprom_bulk_read (eeprom_24AA025E48, 0xfa, config.comm.mac, 6);

	// load calibrated sensor ranges from eeprom
	range_load (config.calibration);

	// init DMA, which is used for SPI and ADC
	dma_init (DMA1);
	dma_init (DMA2);

	pin_write_bit (BOARD_LED_PIN, 0);

	// initialize wiz820io
	uint8_t tx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	uint8_t rx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	wiz_init (PIN_MAP[UDP_SS].gpio_device, PIN_MAP[UDP_SS].gpio_bit, tx_mem, rx_mem);
	wiz_mac_set (config.comm.mac);

	// wait for link up before proceeding
	while (!wiz_link_up ()) // TODO monitor this and go to sleep mode when link is down
		;

	pin_write_bit (BOARD_LED_PIN, 1);

	wiz_init (PIN_MAP[UDP_SS].gpio_device, PIN_MAP[UDP_SS].gpio_bit, tx_mem, rx_mem); //TODO solve this differently

	uint_fast8_t claimed = 0;
	if (config.dhcpc.socket.enabled)
	{
		dhcpc_enable (config.dhcpc.socket.enabled);
		claimed = dhcpc_claim (config.comm.ip, config.comm.gateway, config.comm.subnet);
		dhcpc_enable (0); // disable socket again
	}
	if (!claimed && config.ipv4ll.enabled)
		IPv4LL_claim (config.comm.ip, config.comm.gateway, config.comm.subnet);

	wiz_comm_set (config.comm.mac, config.comm.ip, config.comm.gateway, config.comm.subnet);

	// initialize timers
	adc_timer = TIMER1;
	sntp_timer = TIMER2;
	config_timer = TIMER3;

	timer_init (adc_timer);
	timer_init (sntp_timer);
	timer_init (config_timer);

	timer_pause (adc_timer);
	adc_timer_reconfigure ();

	// initialize sockets
	output_enable (config.output.socket.enabled);
	config_enable (config.config.socket.enabled);
	sntp_enable (config.sntp.socket.enabled);
	debug_enable (config.debug.socket.enabled);
	mdns_enable (config.mdns.socket.enabled);

	// set up ADCs
	adc_disable (ADC1);
	adc_disable (ADC2);
	adc_disable (ADC3);
	adc_disable (ADC4);

	adc_set_exttrig (ADC1, ADC_EXTTRIG_MODE_SOFTWARE);
	adc_set_exttrig (ADC2, ADC_EXTTRIG_MODE_SOFTWARE);
	adc_set_exttrig (ADC3, ADC_EXTTRIG_MODE_SOFTWARE);

	adc_set_sample_rate (ADC1, ADC_SMPR_61_5); //TODO make this configurable
	adc_set_sample_rate (ADC2, ADC_SMPR_61_5);
	adc_set_sample_rate (ADC3, ADC_SMPR_61_5);

	// fill raw sequence array with corresponding ADC channels
	for (i=0; i<ADC_DUAL_LENGTH; i++)
	{
		adc1_raw_sequence[i] = PIN_MAP[adc1_sequence[i]].adc_channel;
		adc2_raw_sequence[i] = PIN_MAP[adc2_sequence[i]].adc_channel;
	}
	for (i=0; i<ADC_SING_LENGTH; i++)
		adc3_raw_sequence[i] = PIN_MAP[adc3_sequence[i]].adc_channel;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_DUAL_LENGTH*2; i++)
			order12[p*ADC_DUAL_LENGTH*2 + i] = mux_order[p] + adc_order[i]*MUX_MAX;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_SING_LENGTH; i++)
			order3[p*ADC_SING_LENGTH + i] = mux_order[p] + adc_order[ADC_DUAL_LENGTH*2+i]*MUX_MAX;

	// set channels in register
	adc_set_conv_seq (ADC1, adc1_raw_sequence, ADC_DUAL_LENGTH);
	adc_set_conv_seq (ADC2, adc2_raw_sequence, ADC_DUAL_LENGTH);
#if (ADC_SING_LENGTH > 0)
	adc_set_conv_seq (ADC3, adc3_raw_sequence, ADC_SING_LENGTH);
#endif

	ADC1->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	nvic_irq_enable (NVIC_ADC1_2);
	//adc_attach_interrupt (ADC1, ADC_IER_EOS, adc1_2_irq, NULL);

#if (ADC_SING_LENGTH > 0)
	ADC3->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	nvic_irq_enable (NVIC_ADC3);
	//adc_attach_interrupt (ADC3, ADC_IER_EOS, adc3_irq, NULL);

	ADC3->regs->CFGR |= ADC_CFGR_DMAEN; // enable DMA request
	ADC3->regs->CFGR |= ADC_CFGR_DMACFG, // enable ADC circular mode for use with DMA
#endif

	ADC12_BASE->CCR |= ADC_MDMA_MODE_ENABLE_12_10_BIT; // enable ADC DMA in 12-bit dual mode
	ADC12_BASE->CCR |= ADC_CCR_DMACFG; // enable ADC circular mode for use with DMA
	ADC12_BASE->CCR |= ADC_MODE_DUAL_REGULAR_ONLY; // set DMA dual mode to regular channels only

	adc_enable (ADC1);
	adc_enable (ADC2);
#if (ADC_SING_LENGTH > 0)
	adc_enable (ADC3);
#endif

	// set up ADC DMA tubes
	int status;

	adc_tube12.tube_dst = (void *)adc12_raw;
	status = dma_tube_cfg (DMA1, DMA_CH1, &adc_tube12);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	dma_set_priority (DMA1, DMA_CH1, DMA_PRIORITY_MEDIUM);    //Optional
	dma_set_num_transfers (DMA1, DMA_CH1, ADC_DUAL_LENGTH*MUX_MAX*2);
	dma_attach_interrupt (DMA1, DMA_CH1, adc12_dma_irq);
	dma_enable (DMA1, DMA_CH1);                //CCR1 EN bit 0
	nvic_irq_set_priority (NVIC_DMA_CH1, ADC_DMA_PRIORITY);

#if (ADC_SING_LENGTH > 0)
	adc_tube3.tube_dst = (void *)adc3_raw;
	status = dma_tube_cfg (DMA2, DMA_CH5, &adc_tube3);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	dma_set_priority (DMA2, DMA_CH5, DMA_PRIORITY_MEDIUM);
	dma_set_num_transfers (DMA2, DMA_CH5, ADC_SING_LENGTH*MUX_MAX*2);
	dma_attach_interrupt (DMA2, DMA_CH5, adc3_dma_irq);
	dma_enable (DMA2, DMA_CH5);
	nvic_irq_set_priority (NVIC_DMA_CH5, ADC_DMA_PRIORITY);
#endif

	// set up continuous music controller struct
	cmc_init ();
	dump_init (sizeof(adc_swap), adc_swap);
	tuio2_init ();
	scsynth_init ();
	oscmidi_init ();
	dummy_init ();
	rtpmidi_init ();

	// load saved groups
	groups_load ();
}

void
main ()
{
	cpp_setup ();
  setup ();
	loop ();
}
