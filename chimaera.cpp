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

const uint8_t MUX_LENGTH = 4;
const uint8_t MUX_MAX = 16;
uint8_t MUX_Sequence [MUX_LENGTH] = {1, 0, 25, 26}; // digital out pins to switch MUX channels

volatile uint8_t adc_dma_half_done;
volatile uint8_t adc_dma_done;
//const uint8_t ADC_LENGTH = 9; // the number of channels to be converted per ADC  FIXME
//const uint8_t ADC_DUAL_LENGTH = 5; // the number of channels to be converted per ADC FIXME
const uint8_t ADC_LENGTH = 3; // the number of channels to be converted per ADC 
const uint8_t ADC_DUAL_LENGTH = 2; // the number of channels to be converted per ADC 
uint16_t rawDataArray[MUX_MAX][ADC_DUAL_LENGTH*2]; // the dma temporary data array.
//uint8_t ADC1_Sequence [ADC_DUAL_LENGTH] = {11, 9, 7, 5, 3}; // analog input pins read out by the ADC FIXME
uint8_t ADC1_Sequence [ADC_DUAL_LENGTH] = {4, 4}; // analog input pins read out by the ADC
uint8_t ADC1_RawSequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
//uint8_t ADC2_Sequence [ADC_DUAL_LENGTH] = {10, 8, 6, 4, 4}; // analog input pins read out by the ADC FIXME
uint8_t ADC2_Sequence [ADC_DUAL_LENGTH] = {3, 5}; // analog input pins read out by the ADC
uint8_t ADC2_RawSequence [ADC_DUAL_LENGTH]; // ^corresponding raw ADC channels
const uint16_t ADC_BITDEPTH = 0xfff; // 12bit
const uint16_t ADC_HALF_BITDEPTH = 0x7ff; // 11bit
static uint8_t ADC_Order_MUX [MUX_MAX] = { 11, 10, 9, 8, 7, 6, 5, 4, 0, 1, 2, 3, 12, 13, 14, 15 };
//static uint8_t ADC_Order_ADC [ADC_LENGTH] = { 8, 7, 6, 5, 4, 3, 2, 1, 0 }; //FIXME
static uint8_t ADC_Order_ADC [ADC_LENGTH] = { 0, 1, 2 };
uint16_t ADC_Offset [MUX_MAX][ADC_LENGTH];

const uint8_t PWDN = 17;

/*
 * include chimaera custom libraries
 */
#include <chimutil.h>

#define ADC_CR1_DUALMOD_BIT 16

timestamp64u_t t0;
timestamp64u_t now;
nOSC_Server *serv = NULL;

HardwareSPI spi(2);
HardwareTimer adc_timer(1);
HardwareTimer sntp_timer(2);

volatile uint8_t mux_counter = MUX_MAX;
volatile uint8_t sntp_should_request = 0;
uint8_t first = 1;

void
timestamp_set (timestamp64u_t *ptr)
{
	timestamp64u_t tmp;

	tmp.part.frac = (micros() % 1000000) * 4295; // only count up to millis // 1us = 1us/1e6*2^32 =~ 4295
	tmp.part.sec = millis() / 1000;

	*ptr = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_uint64 (tmp));
}

static void
sntp_timer_irq ()
{
	sntp_should_request = 1;
}

static void
adc_eoc_irq (void)
{
	//TODO
}

static void
adc_timer_irq ()
{
	//if ( (ADC1->regs->SR & ADC_SR_EOC) == ADC_SR_EOC) TODO
		if (mux_counter < MUX_MAX)
		{
			digitalWrite (MUX_Sequence[0], (mux_counter & 0b0001)>>0);
			digitalWrite (MUX_Sequence[1], (mux_counter & 0b0010)>>1);
			digitalWrite (MUX_Sequence[2], (mux_counter & 0b0100)>>2);
			digitalWrite (MUX_Sequence[3], (mux_counter & 0b1000)>>3);
		
			delayMicroseconds (1); //TODO don't use delay in interrupt
			ADC1->regs->CR2 |= ADC_CR2_SWSTART;

			mux_counter++;
		}
		else
			adc_timer.pause ();
}

static void
adc_dma_irq ()
{
	switch (dma_get_irq_cause (DMA1, DMA_CH1))
	{
		case DMA_TRANSFER_HALF_COMPLETE:
			adc_dma_half_done = 1;
			return;
		case DMA_TRANSFER_COMPLETE:
			adc_dma_done = 1;
			return;
		default:
			break;
	};
}

inline void
adc_dma_run ()
{
	adc_dma_half_done = 0;
	adc_dma_done = 0;
	mux_counter = 0;
	adc_timer.resume ();
}

inline void
adc_dma_half_block ()
{
	while (!adc_dma_half_done)
		;
}

inline void
adc_dma_block ()
{
	while (!adc_dma_done)
		;
}

inline void
loop ()
{
	uint8_t cmc_job;
	uint16_t len;

	adc_dma_run ();

	if (config.dump.enabled)
	{
		timestamp_set (&now);
		//len = cmc_dump (cmc, now, buf_o);
		len = cmc_dump_partial (cmc, now, buf_o, 0*MUX_MAX, 3*MUX_MAX);
		//len = cmc_dump_partial (cmc, now, buf_o, 3*MUX_MAX, 6*MUX_MAX);
		//len = cmc_dump_partial (cmc, now, buf_o, 6*MUX_MAX, 9*MUX_MAX);
		udp_send (config.dump.socket.sock, buf_o, len);
	}

	if (config.tuio.enabled || config.rtpmidi.enabled)
	{
		cmc_job = cmc_process (cmc);

		if (cmc_job)
		{
			timestamp_set (&now);

			if (config.tuio.enabled)
			{
				len = cmc_write_tuio2 (cmc, now, buf_o); //TODO speed this up
				udp_send (config.tuio.socket.sock, buf_o, len);
			}

			if (config.rtpmidi.enabled)
			{
				len = cmc_write_rtpmidi (cmc, buf_o);
				udp_send (config.rtpmidi.payload.socket.sock, buf_o, len);
			}
		}
	}

	// run osc config server
	if (config.config.enabled && (len = udp_available (config.config.socket.sock)) )
	{
		udp_receive (config.config.socket.sock, buf_i, len);

		// separate concurrent UDP messages
		uint16_t remaining = len; //TODO implement UDP packet iterator
		while (remaining)
		{
			uint8_t *buf_in_ptr = buf_i+len-remaining;

			// get UDP header info
			//uint8_t *src_ip = buf_in_ptr; // TODO handle it?
			//uint16_t src_port = (buf_in_ptr[4] << 8) + buf_in_ptr[5]; // TODO handle it?
			uint16_t src_size = (buf_in_ptr[6] << 8) + buf_in_ptr[7];
		
			nosc_server_dispatch (serv, buf_in_ptr+UDP_HDR_SIZE, src_size); // skip UDP header

			remaining -= UDP_HDR_SIZE + src_size;
		}
	}

	// send sntp request
	if (config.sntp.enabled)
	{
		if (sntp_should_request)
		{
			sntp_should_request = 0;

			timestamp_set (&now);
			len = sntp_request (buf_o, now);
			udp_send (config.sntp.socket.sock, buf_o, len);
		}

		// receive sntp request answer
		if ( (len = udp_available (config.sntp.socket.sock)) )
		{
			udp_receive (config.sntp.socket.sock, buf_i, len);

			// separate concurrent UDP messages
			uint16_t remaining = len;
			while (remaining)
			{
				uint8_t *buf_in_ptr = buf_i+len-remaining;

				// get UDP header info
				//uint8_t *src_ip = buf_in_ptr; // TODO handle it?
				//uint16_t src_port = (buf_in_ptr[4] << 8) + buf_in_ptr[5]; // TODO handle it?
				uint16_t src_size = (buf_in_ptr[6] << 8) + buf_in_ptr[7];

				timestamp64u_t *transmit;
				timestamp64u_t roundtrip_delay;
				timestamp64s_t clock_offset;

				timestamp_set (&now);
				transmit = sntp_dispatch (buf_in_ptr+UDP_HDR_SIZE, now, &roundtrip_delay, &clock_offset);

				if (t0.all == 0ULL)
				{
					t0.part.sec = transmit->part.sec;
					t0.part.frac = transmit->part.frac;
				}
				else
					t0 = uint64_to_timestamp (timestamp_to_uint64 (t0) + timestamp_to_int64 (clock_offset));
			
				remaining -= UDP_HDR_SIZE + src_size;
			}
		}
	}

	if (!first)
	{
		// store second half of ADC values into CMC struct
		for (uint8_t p=MUX_MAX/2; p<MUX_MAX; p++)
			for (uint8_t i=0; i<ADC_LENGTH; i++)
			{
				int16_t adc_data;
				adc_data = (rawDataArray[p][i] & ADC_BITDEPTH) - ADC_Offset[p][i];
				cmc_set (cmc, ADC_Order_MUX[p] + ADC_Order_ADC[i]*MUX_MAX, abs (adc_data), adc_data < 0);
			}

		/*
		// TODO broken sensors
		cmc_set (cmc, 0x0 + 0x10*8, 0, 0);
		cmc_set (cmc, 0x2 + 0x10*8, 0, 0);
		cmc_set (cmc, 0x3 + 0x10*8, 0, 0);

		cmc_set (cmc, 0xc + 0x10*7, 0, 0);
		*/
	}
	else
		first = 0;

	adc_dma_half_block ();

	// store first half of ADC values into CMC struct
	for (uint8_t p=0; p<MUX_MAX/2; p++)
		for (uint8_t i=0; i<ADC_LENGTH; i++)
		{
			int16_t adc_data;
			adc_data = (rawDataArray[p][i] & ADC_BITDEPTH) - ADC_Offset[p][i];
			cmc_set (cmc, ADC_Order_MUX[p] + ADC_Order_ADC[i]*MUX_MAX, abs (adc_data), adc_data < 0);
		}

	adc_dma_block ();
}

extern "C" void adc_timer_pause ()
{
	adc_timer.pause ();
}

extern "C" void adc_timer_reconfigure ()
{
  adc_timer.setPeriod (1e6/config.rate/MUX_MAX); // set period based on update rate and mux channels
	adc_timer.setChannel1Mode (TIMER_OUTPUT_COMPARE);
	adc_timer.setCompare (TIMER_CH1, 1);  // Interrupt 1 count after each update
	adc_timer.attachCompare1Interrupt (adc_timer_irq);
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
  sntp_timer.setPeriod (1e6 * 6); // update at 6Hz
	sntp_timer.setChannel1Mode (TIMER_OUTPUT_COMPARE);
	sntp_timer.setCompare (TIMER_CH1, 1);  // Interrupt 1 count after each update
	sntp_timer.attachCompare1Interrupt (sntp_timer_irq);
	sntp_timer.refresh ();
}

extern "C" void sntp_timer_resume ()
{
	sntp_timer.resume ();
}

inline void
setup ()
{
	uint8_t i;

  pinMode (BOARD_BUTTON_PIN, INPUT);
  pinMode (BOARD_LED_PIN, OUTPUT);

	pinMode (PWDN, OUTPUT);
	digitalWrite (PWDN, 0); // enable wiz820io

	for (i=0; i<MUX_LENGTH; i++)
		pinMode (MUX_Sequence[i], OUTPUT);

	for (i=0; i<ADC_DUAL_LENGTH; i++)
	{
		pinMode (ADC1_Sequence[i], INPUT_ANALOG);
		pinMode (ADC2_Sequence[i], INPUT_ANALOG);
	}

	// init eeprom for I2C1
	eeprom_init (I2C1, _24LC64_SLAVE_ADDR | 0b000);
	//TODO implement reading and writing the config

	// init DMA
	dma_init (DMA1);

	// set up udp
  spi.begin (SPI_18MHZ, MSBFIRST, 0); 
	pinMode (BOARD_SPI2_NSS_PIN, OUTPUT);

	spi_rx_dma_enable (SPI2); // Enables RX DMA on SPI2
	spi_tx_dma_enable (SPI2); // Enables TX DMA on SPI2

	udp_init (config.comm.mac, config.comm.ip, config.comm.gateway, config.comm.subnet,
		PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_device, PIN_MAP[BOARD_SPI2_NSS_PIN].gpio_bit);

	tuio_enable (config.tuio.enabled);
	config_enable (config.config.enabled);
	sntp_enable (config.sntp.enabled);
	dump_enable (config.dump.enabled);
	debug_enable (config.debug.enabled);
	rtpmidi_enable (config.rtpmidi.enabled);
	ping_enable (config.ping.enabled);

	// set start time to 0
	t0.all = 0ULL;

	// add methods to OSC server
	serv = config_methods_add (serv, buf_o);

	// set up ADCs
	adc_set_exttrig (ADC1, 1);
	adc_set_exttrig (ADC2, 1);

	adc_set_extsel (ADC1, ADC_EXT_EV_SWSTART);
	adc_set_extsel (ADC2, ADC_EXT_EV_SWSTART);

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

	//ADC1->regs->CR1 |= ADC_CR1_EOCIE; // enable interrupt
	//ADC2->regs->CR1 |= ADC_CR1_EOCIE; // enable interrupt
	//adc12_attach_interrupt (adc_eoc_irq);

	ADC1->regs->CR1 |= 6U << ADC_CR1_DUALMOD_BIT; // 6: regular simultaneous dual mode
  ADC1->regs->CR2 |= ADC_CR2_DMA; // use DMA (write analog values directly into DMA buffer)

  adc_set_reg_seqlen (ADC1, ADC_DUAL_LENGTH);  //The number of channels to be converted 
  adc_set_reg_seqlen (ADC2, ADC_DUAL_LENGTH);  //The number of channels to be converted

	// fill raw sequence array with corresponding ADC channels
	for (uint8_t i=0; i<ADC_DUAL_LENGTH; i++)
	{
		ADC1_RawSequence[i] = PIN_MAP[ADC1_Sequence[i]].adc_channel;
		ADC2_RawSequence[i] = PIN_MAP[ADC2_Sequence[i]].adc_channel;
	}

	// set channels in register
	set_adc_sequence (ADC1, ADC1_RawSequence, ADC_DUAL_LENGTH);
	set_adc_sequence (ADC2, ADC2_RawSequence, ADC_DUAL_LENGTH);

	// set up ADC DMA tube
	adc_tube.tube_dst = (void *)rawDataArray;
	int status = dma_tube_cfg (DMA1, DMA_CH1, &adc_tube);
	ASSERT (status == DMA_TUBE_CFG_SUCCESS);
	dma_set_priority (DMA1, DMA_CH1, DMA_PRIORITY_HIGH);    //Optional
	dma_set_num_transfers (DMA1, DMA_CH1, ADC_DUAL_LENGTH*MUX_MAX);
	dma_attach_interrupt (DMA1, DMA_CH1, adc_dma_irq);
	dma_enable (DMA1, DMA_CH1);                //CCR1 EN bit 0

	// set up continuous music controller struct
	cmc = cmc_new (ADC_LENGTH*MUX_MAX, 8, ADC_HALF_BITDEPTH, config.cmc.diff, config.cmc.thresh0, config.cmc.thresh1);

	// init adc_timer (but do not start it yet)
	adc_timer_pause ();
	adc_timer_reconfigure ();

	// initialize sensor offsets 
	for (uint8_t p=0; p<MUX_MAX; p++)
		for (uint8_t i=0; i<ADC_LENGTH; i++)
			ADC_Offset[p][i] = ADC_HALF_BITDEPTH;

	// calibrate sensors offsets
	for (uint8_t c=0; c<100; c++)
	{
		adc_dma_run ();
		adc_dma_block ();

		for (uint8_t p=0; p<MUX_MAX; p++)
			for (uint8_t i=0; i<ADC_LENGTH; i++)
			{
				int16_t adc_data;
				adc_data = (rawDataArray[p][i] & ADC_BITDEPTH); // ADC lower 12 bit out of 16 bits data register
				ADC_Offset[p][i] += adc_data;
				ADC_Offset[p][i] /= 2;
			}
	}

	// init sntp_timer
	sntp_timer_pause ();
	sntp_timer_reconfigure ();
	sntp_timer_resume ();
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
