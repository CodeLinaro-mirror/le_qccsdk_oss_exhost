/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <stdint.h>
#include <string.h>

#ifndef _ATCMD_H_
#define _ATCMD_H_

#define ATCMD_BUF_LEN	1400

#define ATCMD_TXRX_OK                  0
#define ATCMD_RX_TIMEOUT              -1
#define ATCMD_RX_BUF_NOT_LONG_ENOUGH  -2
#define ATCMD_TX_QUEUE_SIZE            8
#define ATCMD_RX_QUEUE_SIZE            8

#define atcmd_print printf

typedef struct {
	uint8_t *payload;
	uint16_t payload_len;
	uint16_t info;	
} atcmd_queue_node_t;

typedef struct qcc730_atcmd_s
{
    uint8_t rx_buf[ATCMD_BUF_LEN];
    uint8_t tx_buf[ATCMD_BUF_LEN];
	int     spi_state;
    qc_osal_semaphore_handle_t semaphore;
    qc_osal_osMutexId_t mutex;
    qc_osal_queue_handle_t tx_queue;
    qc_osal_queue_handle_t rx_queue;	
	void (*recv_cb)(uint8_t *data, uint32_t len);
}qcc730_atcmd_t;

extern qcc730_atcmd_t *qcc730_atcmd;

extern void qcc730_atcmd_spi_trans_task(void * pvParameters);
extern void qcc730_atcmd_rx_task(void * pvParameters);

#endif /* SERVICE_ATCMD_H_ */
