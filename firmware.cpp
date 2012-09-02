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

uint8_t mux_sequence [MUX_LENGTH] = {19, 20, 21, 22}; // digital out pins to switch MUX channels
gpio_dev *mux_gpio_dev [MUX_LENGTH];
uint8_t mux_gpio_bit [MUX_LENGTH];

uint8_t adc1_sequence [ADC_DUAL_LENGTH] = {11, 9, 7, 5, 3}; // analog input pins read out by the ADC 
uint8_t adc2_sequence [ADC_DUAL_LENGTH] = {10, 8, 6, 4, 4}; // analog input pins read out by the ADC 

uint8_t adc1_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
uint8_t adc2_raw_sequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels

int16_t adc_raw[2][MUX_MAX][ADC_DUAL_LENGTH*2]; // the dma temporary data array.

uint8_t mux_order [MUX_MAX] = { 11, 10, 9, 8, 7, 6, 5, 4, 0, 1, 2, 3, 12, 13, 14, 15 };
uint8_t adc_order [ADC_LENGTH] = { 8, 7, 6, 5, 4, 3, 2, 1, 0 };
uint8_t order [MUX_MAX][ADC_LENGTH];

nOSC_Server *serv = NULL;

HardwareSPI spi(2);
HardwareTimer adc_timer(1);
HardwareTimer sntp_timer(2);
HardwareTimer config_timer(3);

uint8_t first = 1;
timestamp64u_t t0;
timestamp64u_t now;

volatile uint8_t adc_dma_done = 0;
volatile uint8_t adc_time_up = 1;
volatile uint8_t adc_raw_ptr = 0;
volatile uint8_t mux_counter = MUX_MAX;
volatile uint8_t sntp_should_request = 0;
volatile uint8_t sntp_should_listen = 0;
volatile uint8_t config_should_request = 0;

void
timestamp_set (timestamp64u_t *ptr)
{
	timestamp64u_t tmp;

	tmp.part.frac = (micros() % 1000000) * 4295; // only count up to millis // 1us = 1us/1e6*2^32 =~ 4295
	tmp.part.sec = millis() / 1000;

	*ptr = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_uint64 (tmp));
}

static void
adc_timer_irq ()
{
	adc_time_up = 1;
	adc_timer.pause ();
}

static void
sntp_timer_irq ()
{
	sntp_should_request = 1;
}

static void
config_timer_irq ()
{
	config_should_request = 1;
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
		ADC1->regs->CR2 |= ADC_CR2_SWSTART;
		mux_counter++;
	}
}

static void
adc_dma_irq ()
{
	switch (dma_get_irq_cause (DMA1, DMA_CH1))
	{
		case DMA_TRANSFER_HALF_COMPLETE:
			adc_raw_ptr = 0;
			adc_dma_done = 1;
			return;
		case DMA_TRANSFER_COMPLETE:
			adc_raw_ptr = 1;
			adc_dma_done = 1;
			return;
		default:
			break;
	};
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
}

void
config_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	nosc_server_dispatch (serv, buf, len);
}

void
sntp_cb (uint8_t *ip, uint16_t port, uint8_t *buf, uint16_t len)
{
	timestamp64u_t *transmit;
	timestamp64u_t roundtrip_delay;
	timestamp64s_t clock_offset;

	timestamp_set (&now);
	transmit = sntp_dispatch (buf, now, &roundtrip_delay, &clock_offset);

	if (t0.all == 0ULL)
	{
		t0.part.sec = transmit->part.sec;
		t0.part.frac = transmit->part.frac;
	}
	else
		t0 = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_int64 (clock_offset));

	sntp_should_listen = 0;
}

uint8_t send_status = 0;
uint8_t cmc_job = 0;
uint16_t cmc_len = 0;
uint16_t len = 0;

inline void
loop ()
{
	adc_dma_run ();

	if (first) // in the first round there is no data
	{
		adc_dma_block ();
		first = 0;
		return;
	}

	if (config.rate)
	{
		adc_time_up = 0;
		adc_timer.resume ();
	}

	// TODO implement warm-up

	if (calibrating)
		range_calibrate (adc_raw[adc_raw_ptr]);

	// do touch recognition and interpolation
	if (config.tuio.enabled && !calibrating) // do nothing while calibrating
	{
//debug_str ("tuio");
		/*
		if (cmc_job) // start nonblocking sending of last cycles tuio output
			send_status = udp_send_nonblocking (config.tuio.socket.sock, !buf_o_ptr, cmc_len);

		uint8_t job = cmc_process (adc_raw[adc_raw_ptr], order); // touch recognition of current cycle

		if (job)
		{
			timestamp_set (&now);
			cmc_len = cmc_write_tuio2 (now, &buf_o[buf_o_ptr][UDP_SEND_OFFSET]); // serialization to tuio2 of current cycle blobs
		}

		if (cmc_job && send_status) // block for end of sending of last cycles tuio output
			udp_send_block (config.tuio.socket.sock);

		if (job) // switch output buffer
			buf_o_ptr = !buf_o_ptr;

		cmc_job = job;
		*/

		//FIXME synchronous for debugging
		//debug_str ("cmc_process in");
		cmc_job = cmc_process (adc_raw[adc_raw_ptr], order); // touch recognition of current cycle
		//debug_str ("cmc_process out");

		if (cmc_job)
		{
			timestamp_set (&now);
			//debug_str ("cmc_write_tuio2 in");
			cmc_len = cmc_write_tuio2 (now, &buf_o[!buf_o_ptr][UDP_SEND_OFFSET]); // serialization to tuio2 of current cycle blobs
			//debug_str ("cmc_write_tuio2 out");
			//debug_str ("udp_send in");
			udp_send (config.tuio.socket.sock, !buf_o_ptr, cmc_len);
			//debug_str ("udp_send out");
		}
	}

	// dump raw sensor data
	if (config.dump.enabled)
	{
//debug_str ("dump");
		timestamp_set (&now);

		for (uint8_t u=0; u<ADC_LENGTH; u++)
		{
			len = cmc_dump_unit (now, &buf_o[buf_o_ptr][UDP_SEND_OFFSET], u); //TODO make this faster
			udp_send (config.dump.socket.sock, buf_o_ptr, len); //TODO nonblocking sending
		}
	}

	// run osc config server
	if (config.config.enabled && config_should_request)
	{
//debug_str ("config");
		udp_dispatch (config.config.socket.sock, config_cb);
		config_should_request = 0;
	}

	// run sntp client
	if (config.sntp.enabled)
	{
		// listen for sntp request answer
		if (sntp_should_listen)
		{
//debug_str ("sntp dispatch");
			udp_dispatch (config.sntp.socket.sock, sntp_cb);
		}

		// send sntp request
		if (sntp_should_request)
		{
//debug_str ("sntp request");
			sntp_should_request = 0;
			sntp_should_listen = 1;

			timestamp_set (&now);
			len = sntp_request (&buf_o[buf_o_ptr][UDP_SEND_OFFSET], now);
			udp_send (config.sntp.socket.sock, buf_o_ptr, len);
		}
	}

	adc_dma_block ();

	if (config.rate)
		while (!adc_time_up)
			;
}

extern "C" uint32_t _micros ()
{
	return micros ();
}

extern "C" void adc_timer_pause ()
{
	adc_timer.pause ();
}

extern "C" void adc_timer_reconfigure ()
{
  adc_timer.setPeriod (1e6/config.rate); // set period based on update rate and mux channels

	adc_timer.setMode (TIMER_CH1, TIMER_OUTPUT_COMPARE);
	adc_timer.setCompare (TIMER_CH1, adc_timer.getOverflow ());
	adc_timer.attachInterrupt (TIMER_CH1, adc_timer_irq);
	// prescaler = 1
	// overflow = 72Mhz / rate

	adc_timer.refresh ();
}

extern "C" void adc_timer_resume ()
{
	adc_timer.resume ();
}

extern "C" void sntp_timer_pause ()
{
	sntp_timer.pause ();
}

extern "C" void sntp_timer_reconfigure ()
{
  sntp_timer.setPeriod (1e6 * config.sntp.tau);
	sntp_timer.setMode (TIMER_CH1, TIMER_OUTPUT_COMPARE);
	sntp_timer.setCompare (TIMER_CH1, sntp_timer.getOverflow ());  // Interrupt 1 count after each update
	sntp_timer.attachInterrupt (TIMER_CH1, sntp_timer_irq);
	sntp_timer.refresh ();
}

extern "C" void sntp_timer_resume ()
{
	sntp_timer.resume ();
}

extern "C" void config_timer_pause ()
{
	config_timer.pause ();
}

extern "C" void config_timer_reconfigure ()
{
  config_timer.setPeriod (1e6/config.config.rate); // set period based on update rate
	config_timer.setMode (TIMER_CH1, TIMER_OUTPUT_COMPARE);
	config_timer.setCompare (TIMER_CH1, config_timer.getOverflow ());  // Interrupt 1 count after each update
	config_timer.attachInterrupt (TIMER_CH1, config_timer_irq);
	config_timer.refresh ();
}

extern "C" void config_timer_resume ()
{
	config_timer.resume ();
}

inline void
setup ()
{
	uint8_t i;

  pinMode (BOARD_BUTTON_PIN, INPUT);
  pinMode (BOARD_LED_PIN, OUTPUT);

	// enable wiz820io module
	pinMode (PWDN, OUTPUT);
	digitalWrite (PWDN, 0); // enable wiz820io

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

	// init eeprom for I2C1
	eeprom_init (I2C1, _24LC64_SLAVE_ADDR | 0b000);

	// load configuration from eeprom
	config_load ();

	// load calibrated sensor ranges from eeprom
	range_load ();

	// init DMA, which is uses for SPI and ADC
	dma_init (DMA1);

	// set up SPI for usage with wiz820io
  spi.begin (SPI_18MHZ, MSBFIRST, 0); 
	pinMode (BOARD_SPI2_NSS_PIN, OUTPUT);

	spi_rx_dma_enable (SPI2); // Enables RX DMA on SPI2
	spi_tx_dma_enable (SPI2); // Enables TX DMA on SPI2

	// initialize wiz820io
	uint8_t tx_mem[UDP_MAX_SOCK_NUM] = {8, 2, 1, 1, 1, 1, 1, 1};
	uint8_t rx_mem[UDP_MAX_SOCK_NUM] = {8, 2, 1, 1, 1, 1, 1, 1};
	udp_init (config.comm.mac, config.comm.ip, config.comm.gateway, config.comm.subnet,
		PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit, tx_mem, rx_mem);

	// initialize sockets
	tuio_enable (config.tuio.enabled);
	config_enable (config.config.enabled);
	sntp_enable (config.sntp.enabled);
	dump_enable (config.dump.enabled);
	debug_enable (config.debug.enabled);

	// set start time to 0
	t0.all = 0ULL;

	// add methods to OSC server
	serv = config_methods_add (serv); //TODO move to config_enable

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
	adc_set_sample_rate (ADC1, ADC_SMPR_41_5); //TODO make this configurable
	adc_set_sample_rate (ADC2, ADC_SMPR_41_5);

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
	dma_set_priority (DMA1, DMA_CH1, DMA_PRIORITY_HIGH);    //Optional
	dma_set_num_transfers (DMA1, DMA_CH1, ADC_DUAL_LENGTH*MUX_MAX*2);
	dma_attach_interrupt (DMA1, DMA_CH1, adc_dma_irq);
	dma_enable (DMA1, DMA_CH1);                //CCR1 EN bit 0

	// set up continuous music controller struct
	cmc_init ();
	tuio2_init ();

	// load saved groups
	groups_load ();

	// init adc_timer (but do not start it yet)
	adc_timer_pause ();
	adc_timer_reconfigure ();

	// init sntp_timer
	if (config.sntp.enabled)
	{
		sntp_timer_pause ();
		sntp_timer_reconfigure ();
		sntp_timer_resume ();
	}

	// init config_timer
	if (config.config.enabled)
	{
		config_timer_pause ();
		config_timer_reconfigure ();
		config_timer_resume ();
	}
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
