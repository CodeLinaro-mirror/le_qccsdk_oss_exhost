/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QAPI_SPI_H__
#define __QAPI_SPI_H__

#include "ring.h"

#define SPI_SLAVE_ID_LENGTH	3

typedef enum{
    QAPI_ERROR = -1,  
	QAPI_OK = 0,
} qapi_status_t;

struct device_id {
	uint8_t array[SPI_SLAVE_ID_LENGTH];
};

/**
 * @brief API to be used to send data to QCC730.
 *
 * @param[in] buf              Pointer to write buffer 
 * @param[in] write_len        Length of payload in write buffer
 * @param[in] ring_id          Identity of ring 
 * @param[in] info             Extra information 
 * @return  
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_spi_ring_write(uint8_t *buf, uint16_t write_len, uint8_t ring_id, uint16_t info);

/**
 * @brief API to be used to read data from QCC730.
 *
 * @param[out] buf              Pointer to read buffer 
 * @param[in/out] read_len      Pointer to the length of read buffer and the pointer to the length of read data 
 * @param[in] ring_id           Identity of ring 
 * @param[out] info             Pointer to extra information 
 * @return  
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_spi_ring_read(uint8_t *buf, uint16_t *read_len, uint8_t ring_id, uint16_t *info);

/**
 * @brief API to be used to get device ID of QCSPI Slave.
 *
 * @param[out] id              Pointer to device id 
 * @return  
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_spi_get_slaveid(struct device_id *id);

/**
 * @brief API to be used to get device ID of QCSPI Slave.
 * 
 * @param[in] ring_id          Identity of ring  
 * @param[out] unread          Count of Ring buffers to be read 
 * @param[out] len             Pointer to data length of nex ring buffer to be read 
 * @return  
 * Length of data for the next read  -- On success.\n
 * Zero -- On failure.
 */
int32_t qapi_spi_ring_ready(uint8_t ring_id, uint8_t *unread, uint16_t *len);

/**
 * @brief API to be used to initialize the ring interface.
 * return
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_spi_ring_init( void );

/**
 * @brief API to be used to initialize the ring interface.
 */
void qapi_spi_init( void );

#endif
