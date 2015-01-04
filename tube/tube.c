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

#include <stdint.h>

#include <chimaera.h>
#include <tube.h>

#include <libmaple/adc.h>
#include <libmaple/spi.h>

dma_tube_config adc_tube12 = {
	.tube_src = &ADC12_BASE->CDR,
	.tube_src_size = DMA_SIZE_32BITS,
	.tube_dst = NULL, //set me
	.tube_dst_size = DMA_SIZE_32BITS,
	.tube_nr_xfers = 0, //set me
	.tube_flags = DMA_CFG_DST_INC | DMA_CFG_CIRC | DMA_CFG_HALF_CMPLT_IE | DMA_CFG_CMPLT_IE,
	.target_data = NULL,
	.tube_req_src = DMA_REQ_SRC_ADC1
};

dma_tube_config adc_tube3 = {
	.tube_src = &ADC3_BASE->DR,
	.tube_src_size = DMA_SIZE_16BITS,
	.tube_dst = NULL, //set me
	.tube_dst_size = DMA_SIZE_16BITS,
	.tube_nr_xfers = 0, //set me
	.tube_flags = DMA_CFG_DST_INC | DMA_CFG_CIRC | DMA_CFG_HALF_CMPLT_IE | DMA_CFG_CMPLT_IE,
	.target_data = NULL,
	.tube_req_src = DMA_REQ_SRC_ADC3
};

dma_tube_config spi_rx_tube = {
	.tube_src = &WIZ_SPI_BAS->DR,
	.tube_src_size = DMA_SIZE_8BITS,
	.tube_dst = NULL, //set me
	.tube_dst_size = DMA_SIZE_8BITS,
	.tube_nr_xfers = 0, //set me
	.tube_flags = DMA_CFG_DST_INC,
	.target_data = NULL,
	.tube_req_src = WIZ_SPI_RX_DMA_SRC
};

dma_tube_config spi_tx_tube = {
	.tube_src = NULL, //set me
	.tube_src_size = DMA_SIZE_8BITS,
	.tube_dst = &WIZ_SPI_BAS->DR,
	.tube_dst_size = DMA_SIZE_8BITS,
	.tube_nr_xfers = 0, //set me
	.tube_flags = DMA_CFG_SRC_INC,
	.target_data = NULL,
	.tube_req_src = WIZ_SPI_TX_DMA_SRC
};
