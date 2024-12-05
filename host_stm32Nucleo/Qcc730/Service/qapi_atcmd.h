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

typedef void (*qapi_atcmd_recv_callback)(uint8_t *data, uint32_t len);

void qapi_atcmd_init();
int32_t qapi_atcmd_get_spi_state(void);
void qapi_atcmd_set_spi_state(qcc730_spi_state state);
void qapi_atcmd_set_recv_cb(qapi_atcmd_recv_callback callback);
void qapi_atcmd_release_semp(void);
int32_t qapi_atcmd_send(uint8_t *cmd, uint32_t len);
#endif
