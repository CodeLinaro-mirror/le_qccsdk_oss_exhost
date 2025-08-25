/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qc_plat_al.h"
#include "qc_os_al.h"
#include "ring.h"
#include "qapi_spi.h"
#include "qapi_atcmd.h"

extern uint8_t tx_test_start;

int32_t qapi_atcmd_get_spi_state(void)
{
    if (qcc730_atcmd)
    {
        return qcc730_atcmd->spi_state;
    }
    return QCC730_SPI_NOT_READY;
}

int32_t qapi_atcmd_set_recv_cb(qapi_atcmd_recv_callback callback)
{
    if (qcc730_atcmd)
    {
        qcc730_atcmd->recv_cb= callback;
		return QAPI_OK;
    }
	return QAPI_ERROR;
}

void qapi_atcmd_release_semp(void)
{
	if (qcc730_atcmd && (qcc730_atcmd->semaphore != NULL)) {
		qc_osal_semaphore_give(qcc730_atcmd->semaphore);        
	}
}

extern char test_buf[];
int32_t qapi_atcmd_send(uint8_t *cmd, uint32_t len)
{
 	uint8_t *txbuf = qcc730_atcmd->tx_buf;
	uint32_t sent_len = 0;

	if (tx_test_start == 0) {
		if (strncmp((char*)cmd, test_buf, strlen(cmd)) != 0) {
    		atcmd_print("atcmd send: %d %s\r\n", len ,cmd);
		}
	}
	
    while(len >= ATCMD_BUF_LEN)
    {
        memcpy(txbuf, cmd+sent_len, ATCMD_BUF_LEN);
		if(qapi_spi_ring_write(txbuf, ATCMD_BUF_LEN, A2F_DATA_RING_ID, CONFIG_INFO_HFC))
		{
			atcmd_print("qapi_spi_ring_write error\r\n");
			return QAPI_ERROR;
		}

		len -= ATCMD_BUF_LEN;
		sent_len += ATCMD_BUF_LEN;
    }

    if (len)
    {        
        memcpy(txbuf, cmd+sent_len, len);
		if(qapi_spi_ring_write(txbuf, len, A2F_DATA_RING_ID, CONFIG_INFO_HFC))
		{
			atcmd_print("qapi_spi_ring_write error\r\n");
		}		
    }

    return QAPI_OK;	
}

void qapi_atcmd_init()
{
    qcc730_atcmd_init();
}

