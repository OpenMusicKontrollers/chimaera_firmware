/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
#include <libmaple/syscfg.h> // syscfg register
#include <libmaple/systick.h> // systick
#include <series/simd.h> // SIMD instructions

/*
 * include chimaera custom libraries
 */
#include <chimaera.h>
#include <chimutil.h>
#include <debug.h>
#include <eeprom.h>
#include <ptp.h>
#include <sntp.h>
#include <config.h>
#include <tube.h>
#include <ipv4ll.h>
#include <mdns-sd.h>
#include <dhcpc.h>
#include <arp.h>
#include <engines.h>
#include <wiz.h>
#include <calibration.h>
#include <sensors.h>
#include <osc.h>

static uint8_t adc1_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
static uint8_t adc2_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
static uint8_t adc3_raw_sequence [ADC_SING_LENGTH];

static int16_t adc12_raw[2][MUX_MAX*ADC_DUAL_LENGTH*2] __attribute__((aligned(4))); // the dma temporary data array.
static int16_t adc3_raw[2][MUX_MAX*ADC_SING_LENGTH] __attribute__((aligned(4)));

static int16_t adc_sum[SENSOR_N];
static int16_t adc_rela[SENSOR_N];
static int16_t adc_swap[SENSOR_N];

#if REVISION == 3
static uint8_t mux_sequence [MUX_LENGTH] = {PA15, PB3, PB4, PB5}; // digital out pins to switch MUX channels
#elif REVISION == 4
static uint8_t mux_sequence [MUX_LENGTH] = {PB8, PB9}; // MR and CP pins of 4-bit counter
#endif

static uint8_t mux_order [MUX_MAX] = {0xf, 0x4, 0xb, 0x3, 0xd, 0x6, 0x9, 0x1, 0xe, 0x5, 0xa, 0x2, 0xc, 0x7, 0x8, 0x0};
static uint8_t order12 [MUX_MAX*ADC_DUAL_LENGTH*2];
static uint8_t order3 [MUX_MAX*ADC_SING_LENGTH];

#if( (ADC_DUAL_LENGTH > 0) && (ADC_SING_LENGTH > 0) )
static volatile uint_fast8_t adc12_eos = 0;
static volatile uint_fast8_t adc3_eos = 0;
#endif
static volatile uint_fast8_t adc12_dma_done = 0;
static volatile uint_fast8_t adc12_dma_err = 0;
static volatile uint_fast8_t adc3_dma_done = 0;
static volatile uint_fast8_t adc3_dma_err = 0;
static volatile uint_fast8_t adc_time_up = 1;
static volatile uint_fast8_t adc_raw_ptr = 1;
static volatile uint_fast8_t mux_counter = MUX_MAX;
static volatile uint_fast8_t sync_should_request = 1; // send first request at boot
static volatile uint_fast8_t sntp_should_listen = 0;
static volatile uint_fast8_t ptp_should_request = 0;
static volatile uint_fast8_t ptp_event_should_listen = 0;
static volatile uint_fast8_t ptp_general_should_listen = 0;
static volatile uint_fast8_t mdns_should_listen = 0;
static volatile uint_fast8_t output_should_listen = 0;
static volatile uint_fast8_t config_should_listen = 0;
static volatile uint_fast8_t debug_should_listen = 0;
static volatile uint_fast8_t dhcpc_should_listen = 0;
static volatile uint_fast8_t dhcpc_needs_refresh = 0;
static volatile uint_fast8_t mdns_timeout = 0;
static volatile uint_fast8_t wiz_needs_attention = 0;
static volatile uint32_t wiz_irq_tick;
static volatile int64_t wiz_ptp_tick;

static OSC_Timetag now;

static void __CCM_TEXT__
adc_timer_irq()
{
	adc_time_up = 1;
	timer_pause(adc_timer);
}

static void __CCM_TEXT__
sync_timer_irq()
{
	sync_should_request = 1;
}

static void __CCM_TEXT__
dhcpc_timer_irq()
{
	dhcpc_needs_refresh = 1;
}

static void __CCM_TEXT__
mdns_timer_irq()
{
	mdns_timeout = 1;
	timer_pause(mdns_timer);
}

static void
soft_irq()
{
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_SOFT); // set soft reset flag
	bkp_disable_writes();
}

static void __CCM_TEXT__
wiz_irq()
{
	//TODO substract 12 cycles interrupt latency for ARM Cortex M4?
	wiz_ptp_tick = ptp_uptime();
	wiz_irq_tick = systick_uptime();
	wiz_needs_attention = 1;
}

static void __CCM_TEXT__
wiz_output_irq(uint8_t isr)
{
	output_should_listen = isr;
}

static void __CCM_TEXT__
wiz_config_irq(uint8_t isr)
{
	config_should_listen = isr;
}

static void __CCM_TEXT__
wiz_debug_irq(uint8_t isr)
{
	debug_should_listen = isr;
}

static void __CCM_TEXT__
wiz_mdns_irq(uint8_t isr)
{
	mdns_should_listen = isr;
}

static void __CCM_TEXT__
wiz_sntp_irq(uint8_t isr)
{
	sntp_should_listen = isr;
}

static void __CCM_TEXT__
ptp_timer_irq()
{
	ptp_should_request = 1;
	timer_pause(ptp_timer);
}

static void __CCM_TEXT__
wiz_ptp_event_irq(uint8_t isr)
{
	ptp_event_should_listen = isr;
}

static void __CCM_TEXT__
wiz_ptp_general_irq(uint8_t isr)
{
	ptp_general_should_listen = isr;
}

static void __CCM_TEXT__
wiz_dhcpc_irq(uint8_t isr)
{
	dhcpc_should_listen = isr;
}

static inline __always_inline void
_counter_inc()
{
#if REVISION == 3
	pin_write_bit(mux_sequence[0], mux_counter & 0b0001);
	pin_write_bit(mux_sequence[1], mux_counter & 0b0010);
	pin_write_bit(mux_sequence[2], mux_counter & 0b0100);
	pin_write_bit(mux_sequence[3], mux_counter & 0b1000);
#elif REVISION == 4
	if(mux_counter == 0)
	{
		// reset counter
		pin_write_bit(mux_sequence[0], 0); // MR
		pin_write_bit(mux_sequence[1], 1); // CP
		pin_write_bit(mux_sequence[0], 1); // MR
		pin_write_bit(mux_sequence[1], 0); // CP
	}
	else
	{
		// trigger counter
		pin_write_bit(mux_sequence[1], 1); // CP
		pin_write_bit(mux_sequence[1], 0); // CP
	}
#endif
}

static inline __always_inline void
_irq_adc_block()
{
#if(ADC_DUAL_LENGTH > 0)
# if(ADC_SING_LENGTH > 0)
	if(adc12_eos && adc3_eos)
	{
		_counter_inc();
		if(mux_counter < MUX_MAX)
		{
			mux_counter++;

			adc12_eos = 0;
			adc3_eos = 0;
			ADC1->regs->CR |= ADC_CR_ADSTART; // start master(ADC1) and slave(ADC2) conversion
			ADC3->regs->CR |= ADC_CR_ADSTART;
		}
	}
# else
	_counter_inc();
	if(mux_counter < MUX_MAX)
	{
		mux_counter++;

		ADC1->regs->CR |= ADC_CR_ADSTART; // start master(ADC1) and slave(ADC2) conversion
	}
# endif
#else
	_counter_inc();
	if(mux_counter < MUX_MAX)
	{
		mux_counter++;

		ADC3->regs->CR |= ADC_CR_ADSTART;
	}
#endif
}

void __CCM_TEXT__
__irq_adc1_2()
//adc1_2_irq(adc_callback_data *data)
{
	ADC1->regs->ISR |= ADC_ISR_EOS;
	//ADC1->regs->ISR |= data->irq_flags; // clear flags

#if( (ADC_DUAL_LENGTH > 0) && (ADC_SING_LENGTH > 0) )
	adc12_eos = 1;
#endif
	_irq_adc_block();
}

void __CCM_TEXT__
__irq_adc3()
//adc3_irq(adc_callback_data *data)
{
	ADC3->regs->ISR |= ADC_ISR_EOS;
	//ADC3->regs->ISR |= data->irq_flags; // clear flags

#if( (ADC_DUAL_LENGTH > 0) && (ADC_SING_LENGTH > 0) )
	adc3_eos = 1;
#endif
	_irq_adc_block();
}

static void __CCM_TEXT__
adc12_dma_irq()
{
	uint8_t isr = dma_get_isr_bits(DMA1, DMA_CH1);
	dma_clear_isr_bits(DMA1, DMA_CH1);

	if(isr & 0x8)
		adc12_dma_err = 1;

	adc12_dma_done = 1;
}

static void __CCM_TEXT__
adc3_dma_irq()
{
	uint8_t isr = dma_get_isr_bits(DMA2, DMA_CH5);
	dma_clear_isr_bits(DMA2, DMA_CH5);

	if(isr & 0x8)
		adc3_dma_err = 1;

	adc3_dma_done = 1;
}

static inline __always_inline void
adc_dma_run()
{
	adc12_dma_done = 0;
	adc3_dma_done = 0;
	mux_counter = 0;
#if( (ADC_DUAL_LENGTH > 0) && (ADC_SING_LENGTH > 0) )
	adc12_eos = 1;
	adc3_eos = 1;
#endif
	_irq_adc_block();
}

static inline __always_inline void
adc_dma_block()
{ 
#if(ADC_DUAL_LENGTH  > 0)
#	if(ADC_SING_LENGTH > 0)
	while( !adc12_dma_done || !adc3_dma_done ) // wait for all 3 ADCs to end
#	else // ADC_SING_LENGTH == 0
	while( !adc12_dma_done ) // wait for ADC12 to end
#	endif
#else // ADC_DUAL_LENGTH == 0
	while( !adc3_dma_done ) // wait for ADC3 to end
#endif
		;
	adc_raw_ptr ^= 1;
}

static void __CCM_TEXT__
config_cb(uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	if(osc_packet_check(buf, len))
		osc_method_dispatch(buf, len, config_serv);
	else
		DEBUG("s", "invalid OSC packet");
}

static void __CCM_TEXT__
sntp_cb(uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	sntp_timestamp_refresh(wiz_irq_tick, &now, NULL);
	sntp_dispatch(buf, now);
}

static void __CCM_TEXT__
ptp_cb(uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	ptp_dispatch(buf, wiz_ptp_tick);
}

static void __CCM_TEXT__
mdns_cb(uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	mdns_dispatch(buf, len);
}

// loops are explicitely unrolled which makes it fast but cumbersome to read
static void __CCM_TEXT__
adc_fill(uint_fast8_t raw_ptr)
{
	uint_fast8_t i;
	uint_fast8_t pos;
	int16_t *raw12 = adc12_raw[raw_ptr];
	int16_t *raw3 = adc3_raw[raw_ptr];
	uint16_t *qui = range.qui;
	uint32_t *rela_vec32 =(uint32_t *)adc_rela;
	uint32_t *sum_vec32 =(uint32_t *)adc_sum;
	uint32_t *qui_vec32 =(uint32_t *)range.qui;
	uint32_t *swap_vec32 =(uint32_t *)adc_swap;

	uint32_t zero = 0UL;
	uint_fast8_t dump_enabled = config.dump.enabled; // local copy
	uint_fast8_t movingaverage_enabled = config.sensors.movingaverage_bitshift > 0; // local copy
	uint_fast8_t bitshift = config.sensors.movingaverage_bitshift; // local copy

#if(ADC_DUAL_LENGTH > 0)
	for(i=0; i<MUX_MAX*ADC_DUAL_LENGTH*2; i++)
	{
		pos = order12[i];
		adc_rela[pos] = raw12[i];
	}
#endif

#if(ADC_SING_LENGTH > 0)
	for(i=0; i<MUX_MAX*ADC_SING_LENGTH; i++)
	{
		pos = order3[i];
		adc_rela[pos] = raw3[i];
	}
#endif

	if(movingaverage_enabled)
	{
		switch(bitshift)
		{
			case 1: // 2^1 = 2 samples moving average
				if(dump_enabled)
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
						swap_vec32[i] = __rev16(rela32); // SIMD hton
					}
				else // !dump_enabled
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
					}
				break;
			case 2: // 2^2 = 4 samples moving average
				if(dump_enabled)
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
						swap_vec32[i] = __rev16(rela32); // SIMD hton
					}
				else // !dump_enabled
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
					}
				break;
			case 3: // 2^3 = 8 samples moving average
				if(dump_enabled)
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
						swap_vec32[i] = __rev16(rela32); // SIMD hton
					}
				else // !dump_enabled
					for(i=0; i<SENSOR_N/2; i++)
					{
						uint32_t rela32;
						rela32 = __ssub16(rela_vec32[i], qui_vec32[i]); // rela -= qui

						sum_vec32[i] = __sadd16(sum_vec32[i], rela32); // sum += rela
						rela32 = __shadd16(sum_vec32[i], zero); // rela = sum / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						rela32 = __shadd16(rela32, zero); // rela = rela / 2
						sum_vec32[i] = __ssub16(sum_vec32[i], rela32); // sum -= rela

						rela_vec32[i] = rela32;
					}
				break;
		}
	}
	else // !movingaverage_enabled
	{
		if(dump_enabled)
			for(i=0; i<SENSOR_N/2; i++)
			{
				rela_vec32[i] = __ssub16(rela_vec32[i], qui_vec32[i]); // SIMD sub
				swap_vec32[i] = __rev16(rela_vec32[i]); // SIMD hton
			}
		else // !dump_enabled
			for(i=0; i<SENSOR_N/2; i++)
			{
				rela_vec32[i] = __ssub16(rela_vec32[i], qui_vec32[i]); // SIMD sub
			}
	}
}

void
loop()
{
	uint_fast8_t cmc_stat;
	uint_fast8_t cmc_job = 0;
	uint_fast16_t cmc_len = 0;
	uint_fast16_t len = 0;

	uint_fast8_t first = 1;
	OSC_Timetag offset;

//#define OSCTEST
#ifdef OSCTEST
	osc_data_t *bndl;
	osc_data_t *itm;
	osc_data_t *buf_ptr;
	
	buf_ptr = BUF_O_OFFSET(!buf_o_ptr);
	buf_ptr = osc_start_bundle(buf_ptr, OSC_IMMEDIATE, &bndl);
		buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
			buf_ptr = osc_set_path(buf_ptr, "/osc");
			buf_ptr = osc_set_fmt(buf_ptr, "b");
			buf_ptr = osc_set_blob(buf_ptr, 1024, adc12_raw);
		buf_ptr = osc_end_bundle_item(buf_ptr, itm);
	buf_ptr = osc_end_bundle(buf_ptr, bndl);
	cmc_len = buf_ptr - BUF_O_OFFSET(!buf_o_ptr);

	while(1)
	{
		osc_send_nonblocking(&config.output.osc, BUF_O_BASE(!buf_o_ptr), cmc_len);

		buf_ptr = BUF_O_OFFSET(buf_o_ptr);
		buf_ptr = osc_start_bundle(buf_ptr, OSC_IMMEDIATE, &bndl);
			buf_ptr = osc_start_bundle_item(buf_ptr, &itm);
				buf_ptr = osc_set_path(buf_ptr, "/osc");
				buf_ptr = osc_set_fmt(buf_ptr, "b");
				buf_ptr = osc_set_blob(buf_ptr, 1024, adc12_raw);
			buf_ptr = osc_end_bundle_item(buf_ptr, itm);
		buf_ptr = osc_end_bundle(buf_ptr, bndl);
		cmc_len = buf_ptr - BUF_O_OFFSET(buf_o_ptr);

		osc_send_block(&config.output.osc);
		buf_o_ptr ^= 1;
	}
#endif

//#define BENCHMARK
#ifdef BENCHMARK
	Stop_Watch sw_adc_fill = {.id = "adc_fill", .thresh=2000};
	Stop_Watch sw_output_send = {.id = "output_send", .thresh=2000};
	Stop_Watch sw_tuio_process = {.id = "tuio_process", .thresh=2000};
	Stop_Watch sw_output_serialize = {.id = "output_serialize", .thresh=2000};
	Stop_Watch sw_output_block = {.id = "output_block", .thresh=2000};
#endif // BENCHMARK

	while(1) // endless loop
	{
		if(config.sensors.rate)
		{
			adc_time_up = 0;
			timer_resume(adc_timer);
		}

		adc_dma_run();

		if(first) // in the first round there is no data
		{
			adc_dma_block();
			first = 0;
			continue;
		}

		if(calibrating)
			range_calibrate(adc12_raw[adc_raw_ptr], adc3_raw[adc_raw_ptr], order12, order3, adc_sum, adc_rela);

		if(config.output.osc.socket.enabled && (wiz_socket_state[SOCK_OUTPUT] == WIZ_SOCKET_STATE_OPEN) )
		{
			uint_fast8_t job = 0;

#ifdef BENCHMARK
			stop_watch_start(&sw_output_send);
#endif
			if(config.output.parallel && cmc_job) // start nonblocking sending of last cycles output
				cmc_stat = osc_send_nonblocking(&config.output.osc, BUF_O_BASE(!buf_o_ptr), cmc_len);

		// fill adc_rela
#ifdef BENCHMARK
			stop_watch_start(&sw_adc_fill);
#endif
			adc_fill(adc_raw_ptr);

			if(config.sntp.socket.enabled)
				sntp_timestamp_refresh(systick_uptime(), &now, &offset);
			else if(config.ptp.event.enabled)
				ptp_timestamp_refresh(ptp_uptime(), &now, &offset);

			// initiate OSC bundle
			osc_data_t *buf = BUF_O_OFFSET(buf_o_ptr);
			osc_data_t *buf_ptr = buf;
			osc_data_t *preamble;
			osc_data_t *bndl;
			if(config.output.osc.mode == OSC_MODE_TCP)
				buf_ptr = osc_start_bundle_item(buf_ptr, &preamble);
			if(cmc_engines_active + config.dump.enabled > 1)
				buf_ptr = osc_start_bundle(buf_ptr, OSC_IMMEDIATE, &bndl); // node bundle

			if(config.dump.enabled) // dump output is functional even when calibrating
				buf_ptr = dump_update(buf_ptr, now, offset);
		
			if(!calibrating && cmc_engines_active) // output engines are disfunctional when calibrating
			{
#ifdef BENCHMARK
				stop_watch_start(&sw_tuio_process);
#endif
				buf_ptr = cmc_process(now, offset, adc_rela, engines, buf_ptr); // touch recognition of current cycle
			}
			
			if(cmc_engines_active + config.dump.enabled > 1)
				buf_ptr = osc_end_bundle(buf_ptr, bndl); // node bundle
			if(config.output.osc.mode == OSC_MODE_TCP)
				buf_ptr = osc_end_bundle_item(buf_ptr, preamble);

			cmc_len = buf_ptr - buf;
			if(cmc_len > 0) // is there anything after OSC bundle header?
			{
				job = 1;

				if(config.output.osc.mode == OSC_MODE_SLIP)
					cmc_len = slip_encode(buf, cmc_len);
			}
			else // cmc_len <= 16
				job = 0;

#ifdef BENCHMARK
			stop_watch_start(&sw_output_block);
#endif
			if(config.output.parallel && cmc_job && cmc_stat) // block for end of sending of last cycles output
				osc_send_block(&config.output.osc);

			if(job) // switch output buffer
				buf_o_ptr ^= 1;

			cmc_job = job;

			// serial output processing
			if(!config.output.parallel && cmc_job) // start blocking sending of this cycles output
			{
				osc_send(&config.output.osc, BUF_O_BASE(!buf_o_ptr), cmc_len);
				cmc_job = 0;
			}

#ifdef BENCHMARK
			stop_watch_stop(&sw_output_send);
			stop_watch_stop(&sw_adc_fill);
			stop_watch_stop(&sw_tuio_process);
			stop_watch_stop(&sw_output_serialize);
			stop_watch_stop(&sw_output_block);
#endif
		}

		// handle WIZnet IRQs XXX check manually if we should have missed an interrupt
		if(wiz_needs_attention || (pin_read_bit(UDP_INT) == 0) )
		{
			// as long as interrupt pin is low, handle interrupts
			while(pin_read_bit(UDP_INT) == 0)
				wiz_irq_handle();
			wiz_needs_attention = 0;
		}

		// run osc config server
		if(config_should_listen)
		{
			if(config_should_listen & WIZ_Sn_IR_CON) // TCP only
			{
				wiz_socket_state[SOCK_CONFIG] = WIZ_SOCKET_STATE_OPEN;
				udp_update_read_write_pointers(SOCK_CONFIG);
				debug_str("config connect");
			}
			if( (config_should_listen & WIZ_Sn_IR_TIMEOUT) || (config_should_listen & WIZ_Sn_IR_DISCON) )
			{
				uint8_t enabled = config.config.osc.socket.enabled;
				config_enable(0);
				if(config.config.osc.mode && enabled)
					config_enable(1);
				debug_str("config ARPto or TCP disconect");
			}
			else if( (config_should_listen & WIZ_Sn_IR_RECV) && (wiz_socket_state[SOCK_CONFIG] == WIZ_SOCKET_STATE_OPEN) )
				osc_dispatch(&config.config.osc, BUF_I_BASE(buf_i_ptr), config_cb);
			config_should_listen = 0;
		}

		if(output_should_listen)
		{
			if(output_should_listen & WIZ_Sn_IR_CON) // TCP only
			{
				wiz_socket_state[SOCK_OUTPUT] = WIZ_SOCKET_STATE_OPEN;
				udp_update_read_write_pointers(SOCK_OUTPUT);
				debug_str("output connect");
			}
			if( (output_should_listen & WIZ_Sn_IR_TIMEOUT) || (output_should_listen & WIZ_Sn_IR_DISCON) )
			{
				output_enable(0);
				debug_str("output ARPto or TCP disconect");
			}
			else if( (output_should_listen & WIZ_Sn_IR_RECV) && (wiz_socket_state[SOCK_OUTPUT] == WIZ_SOCKET_STATE_OPEN) )
				osc_ignore(config.output.osc.socket.sock);
			output_should_listen = 0;
		}

		if(debug_should_listen)
		{
			if(debug_should_listen & WIZ_Sn_IR_CON) // TCP only
			{
				wiz_socket_state[SOCK_DEBUG] = WIZ_SOCKET_STATE_OPEN;
				udp_update_read_write_pointers(SOCK_DEBUG);
			}
			if( (debug_should_listen & WIZ_Sn_IR_TIMEOUT) || (debug_should_listen & WIZ_Sn_IR_DISCON) ) {
				debug_enable(0);
			}
			else if( (debug_should_listen & WIZ_Sn_IR_RECV) && (wiz_socket_state[SOCK_DEBUG] == WIZ_SOCKET_STATE_OPEN) )
				osc_ignore(config.debug.osc.socket.sock);
			debug_should_listen = 0;
		}
		
		// run sntp client
		if(config.sntp.socket.enabled)
		{
			if(sntp_should_listen & WIZ_Sn_IR_TIMEOUT)
			{
				sntp_enable(0);
				debug_str("sntp ARPto");
			}
			// listen for sntp request answer
			else if(sntp_should_listen & WIZ_Sn_IR_RECV)
			{
				udp_dispatch(config.sntp.socket.sock, BUF_I_BASE(buf_i_ptr), sntp_cb);
				sntp_should_listen = 0;
			}

			// send sntp request
			if(sync_should_request)
			{
				sntp_timestamp_refresh(systick_uptime(), &now, NULL);
				len = sntp_request(BUF_O_OFFSET(buf_o_ptr), now);
				udp_send(config.sntp.socket.sock, BUF_O_BASE(buf_o_ptr), len);
				sync_should_request = 0;
			}
		}

		// run ptp client
		if(config.ptp.event.enabled)
		{
			if(ptp_event_should_listen & WIZ_Sn_IR_RECV)
			{
				udp_dispatch(config.ptp.event.sock, BUF_I_BASE(buf_i_ptr), ptp_cb);
				ptp_event_should_listen = 0;
			}

			if(ptp_general_should_listen & WIZ_Sn_IR_RECV)
			{
				udp_dispatch(config.ptp.general.sock, BUF_I_BASE(buf_i_ptr), ptp_cb);
				ptp_general_should_listen = 0;
			}

			if(ptp_should_request)
			{
				ptp_request();
				ptp_should_request = 0;
			}
		}

		// run ZEROCONF server
		if(config.mdns.socket.enabled)
		{
			// ARPto does not exist for multicast connections
			if(mdns_should_listen & WIZ_Sn_IR_RECV)
			{
				udp_dispatch(config.mdns.socket.sock, BUF_I_BASE(buf_i_ptr), mdns_cb);
				mdns_should_listen = 0;
			}

			if(mdns_timeout)
			{
				mdns_resolve_timeout();
				mdns_timeout = 0;
			}
		}

		// DHCPC REFRESH
		if(dhcpc_needs_refresh)
		{
			timer_pause(dhcpc_timer);
			dhcpc_enable(1);
			dhcpc_refresh();
			dhcpc_enable(0);
			dhcpc_needs_refresh = 0;
		}

		//FIXME asio
		/*
		if(config.dhcpc.socket.enabled && dhcpc_should_listen)
		{
			if(dhcpc_should_listen & WIZ_Sn_IR_TIMEOUT)
			{
				dhcpc_enable(0);
				debug_str("dhcpc ARPto");
			}
			else if(dhcpc_should_listen & WIZ_Sn_IR_RECV)
				udp_ignore(config.dhcpc.socket.sock);
			dhcpc_should_listen = 0;
		}
		*/

		adc_dma_block();

		if(config.sensors.rate)
			while(!adc_time_up)
				;
	} // endless loop
}

void
adc_timer_reconfigure()
{
	// this scheme is good for rates in the range of 20-2000+
	uint16_t prescaler = 50-1;
	uint16_t reload = 72e6 / (config.sensors.rate * 50);
	uint16_t compare = reload;

	timer_set_prescaler(adc_timer, prescaler);
	timer_set_reload(adc_timer, reload);
	timer_set_mode(adc_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare(adc_timer, TIMER_CH1, compare);
	timer_attach_interrupt(adc_timer, TIMER_CH1, adc_timer_irq);
	timer_generate_update(adc_timer);

	nvic_irq_set_priority(NVIC_TIMER1_CC, ADC_TIMER_PRIORITY);
}

void 
sync_timer_reconfigure()
{
	uint16_t prescaler = 0xffff; 
	uint16_t reload = 72e6 / 0xffff * config.sntp.tau;
	uint16_t compare = reload;

	timer_set_prescaler(sync_timer, prescaler);
	timer_set_reload(sync_timer, reload);
	timer_set_mode(sync_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare(sync_timer, TIMER_CH1, compare);
	timer_attach_interrupt(sync_timer, TIMER_CH1, sync_timer_irq);
	timer_generate_update(sync_timer);

	nvic_irq_set_priority(NVIC_TIMER2, SYNC_TIMER_PRIORITY);
}

void 
ptp_timer_reconfigure(float sec)
{
	uint16_t prescaler = 0xffff; 
	uint16_t reload = 72e6 / 0xffff * sec;
	uint16_t compare = reload;

	timer_set_prescaler(ptp_timer, prescaler);
	timer_set_reload(ptp_timer, reload);
	timer_set_mode(ptp_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare(ptp_timer, TIMER_CH1, compare);
	timer_attach_interrupt(ptp_timer, TIMER_CH1, ptp_timer_irq);
	timer_generate_update(ptp_timer);

	nvic_irq_set_priority(NVIC_TIMER1_BRK_TIMER15, SYNC_TIMER_PRIORITY);
}

void 
dhcpc_timer_reconfigure()
{
	uint16_t prescaler = 0xffff; 
	uint16_t reload = 72e6 / 0xffff * dhcpc.leastime;
	uint16_t compare = reload;

	timer_set_prescaler(dhcpc_timer, prescaler);
	timer_set_reload(dhcpc_timer, reload);
	timer_set_mode(dhcpc_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare(dhcpc_timer, TIMER_CH1, compare);
	timer_attach_interrupt(dhcpc_timer, TIMER_CH1, dhcpc_timer_irq);
	timer_generate_update(dhcpc_timer);

	nvic_irq_set_priority(NVIC_TIMER4, DHCPC_TIMER_PRIORITY);
}

void 
mdns_timer_reconfigure()
{
	uint16_t prescaler = 0xffff; 
	uint16_t reload = 72e6 / 0xffff * 2; // timeout after 2 seconds
	uint16_t compare = reload;

	timer_set_prescaler(mdns_timer, prescaler);
	timer_set_reload(mdns_timer, reload);
	timer_set_mode(mdns_timer, TIMER_CH1, TIMER_OUTPUT_COMPARE);
	timer_set_compare(mdns_timer, TIMER_CH1, compare);
	timer_attach_interrupt(mdns_timer, TIMER_CH1, mdns_timer_irq);
	timer_generate_update(mdns_timer);

	nvic_irq_set_priority(NVIC_TIMER3, MDNS_TIMER_PRIORITY);
}

void
setup()
{
	uint_fast8_t i;
	uint_fast8_t p;

	// determine power vs factory reset
	bkp_init();

	// get reset mode
	Reset_Mode reset_mode = bkp_read(RESET_MODE_REG);
	
	// set hard reset mode by default for next boot
	bkp_enable_writes();
	bkp_write(RESET_MODE_REG, RESET_MODE_FLASH_HARD);
	bkp_disable_writes();

	switch(reset_mode)
	{
		case RESET_MODE_FLASH_SOFT:
			// fall through
		case RESET_MODE_FLASH_HARD:
			syscfg_set_mem_mode(SYSCFG_MEM_MODE_FLASH);
			break;
		case RESET_MODE_SYSTEM_FLASH:
			syscfg_set_mem_mode(SYSCFG_MEM_MODE_SYSTEM_FLASH);
			// jump to system memory, aka DfuSe boot loader
			asm volatile(
				"\tLDR		R0, =0x1FFFD800\n"
				"\tLDR		SP, [R0, #0]\n"
				"\tLDR		R0, [R0, #4]\n"
				"\tBX			R0\n");
			break; // never reached
	}

	// by pressing FLASH button before RESET button will trigger a soft reset
	pin_set_modef(SOFT_RESET, GPIO_MODE_INPUT, GPIO_MODEF_PUPD_NONE);
	exti_attach_interrupt((exti_num)(PIN_MAP[SOFT_RESET].gpio_bit),
		gpio_exti_port(PIN_MAP[SOFT_RESET].gpio_device),
		soft_irq,
		EXTI_RISING);

	pin_set_modef(CHIM_LED_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(CHIM_LED_PIN, 0);

	// setup pins to switch the muxes
	for(i=0; i<MUX_LENGTH; i++)
		pin_set_modef(mux_sequence[i], GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);

#if REVISION == 4
	// initialize counter
	pin_write_bit(mux_sequence[0], 1); // MR
	pin_write_bit(mux_sequence[1], 0); // CP
#endif

	// setup analog input pins
	for(i=0; i<ADC_DUAL_LENGTH; i++)
	{
		pin_set_modef(adc1_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);
		pin_set_modef(adc2_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);
	}
	for(i=0; i<ADC_SING_LENGTH; i++)
		pin_set_modef(adc3_sequence[i], GPIO_MODE_ANALOG, GPIO_MODEF_PUPD_NONE);
	for(i=0; i<ADC_UNUSED_LENGTH; i++)
		pin_set_modef(adc_unused[i], GPIO_MODE_INPUT, GPIO_MODEF_PUPD_PD); // pull-down unused analog ins

	// SPI for W5200/W5500
	spi_init(WIZ_SPI_DEV);
	spi_data_size(WIZ_SPI_DEV, SPI_DATA_SIZE_8_BIT);
	spi_master_enable(WIZ_SPI_DEV, SPI_CR1_BR_PCLK_DIV_2, SPI_MODE_0,
													SPI_CR1_BIDIMODE_2_LINE | SPI_FRAME_MSB | SPI_CR1_SSM | SPI_CR1_SSI);
	spi_config_gpios(WIZ_SPI_DEV, 1,
		PIN_MAP[WIZ_SPI_NSS_PIN].gpio_device, PIN_MAP[WIZ_SPI_NSS_PIN].gpio_bit,
		PIN_MAP[WIZ_SPI_SCK_PIN].gpio_device, PIN_MAP[WIZ_SPI_SCK_PIN].gpio_bit, PIN_MAP[WIZ_SPI_MISO_PIN].gpio_bit, PIN_MAP[WIZ_SPI_MOSI_PIN].gpio_bit);
	pin_set_af(WIZ_SPI_NSS_PIN, GPIO_AF_0); // we want to handle NSS by software
	pin_set_modef(WIZ_SPI_NSS_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP); // we want to handle NSS by software
	pin_write_bit(WIZ_SPI_NSS_PIN, 1);

	pin_set_modef(UDP_SS, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(UDP_SS, 1);

#if WIZ_CHIP == 5200 //TODO move everything to wiz_init
	pin_set_modef(UDP_PWDN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(UDP_PWDN, 0);
#endif

	pin_set_modef(UDP_INT, GPIO_MODE_INPUT, GPIO_PUPDR_NOPUPD);
	exti_attach_interrupt((exti_num)(PIN_MAP[UDP_INT].gpio_bit),
		gpio_exti_port(PIN_MAP[UDP_INT].gpio_device),
		wiz_irq,
		EXTI_FALLING);

	// systick 
	systick_disable();
	systick_init(SNTP_SYSTICK_RELOAD_VAL);

	nvic_irq_set_priority(NVIC_SYSTICK, 0x0); // needs highest priority
	nvic_irq_set_priority((exti_num)(PIN_MAP[UDP_INT].gpio_bit), 0x1); // needs second highest priority

	// initialize random number generator based on UID96
	uint32_t seed = uid_seed();
	srand(seed);

	// init eeprom for I2C2
	eeprom_init(EEPROM_DEV);
	eeprom_slave_init(eeprom_24LC64, EEPROM_DEV, 0b000);
	eeprom_slave_init(eeprom_24AA025E48, EEPROM_DEV, 0b001);

	// load config or use factory settings?
	if(reset_mode == RESET_MODE_FLASH_SOFT)
		config_load(); // soft reset: load configuration from EEPROM

	// rebuild engines stack
	cmc_engines_update();

	// read MAC from MAC EEPROM or use custom one stored in config
	if(!config.comm.custom_mac)
		eeprom_bulk_read(eeprom_24AA025E48, 0xfa, config.comm.mac, 6);

	// load calibrated sensor ranges from eeprom
	range_load(0);

	// init DMA, which is used for SPI and ADC
	dma_init(DMA1);
	dma_init(DMA2);

	// initialize WIZnet W5200/W5500
	uint8_t tx_mem[WIZ_MAX_SOCK_NUM] = {
		[SOCK_DHCPC]	= 1, // = SOCK_ARP
		[SOCK_SNTP]		= 1,
		[SOCK_PTP_EV] = 1,
		[SOCK_PTP_GE] = 1,
		[SOCK_OUTPUT]	= 8,
		[SOCK_CONFIG]	= 2,
		[SOCK_DEBUG]	= 1,
		[SOCK_MDNS]		= 1,
	};
	uint8_t rx_mem[WIZ_MAX_SOCK_NUM] = {
		[SOCK_DHCPC]	= 1, // = SOCK_ARP
		[SOCK_SNTP]		= 1,
		[SOCK_PTP_EV] = 1,
		[SOCK_PTP_GE] = 1,
		[SOCK_OUTPUT]	= 8,
		[SOCK_CONFIG]	= 2,
		[SOCK_DEBUG]	= 1,
		[SOCK_MDNS]		= 1,
	};
	wiz_init(PIN_MAP[UDP_SS].gpio_device, PIN_MAP[UDP_SS].gpio_bit, tx_mem, rx_mem);
	wiz_mac_set(config.comm.mac);

	// wait for link up before proceeding
	while(!wiz_link_up()) // TODO monitor this and go to sleep mode when link is down
		;

	wiz_init(PIN_MAP[UDP_SS].gpio_device, PIN_MAP[UDP_SS].gpio_bit, tx_mem, rx_mem); //TODO solve this differently
	
	// choose DHCP, IPv4LL or static IP
	uint_fast8_t claimed = 0;
	if(config.dhcpc.socket.enabled)
	{
		dhcpc_enable(1);
		claimed = dhcpc_claim(config.comm.ip, config.comm.gateway, config.comm.subnet);
		dhcpc_enable(0); // disable socket again
	}
	if(!claimed && config.ipv4ll.enabled)
		IPv4LL_claim(config.comm.ip, config.comm.gateway, config.comm.subnet);

	wiz_comm_set(config.comm.mac, config.comm.ip, config.comm.gateway, config.comm.subnet);

	//TODO put this into config_enable?
	const uint8_t wiz_udp_multicast_irq_mask = WIZ_Sn_IR_RECV;
	const uint8_t wiz_udp_irq_mask = wiz_udp_multicast_irq_mask | WIZ_Sn_IR_TIMEOUT;
	const uint8_t wiz_tcp_irq_mask = wiz_udp_irq_mask | WIZ_Sn_IR_CON | WIZ_Sn_IR_DISCON;
	wiz_socket_irq_set(SOCK_SNTP, wiz_sntp_irq, wiz_udp_irq_mask);
	wiz_socket_irq_set(SOCK_PTP_EV, wiz_ptp_event_irq, wiz_udp_multicast_irq_mask);
	wiz_socket_irq_set(SOCK_PTP_GE, wiz_ptp_general_irq, wiz_udp_multicast_irq_mask);
	wiz_socket_irq_set(SOCK_OUTPUT, wiz_output_irq, wiz_tcp_irq_mask);
	wiz_socket_irq_set(SOCK_CONFIG, wiz_config_irq, wiz_tcp_irq_mask);
	wiz_socket_irq_set(SOCK_DEBUG, wiz_debug_irq, wiz_tcp_irq_mask);
	wiz_socket_irq_set(SOCK_MDNS, wiz_mdns_irq, wiz_udp_multicast_irq_mask);
	//wiz_socket_irq_set(SOCK_DHCPC, wiz_dhcpc_irq, wiz_udp_irq_mask); FIXME asio
	wiz_socket_irq_unset(SOCK_DHCPC); // = SOCK_ARP

	// initialize timers TODO move up
	timer_init(adc_timer);
	timer_pause(adc_timer);
	adc_timer_reconfigure();

	timer_init(sync_timer);

	timer_init(dhcpc_timer);
	timer_pause(dhcpc_timer);

	timer_init(mdns_timer);
	timer_pause(mdns_timer);
	
	timer_init(ptp_timer);
	timer_pause(ptp_timer);

	// initialize sockets
	output_enable(config.output.osc.socket.enabled);
	config_enable(config.config.osc.socket.enabled);
	sntp_enable(config.sntp.socket.enabled);
	ptp_enable(config.ptp.event.enabled);
	debug_enable(config.debug.osc.socket.enabled);
	mdns_enable(config.mdns.socket.enabled);
	
	if(config.mdns.socket.enabled)
		mdns_announce(); // announce new IP

	// set up ADCs
	adc_disable(ADC1);
	adc_disable(ADC2);
	adc_disable(ADC3);
	adc_disable(ADC4);

	adc_set_prescaler(ADC_PRE_PCLK_DIV_1);

	adc_set_exttrig(ADC1, ADC_EXTTRIG_MODE_SOFTWARE);
	adc_set_exttrig(ADC2, ADC_EXTTRIG_MODE_SOFTWARE);
	adc_set_exttrig(ADC3, ADC_EXTTRIG_MODE_SOFTWARE);

	adc_set_sample_rate(ADC1, ADC_SMPR_181_5);
	adc_set_sample_rate(ADC2, ADC_SMPR_181_5);
	adc_set_sample_rate(ADC3, ADC_SMPR_181_5);

	// fill raw sequence array with corresponding ADC channels
	for(i=0; i<ADC_DUAL_LENGTH; i++)
	{
		adc1_raw_sequence[i] = PIN_MAP[adc1_sequence[i]].adc_channel;
		adc2_raw_sequence[i] = PIN_MAP[adc2_sequence[i]].adc_channel;
	}
	for(i=0; i<ADC_SING_LENGTH; i++)
		adc3_raw_sequence[i] = PIN_MAP[adc3_sequence[i]].adc_channel;

	for(p=0; p<MUX_MAX; p++)
		for(i=0; i<ADC_DUAL_LENGTH*2; i++)
			order12[p*ADC_DUAL_LENGTH*2 + i] = mux_order[p] + adc_order[i]*MUX_MAX;

	for(p=0; p<MUX_MAX; p++)
		for(i=0; i<ADC_SING_LENGTH; i++)
			order3[p*ADC_SING_LENGTH + i] = mux_order[p] + adc_order[ADC_DUAL_LENGTH*2+i]*MUX_MAX;

	// set up ADC DMA tubes
	int status;

	// set channels in register
#if(ADC_DUAL_LENGTH > 0)
	adc_set_conv_seq(ADC1, adc1_raw_sequence, ADC_DUAL_LENGTH);
	adc_set_conv_seq(ADC2, adc2_raw_sequence, ADC_DUAL_LENGTH);

	ADC1->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	nvic_irq_enable(NVIC_ADC1_2);
	//adc_attach_interrupt(ADC1, ADC_IER_EOS, adc1_2_irq, NULL);

	ADC12_BASE->CCR |= ADC_MDMA_MODE_ENABLE_12_10_BIT; // enable ADC DMA in 12-bit dual mode
	ADC12_BASE->CCR |= ADC_CCR_DMACFG; // enable ADC circular mode for use with DMA
	ADC12_BASE->CCR |= ADC_MODE_DUAL_REGULAR_ONLY; // set DMA dual mode to regular channels only

	adc_enable(ADC1);
	adc_enable(ADC2);

	// set up DMA tube
	adc_tube12.tube_dst =(void *)adc12_raw;
	status = dma_tube_cfg(DMA1, DMA_CH1, &adc_tube12);
	ASSERT(status == DMA_TUBE_CFG_SUCCESS);

	dma_set_priority(DMA1, DMA_CH1, DMA_PRIORITY_MEDIUM);    //Optional
	dma_set_num_transfers(DMA1, DMA_CH1, ADC_DUAL_LENGTH*MUX_MAX*2);
	dma_attach_interrupt(DMA1, DMA_CH1, adc12_dma_irq);
	dma_enable(DMA1, DMA_CH1);                //CCR1 EN bit 0
	nvic_irq_set_priority(NVIC_DMA_CH1, ADC_DMA_PRIORITY);
#endif

#if(ADC_SING_LENGTH > 0)
	adc_set_conv_seq(ADC3, adc3_raw_sequence, ADC_SING_LENGTH);

	ADC3->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	nvic_irq_enable(NVIC_ADC3);
	//adc_attach_interrupt(ADC3, ADC_IER_EOS, adc3_irq, NULL);

	ADC3->regs->CFGR |= ADC_CFGR_DMAEN; // enable DMA request
	ADC3->regs->CFGR |= ADC_CFGR_DMACFG, // enable ADC circular mode for use with DMA

	adc_enable(ADC3);

	// set up DMA tube
	adc_tube3.tube_dst =(void *)adc3_raw;
	status = dma_tube_cfg(DMA2, DMA_CH5, &adc_tube3);
	ASSERT(status == DMA_TUBE_CFG_SUCCESS);

	dma_set_priority(DMA2, DMA_CH5, DMA_PRIORITY_MEDIUM);
	dma_set_num_transfers(DMA2, DMA_CH5, ADC_SING_LENGTH*MUX_MAX*2);
	dma_attach_interrupt(DMA2, DMA_CH5, adc3_dma_irq);
	dma_enable(DMA2, DMA_CH5);
	nvic_irq_set_priority(NVIC_DMA_CH5, ADC_DMA_PRIORITY);
#endif

	// set up continuous music controller output engines
	cmc_init();
	dump_init(sizeof(adc_swap), adc_swap);
	dummy_init();
	oscmidi_init();
	scsynth_init();
	tuio2_init();
	tuio1_init();
	custom_init();

	pin_write_bit(CHIM_LED_PIN, 1);
	DEBUG("si", "reset_mode", reset_mode);
	DEBUG("si", "config_size", sizeof(Config));
}

void
main()
{
	cpp_setup();
  setup();
	loop();
}
