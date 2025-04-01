/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "string.h"
#include "stdio.h"
#include <qc_os_al.h>
#include <qc_plat_al.h>
#include "atcmd.h"
#include "qapi_spi.h"
#include "qapi_atcmd.h"

qcc730_atcmd_t *qcc730_atcmd = NULL;

void atcmd_set_spi_state(qcc730_spi_state state)
{
    if (qcc730_atcmd)
    {
        qcc730_atcmd->spi_state = state;
    }
}

void qcc730_atcmd_spi_trans_task(void * pvParameters)
{
	atcmd_print("enter atcmd task\r\n");	
	uint16_t info_type;
	uint16_t rx_buf_len = 0;
	uint8_t *sendbuf= NULL;
	uint8_t *rxbuf= NULL;
	atcmd_queue_node_t node = {0};
	
    qcc730_atcmd_t *qcc730_atcmd = (qcc730_atcmd_t*)pvParameters;
	rxbuf = qcc730_atcmd->rx_buf;
	
	while (1) 
	{
		/* Wait till slave is ready for next transaction - qc_osal_semaphore_take*/
		if (qc_osal_semaphore_take(qcc730_atcmd->semaphore, QC_OSAL_osWaitForever) == QC_OSAL_osOK) {
			rx_buf_len = ATCMD_BUF_LEN;
			memset(rxbuf, 0, ATCMD_BUF_LEN);
			if(0 == qapi_spi_ring_read(rxbuf, &rx_buf_len, F2A_DATA_RING_ID, &info_type))
			{
			    sendbuf = (uint8_t *)qc_osal_allocate_memory(ATCMD_BUF_LEN);
				if (sendbuf)
				{
  				    memset(sendbuf, 0, ATCMD_BUF_LEN);
				    memcpy(sendbuf, rxbuf, rx_buf_len);
					memset(&node, 0, sizeof(node));
					node.payload_len = rx_buf_len;
					node.payload = sendbuf;	
                	if (qcTRUE != qc_osal_queue_send(qcc730_atcmd->rx_queue, &node, QC_OSAL_portMAX_DELAY)) 
					{
                		atcmd_print("Failed to send qcc730_atcmd->rx_queue\n\r");
                        qc_osal_free_memory(sendbuf);
						sendbuf = NULL;
                	}					
				}
				else
				{
				    atcmd_print("rx task malloc fail\r\n");
				}
			}
			
		}
	}
}

void qcc730_atcmd_rx_task(void * pvParameters)
{
	atcmd_print("enter atcmd rx demo task\r\n");	
	atcmd_queue_node_t node = {0};
    qcc730_atcmd_t *qcc730_atcmd = (qcc730_atcmd_t*)pvParameters;

    while(1)
    {
		memset(&node, 0, sizeof(node));
		if (qcTRUE == qc_osal_queue_msg_receive(qcc730_atcmd->rx_queue, &node, QC_OSAL_portMAX_DELAY)) 
		{		    
            if (node.payload)
            {
                if (qcc730_atcmd->recv_cb)
                {
                    qcc730_atcmd->recv_cb(node.payload, node.payload_len);
                }
				qc_osal_free_memory(node.payload);                
            }
		}
    }
}

void qcc730_atcmd_init()
{
    qcc730_atcmd = (qcc730_atcmd_t*)qc_osal_allocate_memory(sizeof(qcc730_atcmd_t));
	assert(qcc730_atcmd);

	memset(qcc730_atcmd, 0, sizeof(qcc730_atcmd_t));
	/* spi semaphore */
	qcc730_atcmd->semaphore = qc_osal_semaphore_create(QC_OSAL_SEMP_MAX_COUNT, QC_OSAL_SEMP_INIT_COUNT, NULL);
	assert(qcc730_atcmd->semaphore);
	
	/* Queue - rx */
	qcc730_atcmd->rx_queue = qc_osal_queue_create(ATCMD_RX_QUEUE_SIZE,
			sizeof(atcmd_queue_node_t));
	assert(qcc730_atcmd->rx_queue);

    qc_osal_thread_create(qcc730_atcmd_spi_trans_task, "qcc730_atcmd_spi_trans_task", 1024*2, qcc730_atcmd, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    qc_osal_task_delay(300/QC_OSAL_portTICK_PERIOD_MS);
    
    qc_osal_thread_create(qcc730_atcmd_rx_task, "qcc730_atcmd_rx_task", 1024, qcc730_atcmd, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);	
    qc_osal_task_delay(300/QC_OSAL_portTICK_PERIOD_MS);
	
}

