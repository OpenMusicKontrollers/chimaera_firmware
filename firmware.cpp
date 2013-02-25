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
#include <wirish/wirish.h>
#include <libmaple/i2c.h> // i2c for eeprom
#include <libmaple/adc.h> // analog to digital converter
#include <libmaple/dma.h> // direct memory access
#include <libmaple/bkp.h> // backup register

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

uint8_t mux_sequence [MUX_LENGTH] = {19, 20, 21, 22}; // digital out pins to switch MUX channels
gpio_dev *mux_gpio_dev [MUX_LENGTH];
uint8_t mux_gpio_bit [MUX_LENGTH];

uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {11, 9, 7, 5, 3}; // analog input pins read out by the ADC 
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {10, 8, 6, 4, 4}; // analog input pins read out by the ADC 

uint8_t adc1_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc2_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels

int16_t adc_raw[2][MUX_MAX][ADC_DUAL_LENGTH*2] __attribute__ ((aligned (4)));; // the dma temporary data array.
int16_t adc_rela[SENSOR_N];
int16_t adc_swap[SENSOR_N];

uint8_t mux_order [MUX_MAX] = { 11, 10, 9, 8, 7, 6, 5, 4, 0, 1, 2, 3, 12, 13, 14, 15 };
uint8_t adc_order [ADC_LENGTH] = { 8, 7, 6, 5, 4, 3, 2, 1, 0 };
uint8_t order [MUX_MAX][ADC_LENGTH];

HardwareSPI spi(2);

uint8_t first = 1;

volatile uint8_t adc_dma_done = 0;
volatile uint8_t adc_dma_err = 0;
volatile uint8_t adc_time_up = 1;
volatile uint8_t adc_raw_ptr = 1;
volatile uint8_t mux_counter = MUX_MAX;
volatile uint8_t sntp_should_request = 0;
volatile uint8_t sntp_should_listen = 0;
volatile uint8_t mdns_should_listen = 0;
volatile uint8_t config_should_listen = 0;

uint64_t now;
uint64_t offset;

CMC_Engine engines [] = {
	tuio2_engine,
	scsynth_engine,
	{NULL} // terminator
};

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

extern "C" void
__irq_adc (void)
{
	gpio_write_bit (mux_gpio_dev[0], mux_gpio_bit[0], mux_counter & 0b0001);
	gpio_write_bit (mux_gpio_dev[1], mux_gpio_bit[1], mux_counter & 0b0010);
	gpio_write_bit (mux_gpio_dev[2], mux_gpio_bit[2], mux_counter & 0b0100);
	gpio_write_bit (mux_gpio_dev[3], mux_gpio_bit[3], mux_counter & 0b1000);

	if (mux_counter < MUX_MAX)
	{
		mux_counter++;
		ADC1->regs->CR2 |= ADC_CR2_SWSTART;
	}
}

static void
adc_dma_irq ()
{
	uint8_t isr = dma_get_isr_bits (DMA1, DMA_CH1);
	dma_clear_isr_bits (DMA1, DMA_CH1);

	if (isr & 0x8)
		adc_dma_err = 1;

	adc_dma_done = 1;
}

inline void
adc_dma_run ()
{
	adc_dma_done = 0;
	mux_counter = 0;
	__irq_adc ();
}

inline void
adc_dma_block ()
{
	while (!adc_dma_done)
		;
	adc_raw_ptr ^= 1; // toggle
}

//TODO move to config.c
static void
config_bndl_start_cb (uint64_t timestamp)
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

	nosc_method_dispatch (config_serv, buf, len, config_bndl_start_cb, config_bndl_end_cb);
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

nOSC_Item nest_bndl [ENGINE_N+1]; // +1 because of TERMINATOR

uint8_t send_status = 0;
uint8_t cmc_job = 0;
uint16_t cmc_len = 0;
uint16_t len = 0;

Stop_Watch sw_adc_fill = {.id = "adc_fill", .thresh=2000};

Stop_Watch sw_dump_update = {.id = "dump_update", .thresh=2000};
Stop_Watch sw_dump_serialize = {.id = "dump_serialize", .thresh=2000};
Stop_Watch sw_dump_send = {.id = "dump_send", .thresh=2000};

Stop_Watch sw_tuio_process = {.id = "tuio_process", .thresh=2000};
Stop_Watch sw_tuio_serialize = {.id = "tuio_serialize", .thresh=2000};
Stop_Watch sw_tuio_send = {.id = "tuio_send", .thresh=2000};

inline void
loop ()
{
	if (config.rate)
	{
		adc_time_up = 0;
		timer_resume (adc_timer);
	}

	adc_dma_run ();

	if (first) // in the first round there is no data
	{
		adc_dma_block ();
		first = 0;
		return;
	}

	// fill adc_rela
	//stop_watch_start (&sw_adc_fill);
	adc_fill (adc_raw[adc_raw_ptr], order, adc_rela, adc_swap, !calibrating); // 49us (rela only), 69us (rela & swap)
	//stop_watch_stop (&sw_adc_fill);

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
			dump_update (now, SENSOR_N*sizeof(int16_t), adc_swap); // 6us
			nosc_item_bundle_set (nest_bndl, job++, dump_bndl, nOSC_IMMEDIATE);
		}
	
		if (config.tuio.enabled || config.scsynth.enabled) // put all blob based engine flags here, e.g. TUIO, RTPMIDI, Kraken, SuperCollider, ...
		{
			uint8_t blobs = cmc_process (now, adc_rela, engines); // touch recognition of current cycle

			if (blobs) // was there any update?
			{
				if (config.tuio.enabled)
					nosc_item_bundle_set (nest_bndl, job++, tuio2_bndl, nOSC_IMMEDIATE);

				if (config.scsynth.enabled)
					nosc_item_bundle_set (nest_bndl, job++, scsynth_bndl, nOSC_IMMEDIATE);

				// if (config.rtpmidi.enabled)
				// if (config.kraken.enabled)
			}
		}

		if (job)
		{
			if (job > 1)
			{
				nosc_item_term_set (nest_bndl, job);
				cmc_len = nosc_bundle_serialize (nest_bndl, offset, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
			}
			else // job == 1, there's no need to send a nested bundle in this case
				cmc_len = nosc_bundle_serialize (nest_bndl[0].content.bundle.bndl, offset, &buf_o[buf_o_ptr][WIZ_SEND_OFFSET]);
		}

		if (cmc_job && send_status) // block for end of sending of last cycles tuio output
			udp_send_block (config.output.socket.sock);

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

	adc_dma_block ();

	if (config.rate)
		while (!adc_time_up)
			;
}

extern "C" inline uint32_t
_micros ()
{
	return micros ();
}

extern "C" void
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

extern "C" void 
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

extern "C" void
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

inline void
setup ()
{
	uint8_t i;

	SerialUSB.end (); // we don't need USB communication, so we disable it

  pinMode (BOARD_BUTTON_PIN, INPUT);
  pinMode (BOARD_LED_PIN, OUTPUT);

	// enable wiz820io module
	pinMode (UDP_PWDN, OUTPUT);
	digitalWrite (UDP_PWDN, 0); // enable wiz820io

	//TODO
	//pinMode (UDP_INT, INPUT);
	//attachInterrupt (UDP_INT, udp_irq, FALLING);

	// systick 
	systick_disable ();
	systick_init ((SYSTICK_RELOAD_VAL + 1)/10 - 1); // systick every 100us = every 7200 cycles on a maple mini @72MHz

	// setup pins to switch the muxes
	for (i=0; i<MUX_LENGTH; i++)
	{
		uint8_t pin = mux_sequence[i];
		pinMode (pin, OUTPUT);
		mux_gpio_dev[i] = PIN_MAP[pin].gpio_device;
		mux_gpio_bit[i] = PIN_MAP[pin].gpio_bit;
	}

	// setup nalog input pins
	for (i=0; i<ADC_DUAL_LENGTH; i++)
	{
		pinMode (adc1_sequence[i], INPUT_ANALOG);
		pinMode (adc2_sequence[i], INPUT_ANALOG);
	}

	// create EUI-32 and EUI-48 from unique identifier
	eui_init ();

	// initialize random number generator based on EUI_32
	uint32_t *seed = (uint32_t *)&EUI_32[0];
	srand (*seed);

	// create "unique" ethernet MAC from EUI_48
	uint8_t locally_administered = 0; // FIXME make this configurable
	memcpy (config.comm.mac, EUI_48, 6);
	if (locally_administered)
		config.comm.mac[0] = (config.comm.mac[0] | 0x02) & 0xfe; // locally administered unicast: 0bxxxxxx10
	else
		config.comm.mac[0] = config.comm.mac[0] & 0xfc; // globally administered unicast: 0bxxxxxx00

	// init eeprom for I2C1
	eeprom_init (I2C1);
	eeprom_slave_init (eeprom_24LC64, I2C1, 0b000);
	//eeprom_slave_init (eeprom__24AA025E48, I2C1, 0b001); // TODO MAC EEPROM

	// load configuration from eeprom
	/* FIXME
	uint16_t bkp_val = bkp_read (FACTORY_RESET_REG); //FIXME does not yet work
	if (bkp_val == FACTORY_RESET_VAL)
		bkp_init (); // clear backup register
	else
		config_load ();
	*/

	// load calibrated sensor ranges from eeprom
	range_load (config.calibration);

	// init DMA, which is used for SPI and ADC
	dma_init (DMA1);

	// set up SPI for usage with wiz820io
  spi.begin (SPI_18MHZ, MSBFIRST, 0);
	pinMode (BOARD_SPI2_NSS_PIN, OUTPUT);
	digitalWrite (BOARD_SPI2_NSS_PIN, HIGH); // disable peripheral
	//SPI2->regs->CR2 |= SPI_CR2_SSOE; // automatic toggling of NSS pin in single master mode, we don't use this

	spi_rx_dma_enable (SPI2); // Enables RX DMA on SPI2
	spi_tx_dma_enable (SPI2); // Enables TX DMA on SPI2

	// initialize wiz820io
	uint8_t tx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	uint8_t rx_mem[WIZ_MAX_SOCK_NUM] = {1, 8, 2, 1, 1, 1, 1, 1};
	wiz_init (PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit, tx_mem, rx_mem);
	wiz_mac_set (config.comm.mac);

	// wait for link up before proceeding
	while (!wiz_link_up ()) // TODO monitor this and go to sleep mode when link is down
		;

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

	adc_set_extsel (ADC1, ADC_EXT_EV_SWSTART);
	adc_set_extsel (ADC2, ADC_EXT_EV_SWSTART);

	adc_set_exttrig (ADC1, 1);
	adc_set_exttrig (ADC2, 1);

	/*
	ADC_SMPR_1_5
	ADC_SMPR_7_5
	ADC_SMPR_13_5
	ADC_SMPR_28_5
	ADC_SMPR_41_5
	ADC_SMPR_55_5
	ADC_SMPR_71_5
	ADC_SMPR_239_5
	*/
	adc_set_sample_rate (ADC1, ADC_SMPR_55_5); //TODO make this configurable
	adc_set_sample_rate (ADC2, ADC_SMPR_55_5);

	ADC1->regs->CR1 |= ADC_CR1_SCAN;  // Set scan mode (read channels given in SQR3-1 registers in one burst)
	ADC2->regs->CR1 |= ADC_CR1_SCAN;  // Set scan mode (read channels given in SQR3-1 registers in one burst)

  adc_set_reg_seqlen (ADC1, ADC_DUAL_LENGTH);  //The number of channels to be converted 
  adc_set_reg_seqlen (ADC2, ADC_DUAL_LENGTH);  //The number of channels to be converted

	// fill raw sequence array with corresponding ADC channels
	for (uint8_t i=0; i<ADC_DUAL_LENGTH; i++)
	{
		adc1_raw_sequence[i] = PIN_MAP[adc1_sequence[i]].adc_channel;
		adc2_raw_sequence[i] = PIN_MAP[adc2_sequence[i]].adc_channel;
	}

	for (uint8_t p=0; p<MUX_MAX; p++)
		for (uint8_t i=0; i<ADC_LENGTH; i++)
			order[p][i] = mux_order[p] + adc_order[i]*MUX_MAX;

	// set channels in register
	set_adc_sequence (ADC1, adc1_raw_sequence, ADC_DUAL_LENGTH);
	set_adc_sequence (ADC2, adc2_raw_sequence, ADC_DUAL_LENGTH);

	ADC1->regs->CR1 |= ADC_CR1_EOCIE; // enable interrupt
	nvic_irq_enable (NVIC_ADC_1_2);

	ADC1->regs->CR1 |= 6U << ADC_CR1_DUALMOD_BIT; // 6: regular simultaneous dual mode
  ADC1->regs->CR2 |= ADC_CR2_DMA; // use DMA (write analog values directly into DMA buffer)

	adc_enable (ADC1);
	adc_enable (ADC2);

	adc_calibrate (ADC1);
	adc_calibrate (ADC2);

	// set up ADC DMA tube
	adc_tube.tube_dst = (void *)adc_raw;
	int status = dma_tube_cfg (DMA1, DMA_CH1, &adc_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH1, DMA_PRIORITY_MEDIUM);    //Optional
	dma_set_num_transfers (DMA1, DMA_CH1, ADC_DUAL_LENGTH*MUX_MAX*2);
	dma_attach_interrupt (DMA1, DMA_CH1, adc_dma_irq);
	dma_enable (DMA1, DMA_CH1);                //CCR1 EN bit 0
	nvic_irq_set_priority (NVIC_DMA_CH1, ADC_DMA_PRIORITY);

	// set up continuous music controller struct
	cmc_init ();
	tuio2_init ();
	scsynth_init ();

	// load saved groups
	groups_load ();
}

__attribute__ ((constructor)) void
premain ()
{
  init ();
}

int
main (void)
{
  setup ();

  while (true)
    loop ();

  return 0;
}
