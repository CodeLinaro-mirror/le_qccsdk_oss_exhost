/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __QAPI_ATCMD_H__
#define __QAPI_ATCMD_H__

#include "atcmd.h"

typedef enum{
    QCC730_SPI_NOT_READY,
	QCC730_SPI_READY,
} qcc730_spi_state;

typedef enum{
    QAPI_ERROR = -1,  
	QAPI_OK = 0,
} qapi_status_t;


typedef void (*qapi_atcmd_recv_callback)(uint8_t *data, uint32_t len);

/**
 * @brief API to be used to initialize the resource of AT command.
 */
void qapi_atcmd_init();

/**
 * @brief API to be used to get the state of SPI Slave.
 * return the state of SPI.
 */
int32_t qapi_atcmd_get_spi_state(void);

/**
 * @brief API to be used to set the callback funcion which will process the 
 * received response of AT command or Event from QCC730.
 *
 * @param[in] callback        pointer of callback function
 * @return  
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_atcmd_set_recv_cb(qapi_atcmd_recv_callback callback);

/**
 * @brief API to be used to release the semaphore of AT command.
 */
void qapi_atcmd_release_semp(void);

/**
 * @brief API to be used to send AT command to QCC730.
 *
 * @param[in] cmd        pointer of AT command
 * @param[in] len        the length of AT command
 * @return  
 * QAPI_OK -- On success.\n
 * QAPI_ERROR -- On failure.
 */
int32_t qapi_atcmd_send(uint8_t *cmd, uint32_t len);
#endif
