/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QAPI_SPI_H__
#define __QAPI_SPI_H__

#include "ring.h"

#define SPI_SLAVE_ID_LENGTH	3

struct device_id {
	uint8_t array[SPI_SLAVE_ID_LENGTH];
};

int32_t qapi_spi_ring_write(uint8_t *buf, uint16_t write_len, uint8_t ring_id, uint16_t info);
int32_t qapi_spi_ring_read(uint8_t *buf, uint16_t *read_len, uint8_t ring_id, uint16_t *info);
void qapi_spi_get_slaveid(struct device_id *id);
void qapi_spi_reset(void);
int32_t qapi_spi_ring_ready(uint8_t ring_id, uint8_t *unread, uint16_t *len);
int32_t qapi_spi_ring_init( void );
void qapi_spi_init( void );

#endif
