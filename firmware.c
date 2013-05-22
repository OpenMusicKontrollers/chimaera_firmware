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
//#include <libmaple/i2c.h> // I2C for eeprom
#include <libmaple/spi.h> // SPI for w5200
#include <libmaple/adc.h> // analog to digital converter
#include <libmaple/dma.h> // direct memory access
#include <libmaple/bkp.h> // backup register

/*
 * include chimaera custom libraries
 */
#include <chimaera.h>
#include <chimutil.h>
//#include <eeprom.h>
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
#include <wiz.h>

uint8_t mux_sequence [MUX_LENGTH] = {PB5, PB4, PB3, PA15}; // digital out pins to switch MUX channels

uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {PA1, PA2, PA0, PA3}; // analog input pins read out by the ADC1
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {PA4, PA5, PA6, PA7}; // analog input pins read out by the ADC2
uint8_t adc3_sequence [ADC_SING_LENGTH] = {PB0}; // analog input pins read out by the ADC3

uint8_t adc1_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc2_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc3_raw_sequence [ADC_SING_LENGTH];

int16_t adc12_raw[2][MUX_MAX][ADC_DUAL_LENGTH*2] __attribute__((aligned(4))); // the dma temporary data array.
int16_t adc3_raw[2][MUX_MAX][ADC_SING_LENGTH] __attribute__((aligned(4)));
int16_t adc_sum[SENSOR_N];
int16_t adc_rela[SENSOR_N];
int16_t adc_swap[SENSOR_N];

uint8_t mux_order [MUX_MAX] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };
uint8_t adc_order [ADC_LENGTH] = { 8, 4, 7, 3, 6, 2, 5, 1, 0 };
uint8_t order12 [MUX_MAX][ADC_DUAL_LENGTH*2];
uint8_t order3 [MUX_MAX][ADC_SING_LENGTH];

volatile uint8_t adc12_dma_done = 0;
volatile uint8_t adc12_dma_err = 0;
volatile uint8_t adc3_dma_done = 0;
volatile uint8_t adc3_dma_err = 0;
volatile uint8_t adc_time_up = 1;
volatile uint8_t adc_raw_ptr = 1;
volatile uint8_t mux_counter = MUX_MAX;
volatile uint8_t sntp_should_request = 0;
volatile uint8_t sntp_should_listen = 0;
volatile uint8_t mdns_should_listen = 0;
volatile uint8_t config_should_listen = 0;

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
{
	ADC1_BASE->ISR |= ADC_ISR_EOS;

	pin_write_bit (mux_sequence[0], mux_counter & 0b0001);
	pin_write_bit (mux_sequence[1], mux_counter & 0b0010);
	pin_write_bit (mux_sequence[2], mux_counter & 0b0100);
	pin_write_bit (mux_sequence[3], mux_counter & 0b1000);

	if (mux_counter < MUX_MAX)
	{
		mux_counter++;
		ADC1->regs->CR |= ADC_CR_ADSTART; // start master(ADC1) and slave(ADC2) conversion
		ADC3->regs->CR |= ADC_CR_ADSTART;
	}
}

void
__irq_adc3 ()
{
	ADC3_BASE->ISR |= ADC_ISR_EOS;
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

#define adc_dma_run \
	adc12_dma_done = 0; \
	adc3_dma_done = 0; \
	mux_counter = 0; \
	__irq_adc1_2 ();

#define adc_dma_block \
	while (!adc12_dma_done) \
		; \
	adc_raw_ptr ^= 1;

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
		uint8_t i;
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
		uint8_t i;
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

void
loop ()
{
	nOSC_Item nest_bndl [ENGINE_MAX];
	char nest_fmt [ENGINE_MAX+1];

	uint8_t send_status = 0;
	uint8_t cmc_job = 0;
	uint16_t cmc_len = 0;
	uint16_t len = 0;

	uint8_t first = 1;
	nOSC_Timestamp offset;

#ifdef BENCHMARK
	Stop_Watch sw_adc_fill = {.id = "adc_fill", .thresh=2000};

	Stop_Watch sw_dump_update = {.id = "dump_update", .thresh=2000};
	Stop_Watch sw_dump_serialize = {.id = "dump_serialize", .thresh=2000};
	Stop_Watch sw_dump_send = {.id = "dump_send", .thresh=2000};

	Stop_Watch sw_tuio_process = {.id = "tuio_process", .thresh=2000};
	Stop_Watch sw_tuio_serialize = {.id = "tuio_serialize", .thresh=2000};
	Stop_Watch sw_tuio_send = {.id = "tuio_send", .thresh=2000};
#endif // BENCHMARK

	while (1) // endless loop
	{
		if (config.rate)
		{
			adc_time_up = 0;
			timer_resume (adc_timer);
		}

		adc_dma_run;

		if (first) // in the first round there is no data
		{
			adc_dma_block;
			first = 0;
			continue;
		}

		// fill adc_rela
#ifdef BENCHMARK
		stop_watch_start (&sw_adc_fill);
#endif // BENCHMARK
		adc_fill (adc12_raw[adc_raw_ptr], adc3_raw[adc_raw_ptr], order12, order3, adc_sum, adc_rela, adc_swap, !calibrating); // 49us (rela only), 69us (rela & swap), 71us (movingaverage)
#ifdef BENCHMARK
		stop_watch_stop (&sw_adc_fill);
#endif // BENCHMARK

		if (calibrating)
			range_calibrate (adc_rela);

		if (!calibrating && config.output.enabled)
		{
			uint8_t job = 0;

			if (cmc_job) // start nonblocking sending of last cycles tuio output
				send_status = udp_send_nonblocking (config.output.socket.sock, !buf_o_ptr, cmc_len);

			sntp_timestamp_refresh (&now, &offset);

			if (config.dump.enabled)
			{
				dump_update (now); // 6us
				nosc_item_bundle_set (nest_bndl, job++, dump_bndl, offset, dump_fmt);
			}
		
			if (config.tuio.enabled || config.scsynth.enabled || config.oscmidi.enabled || config.rtpmidi.enabled) // put all blob based engine flags here, e.g. TUIO, RTPMIDI, Kraken, SuperCollider, ...
			{
				uint8_t blobs = cmc_process (now, adc_rela, engines); // touch recognition of current cycle

				if (blobs) // was there any update?
				{
					if (config.tuio.enabled)
						nosc_item_bundle_set (nest_bndl, job++, tuio2_bndl, offset, tuio2_fmt);

					if (config.scsynth.enabled)
						nosc_item_bundle_set (nest_bndl, job++, scsynth_bndl, offset, scsynth_fmt);

					if (config.oscmidi.enabled)
						nosc_item_bundle_set (nest_bndl, job++, oscmidi_bndl, offset, oscmidi_fmt);

					if (config.rtpmidi.enabled) //FIXME we cannot run RTP-MIDI and OSC output at the same time
						cmc_len = rtpmidi_serialize (&buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);

					// if (config.kraken.enabled)
				}
			}

			if (job)
			{
				if (job > 1)
				{
					memset (nest_fmt, nOSC_BUNDLE, job);
					nest_fmt[job] = nOSC_TERM;

					cmc_len = nosc_bundle_serialize (nest_bndl, offset, nest_fmt, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
				}
				else // job == 1, there's no need to send a nested bundle in this case
					cmc_len = nosc_bundle_serialize (nest_bndl[0].bundle.bndl, offset, nest_bndl[0].bundle.fmt, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
			}

			if (config.rtpmidi.enabled && cmc_len) //FIXME we cannot run RTP-MIDI and OSC output at the same time
				job = 1;

			if (cmc_job && send_status) // block for end of sending of last cycles tuio output
				udp_send_block (config.output.socket.sock); //FIXME check return status

			if (job) // switch output buffer
				buf_o_ptr ^= 1;

			cmc_job = job;
		}

		// run osc config server
		if (config.config.enabled && config_should_listen)
		{
			udp_dispatch (config.config.socket.sock, buf_o_ptr, config_cb);
			config_should_listen = 0;
		}
		
		// run sntp client
		if (config.sntp.enabled)
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
		if (config.mdns.enabled && mdns_should_listen)
		{
			udp_dispatch (config.mdns.socket.sock, buf_o_ptr, mdns_cb);
			mdns_should_listen = 0;
		}

		adc_dma_block;

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

/*
static void
udp_irq (void)
{
	//TODO
}
*/

void
setup ()
{
	uint8_t i;
	uint8_t p;

	pin_set_modef (BOARD_BUTTON_PIN, GPIO_MODE_INPUT, GPIO_MODEF_PUPD_NONE);
	pin_set_modef (BOARD_LED_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);

	pin_write_bit (BOARD_LED_PIN, 1);

	//TODO
	//pin_set_mode (UDP_INT, GPIO_INPUT_FLOATING);
	//attachInterrupt (UDP_INT, udp_irq, FALLING);

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

	// systick 
	systick_disable ();
	systick_init ((SYSTICK_RELOAD_VAL + 1)/10 - 1); // systick every 100us = every 7200 cycles on a maple mini @72MHz

	// create EUI-32 and EUI-48 from unique identifier
	eui_init ();

	// initialize random number generator based on EUI_32
	uint32_t *seed = (uint32_t *)&EUI_32[0];
	srand (*seed);

	// init eeprom for I2C1
	//eeprom_init (I2C1);
	//eeprom_slave_init (eeprom_24LC64, I2C1, 0b000);
	//eeprom_slave_init (eeprom_24AA025E48, I2C1, 0b001);

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
	/*
	if (!config.comm.locally)
		eeprom_bulk_read (eeprom_24AA025E48, 0xfa, config.comm.mac, 6);
	*/

	// load calibrated sensor ranges from eeprom
	range_load (config.calibration);

	// init DMA, which is used for SPI and ADC
	dma_init (DMA1);
	dma_init (DMA2);

	// SPI for W5200
	pin_set_modef(UDP_PWDN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(UDP_PWDN, 0);

	pin_set_modef(BOARD_SPI2_NSS_PIN, GPIO_MODE_OUTPUT, GPIO_MODEF_TYPE_PP);
	pin_write_bit(BOARD_SPI2_NSS_PIN, 1);

	pin_set_modef(BOARD_SPI2_SCK_PIN, GPIO_MODE_AF, GPIO_MODEF_TYPE_PP);
	pin_set_modef(BOARD_SPI2_MISO_PIN, GPIO_MODE_AF, GPIO_MODEF_PUPD_NONE);
	pin_set_modef(BOARD_SPI2_MOSI_PIN, GPIO_MODE_AF, GPIO_MODEF_TYPE_PP);

	pin_set_af(BOARD_SPI2_SCK_PIN, GPIO_AF_5);
	pin_set_af(BOARD_SPI2_MISO_PIN, GPIO_AF_5);
	pin_set_af(BOARD_SPI2_MOSI_PIN, GPIO_AF_5);

	spi_init(SPI2); //FIXME add to libmaple F3 port
	SPI2->regs->CR1 |= SPI_CR1_BR_PCLK_DIV_2;
	SPI2->regs->CR1 |= SPI_MODE_0;
	SPI2->regs->CR1 |= SPI_CR1_BIDIMODE_2_LINE;
	SPI2->regs->CR2 |= SPI_DATA_SIZE_8_BIT;
	SPI2->regs->CR1 |= SPI_FRAME_MSB;
	SPI2->regs->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
	SPI2->regs->CR2 |= SPI_CR2_FRXTH;
	SPI2->regs->CR1 |= SPI_CR1_MSTR;
	SPI2->regs->CR1 |= SPI_CR1_SPE;

	// initialize wiz820io
	uint8_t tx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	uint8_t rx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	wiz_init (PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit, tx_mem, rx_mem);
	wiz_mac_set (config.comm.mac);

	// wait for link up before proceeding
	while (!wiz_link_up ()) // TODO monitor this and go to sleep mode when link is down
		;

	wiz_init (PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit, tx_mem, rx_mem); //TODO solve this differently

	uint8_t claimed = 0;
	if (config.dhcpc.enabled)
	{
		dhcpc_enable (config.dhcpc.enabled);
		claimed = dhcpc_claim (config.comm.ip, config.comm.gateway, config.comm.subnet);
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
	output_enable (config.output.enabled);
	config_enable (config.config.enabled);
	sntp_enable (config.sntp.enabled);
	debug_enable (config.debug.enabled);
	mdns_enable (config.mdns.enabled);

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
			order12[p][i] = mux_order[p] + adc_order[i]*MUX_MAX;

	for (p=0; p<MUX_MAX; p++)
		for (i=0; i<ADC_SING_LENGTH; i++)
			order3[p][i] = mux_order[p] + adc_order[ADC_DUAL_LENGTH*2+i]*MUX_MAX;

	// set channels in register
	adc_set_conv_seq (ADC1, adc1_raw_sequence, ADC_DUAL_LENGTH);
	adc_set_conv_seq (ADC2, adc2_raw_sequence, ADC_DUAL_LENGTH);
	adc_set_conv_seq (ADC3, adc3_raw_sequence, ADC_SING_LENGTH);

	ADC1->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	nvic_irq_enable (NVIC_ADC1_2);

	ADC3->regs->IER |= ADC_IER_EOS; // enable end-of-sequence interrupt
	ADC3->regs->CFGR |= ADC_CFGR_DMAEN; // enable DMA request
	ADC3->regs->CFGR |= ADC_CFGR_DMACFG, // enable ADC circular mode for use with DMA
	nvic_irq_enable (NVIC_ADC3);

	ADC12_BASE->CCR |= ADC_MDMA_MODE_ENABLE_12_10_BIT; // enable ADC DMA in 12-bit dual mode
	ADC12_BASE->CCR |= ADC_CCR_DMACFG; // enable ADC circular mode for use with DMA
	ADC12_BASE->CCR |= ADC_MODE_DUAL_REGULAR_ONLY; // set DMA dual mode to regular channels only

	adc_enable (ADC1);
	adc_enable (ADC2);
	adc_enable (ADC3);

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

	adc_tube3.tube_dst = (void *)adc3_raw;
	status = dma_tube_cfg (DMA2, DMA_CH5, &adc_tube3);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);

	dma_set_priority (DMA2, DMA_CH5, DMA_PRIORITY_MEDIUM);
	dma_set_num_transfers (DMA2, DMA_CH5, 1*MUX_MAX*2);
	dma_attach_interrupt (DMA2, DMA_CH5, adc3_dma_irq);
	dma_enable (DMA2, DMA_CH5);
	nvic_irq_set_priority (NVIC_DMA_CH5, ADC_DMA_PRIORITY);

	// set up continuous music controller struct
	cmc_init ();
	dump_init (sizeof(adc_swap), adc_swap);
	tuio2_init ();
	scsynth_init ();
	oscmidi_init ();
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
