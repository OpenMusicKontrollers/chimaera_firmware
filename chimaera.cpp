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
#include <wirish.h>
#include <adc.h> // analog to digital converter
#include <dma.h> // direct memory access

#define MUX_DELAY 1
const uint8_t MUX_LENGTH = 2;
const uint8_t MUX_MAX = 4;
uint8_t MUX_Sequence [MUX_LENGTH] = {9, 10}; // digital out pins to switch MUX channels

volatile uint8_t adc_dma_done;
const uint8_t ADC_LENGTH = 6; // the number of channels to be converted per ADC 
uint16_t rawDataArray[ADC_LENGTH*MUX_MAX]; // the dma temporary data array.
uint8_t ADC_Sequence [ADC_LENGTH] = {8, 3, 6, 5, 4, 7}; // analog input pins read out by the ADC
uint8_t ADC_RawSequence [ADC_LENGTH] = {
	PIN_MAP[ADC_Sequence[0]].adc_channel,
	PIN_MAP[ADC_Sequence[1]].adc_channel,
	PIN_MAP[ADC_Sequence[2]].adc_channel,
	PIN_MAP[ADC_Sequence[3]].adc_channel,
	PIN_MAP[ADC_Sequence[4]].adc_channel,
	PIN_MAP[ADC_Sequence[5]].adc_channel}; // ^corresponding raw ADC channels
const uint16_t ADC_BITDEPTH = 0xfff; // 12bit
const uint16_t ADC_HALF_BITDEPTH = 0x7ff; // 11bit

uint8_t ADC_Order [MUX_MAX*ADC_LENGTH] = {
	0+2, 0+6, 8+2, 8+6, 16+2, 16+6, 
	0+5, 0+1, 8+5, 8+1, 16+5, 16+1, 
	0+3, 0+0, 8+3, 8+0, 16+3, 16+0, 
	0+4, 0+7, 8+4, 8+7, 16+4, 16+7};

uint16_t ADC_Offset [MUX_MAX*ADC_LENGTH] = {
	ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH,
	ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH,
	ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH,
	ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH, ADC_HALF_BITDEPTH};

const uint8_t PWDN = 12;

/*
 * nano Open Sound Control lib
 * continuous music controller lib
 * dma udp lib
 * configuration lib
 */
#include <nosc.h>
#include <cmc.h>
#include <dma_udp.h>
#include <config.h>
#include <sntp.h>
#include <tube.h>

static uint8_t buf[1024]; // general purpose buffer used mainly for nOSC serialization
static uint8_t buf_in[1024]; // general purpose buffer used mainly for nOSC serialization

CMC *cmc = NULL;
timestamp64u_t t0;
timestamp64u_t now;
nOSC_Server *serv = NULL;

HardwareSPI spi(2);
HardwareTimer adc_timer(1);
HardwareTimer sntp_timer(2);

volatile uint8_t mux_counter = MUX_MAX;
volatile uint8_t sntp_should_request = 0;

void
timestamp_set (timestamp64u_t *ptr)
{
	timestamp64u_t tmp;

	tmp.part.frac = (micros() % 1000000) * 4295; // only count up to millis // 1us = 1us/1e6*2^32 =~ 4295
	tmp.part.sec = millis() / 1000;

	*ptr = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_uint64 (tmp));
}

void
sntp_timer_irq (void)
{
	sntp_should_request = 1;
}

void
adc_timer_irq (void)
{
	if (mux_counter < MUX_MAX)
	{
		digitalWrite (MUX_Sequence[0], (mux_counter&1));
		digitalWrite (MUX_Sequence[1], (mux_counter&3)>>1);
		//delayMicroseconds (MUX_DELAY); // let muxes and sensors settle
		
		ADC1->regs->CR2 |= ADC_CR2_SWSTART;

		mux_counter++;
	}
}

void
adc_dma_irq (void)
{
	adc_dma_done = 1;
}

void
adc_dma_run ()
{
	adc_dma_done = 0;
	mux_counter = 0;
}

void
adc_dma_block ()
{
	while (!adc_dma_done)
		;
}

void
loop ()
{
	uint8_t cmc_job;
	uint16_t len;

	adc_dma_run ();

	cmc_job = cmc_process (cmc);

	/*
	len = cmc_dump (cmc, buf);
	dma_udp_send (config.comm.tuio_sock, buf, len);
	cmc_job = 0;
	*/

	if (cmc_job)
	{
		timestamp_set (&now);

		len = cmc_write (cmc, now, buf);
		dma_udp_send (config.comm.tuio_sock, buf, len);
	}

	// run osc server
	if ( (len = dma_udp_available (config.comm.config_sock)) )
	{
		dma_udp_receive (config.comm.config_sock, buf_in, len);

		// separate concurrent UDP messages
		uint16_t remaining = len;
		while (remaining)
		{
			uint8_t *buf_in_ptr = buf_in+len-remaining;

			// get UDP header info
			//uint8_t *src_ip = buf_in_ptr; // TODO handle it?
			//uint16_t src_port = (buf_in_ptr[4] << 8) + buf_in_ptr[5]; // TODO handle it?
			uint16_t src_size = (buf_in_ptr[6] << 8) + buf_in_ptr[7];
		
			nosc_server_dispatch (serv, buf_in_ptr+8, src_size); // skip UDP header

			remaining -= 8 + src_size;
		}
	}

	// send sntp request
	if (sntp_should_request)
	{
		sntp_should_request = 0;

		timestamp_set (&now);
		len = sntp_request (buf, now);
		dma_udp_send (config.comm.sntp_sock, buf, len);
	}

	// receive sntp request answer
	if ( (len = dma_udp_available (config.comm.sntp_sock)) )
	{
		dma_udp_receive (config.comm.sntp_sock, buf_in, len);

		// separate concurrent UDP messages
		uint16_t remaining = len;
		while (remaining)
		{
			uint8_t *buf_in_ptr = buf_in+len-remaining;

			// get UDP header info
			//uint8_t *src_ip = buf_in_ptr; // TODO handle it?
			//uint16_t src_port = (buf_in_ptr[4] << 8) + buf_in_ptr[5]; // TODO handle it?
			uint16_t src_size = (buf_in_ptr[6] << 8) + buf_in_ptr[7];

			timestamp64u_t transmit;
			timestamp64u_t roundtrip_delay;
			timestamp64s_t clock_offset;

			timestamp_set (&now);
			transmit = sntp_dispatch (buf_in_ptr+8, now, &roundtrip_delay, &clock_offset);

			if (t0.all == 0ULL)
				t0 = transmit;
			else
				t0 = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_int64 (clock_offset));
		
			remaining -= 8 + src_size;
		}
	}

	adc_dma_block ();

	// store ADC values into CMC struct
	for (uint8_t p=0; p<MUX_MAX; p++)
		for (uint8_t i=0; i<ADC_LENGTH; i++)
		{
			int16_t adc_data;
			adc_data = (rawDataArray[p*ADC_LENGTH + i] & ADC_BITDEPTH); // ADC lower 12 bit out of 16 bits data register
			adc_data -= ADC_Offset[p*ADC_LENGTH + i];
			adc_data = abs (adc_data); // [0, ADC_HALF_BITDPEPTH]
			cmc_set (cmc, ADC_Order[p*ADC_LENGTH + i], adc_data);
		}
}

/*
 * This function converts the array into one number by multiplying each 5-bits
 * channel numbers by multiplications of 2^5
 */
uint32_t
calc_adc_sequence (uint8_t *adc_sequence_array, uint8_t n)
{
  uint32_t adc_sequence=0;

  for (uint8_t i=0; i<n; i++) // there are 6 available sequences in each SQR3 SQR2, and 4 in SQR1.
		adc_sequence += adc_sequence_array[i] << (i*5);

  return adc_sequence;
}

uint8_t
_firmware_version (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	// send success status message
	uint16_t size = nosc_message_vararg_serialize (buf, path, "Tiii", config.version.major, config.version.minor, config.version.patch_level);
	dma_udp_send (config.comm.config_sock, buf, size);

	return 1;
}

uint8_t
_rate_set (void *data, const char *path, const char *fmt, uint8_t argc, nOSC_Arg **args)
{
	config.cmc.rate = args[0]->i;

	adc_timer.pause ();
  adc_timer.setPeriod (1e6/config.cmc.rate/MUX_MAX); // set period based on update rate and mux channels
	adc_timer.refresh ();
	adc_timer.resume ();

	// send success status message
	uint16_t size = nosc_message_vararg_serialize (buf, path, "T");
	dma_udp_send (config.comm.config_sock, buf, size);

	return 1;
}

void
setup ()
{
	uint8_t i;

  pinMode (BOARD_BUTTON_PIN, INPUT);
  pinMode (BOARD_LED_PIN, OUTPUT);

	pinMode (PWDN, OUTPUT);
	digitalWrite (PWDN, 0); // enable wiz820io

	for (i=0; i<MUX_LENGTH; i++)
		pinMode (MUX_Sequence[i], OUTPUT);

	for (i=0; i<ADC_LENGTH; i++)
		pinMode (ADC_Sequence[i], INPUT_ANALOG);

	// init adc_timer
	adc_timer.pause ();
  adc_timer.setPeriod (1e6/config.cmc.rate/MUX_MAX); // set period based on update rate and mux channels
	adc_timer.setChannel1Mode (TIMER_OUTPUT_COMPARE);
	adc_timer.setCompare (TIMER_CH1, 1);  // Interrupt 1 count after each update
	adc_timer.attachCompare1Interrupt (adc_timer_irq);
	adc_timer.refresh ();
	adc_timer.resume ();

	// init sntp_timer
	sntp_timer.pause ();
  sntp_timer.setPeriod (1e6 * 6); // update at 6Hz
	sntp_timer.setChannel1Mode (TIMER_OUTPUT_COMPARE);
	sntp_timer.setCompare (TIMER_CH1, 1);  // Interrupt 1 count after each update
	sntp_timer.attachCompare1Interrupt (sntp_timer_irq);
	sntp_timer.refresh ();
	sntp_timer.resume ();

	// init DMA
	dma_init (DMA1);

	// set up dma_udp
  spi.begin (SPI_18MHZ, MSBFIRST, 0); 
	pinMode (BOARD_SPI2_NSS_PIN, OUTPUT);

	spi_rx_dma_enable (SPI2); // Enables RX DMA on SPI2
	spi_tx_dma_enable (SPI2); // Enables TX DMA on SPI2

	dma_udp_init (config.comm.mac, config.comm.ip, config.comm.gateway, config.comm.subnet,
		PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit);

	// init tuio socket
	dma_udp_begin (config.comm.tuio_sock, config.comm.tuio_port);
	dma_udp_set_remote (config.comm.tuio_sock, config.comm.remote_ip, config.comm.tuio_port);

	// init config socket
	dma_udp_begin (config.comm.config_sock, config.comm.config_port);
	dma_udp_set_remote (config.comm.config_sock, config.comm.remote_ip, config.comm.config_port);

	// init sntp socket
	dma_udp_begin (config.comm.sntp_sock, config.comm.sntp_port);
	dma_udp_set_remote (config.comm.sntp_sock, config.comm.sntp_ip, config.comm.sntp_port);

	// add methods to OSC server
	t0.all = 0ULL;
	serv = nosc_server_method_add (serv, "/omk/mtr/config/rate/set", "i", _rate_set, NULL);
	serv = nosc_server_method_add (serv, "/omk/mtr/firmware/version/get", "N", _firmware_version, NULL);

	// set up ADC
	ADC1->regs->CR1 |= ADC_CR1_SCAN;  // Set scan mode (read channels given in SQR3-1 registers in one burst)
	//ADC1->regs->CR1 |= ADC_CR1_EOCIE; // enable interrupt
  ADC1->regs->CR2 |= ADC_CR2_DMA; // use DMA (write analog values directly into DMA buffer)
  adc_set_reg_seqlen (ADC1, ADC_LENGTH);  //The number of channels to be converted. 
	uint8_t len = ADC_LENGTH;
	if (len > 12)
	{
		ADC1->regs->SQR1 = calc_adc_sequence (&(ADC_RawSequence[12]), len % 12); 
		len -= len % 12;
	}
	if (len > 6)
	{
		ADC1->regs->SQR2 = calc_adc_sequence (&(ADC_RawSequence[6]), len % 6);
		len -= len % 6;
	}
  ADC1->regs->SQR3 = calc_adc_sequence (&(ADC_RawSequence[0]), len);

	// set up ADC DMA tube
	adc_tube.tube_dst = rawDataArray;
	int status = dma_tube_cfg (DMA1, DMA_CH1, &adc_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH1, DMA_PRIORITY_HIGH);    //Optional
	dma_set_num_transfers (DMA1, DMA_CH1, ADC_LENGTH*MUX_MAX);
	dma_attach_interrupt (DMA1, DMA_CH1, adc_dma_irq);
	dma_enable (DMA1, DMA_CH1);                //CCR1 EN bit 0

	// calibrate sensors
	for (uint8_t c=0; c<100; c++)
	{
		adc_dma_run ();
		adc_dma_block ();

		for (uint8_t p=0; p<MUX_MAX; p++)
			for (uint8 i=0; i<ADC_LENGTH; i++)
			{
				int16_t adc_data;
				adc_data = (rawDataArray[p*ADC_LENGTH + i] & ADC_BITDEPTH); // ADC lower 12 bit out of 16 bits data register
				ADC_Offset[p*ADC_LENGTH + i] += adc_data;
				ADC_Offset[p*ADC_LENGTH + i] /= 2;
			}
	}

	// set up continuous music controller struct
	cmc = cmc_new (24, 12, ADC_HALF_BITDEPTH, config.cmc.diff, config.cmc.thresh0, config.cmc.thresh1);
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
