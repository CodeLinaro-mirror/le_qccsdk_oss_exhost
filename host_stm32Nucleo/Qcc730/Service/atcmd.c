/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "string.h"
#include "stdio.h"
#include "atcmd.h"
#include "qapi_spi.h"
#include "qapi_atcmd.h"

qcc730_atcmd_t *qcc730_atcmd = NULL;

void atcmd_set_spi_ready(void)
{
    if (qcc730_atcmd)
    {
        qcc730_atcmd->spi_state = QCC730_SPI_READY;
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
		/* Wait till slave is ready for next transaction - osSemaphoreAcquire*/
		if (osSemaphoreAcquire(qcc730_atcmd->semaphore, osWaitForever) == osOK) {
			rx_buf_len = ATCMD_BUF_LEN;
			memset(rxbuf, 0, ATCMD_BUF_LEN);
			if(0 == qapi_spi_ring_read(rxbuf, &rx_buf_len, F2A_DATA_RING_ID, &info_type))
			{
			    sendbuf = (uint8_t *)malloc(ATCMD_BUF_LEN);
				memset(sendbuf, 0, ATCMD_BUF_LEN);
				if (sendbuf)
				{
				    memcpy(sendbuf, rxbuf, rx_buf_len);
					memset(&node, 0, sizeof(node));
					node.payload_len = rx_buf_len;
					node.payload = sendbuf;	
                	if (pdTRUE != xQueueSend(qcc730_atcmd->rx_queue, &node, portMAX_DELAY)) 
					{
                		atcmd_print("Failed to send qcc730_atcmd->rx_queue\n\r");
                        free(sendbuf);
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
		if (pdTRUE == xQueueReceive(qcc730_atcmd->rx_queue, &node, portMAX_DELAY)) 
		{		    
            if (node.payload)
            {
                if (qcc730_atcmd->recv_cb)
                {
                    qcc730_atcmd->recv_cb(node.payload, node.payload_len);
                }
				free(node.payload);                
            }
		}
    }
}

void qcc730_atcmd_init()
{
    qcc730_atcmd = (qcc730_atcmd_t*)malloc(sizeof(qcc730_atcmd_t));
	assert(qcc730_atcmd);

	memset(qcc730_atcmd, 0, sizeof(qcc730_atcmd_t));
	/* spi handshake semaphore */
	qcc730_atcmd->semaphore = osSemaphoreNew(6, 1, NULL);
	assert(qcc730_atcmd->semaphore);
	
	/* Queue - rx */
	qcc730_atcmd->rx_queue = xQueueCreate(ATCMD_RX_QUEUE_SIZE,
			sizeof(atcmd_queue_node_t));
	assert(qcc730_atcmd->rx_queue);

    xTaskCreate(qcc730_atcmd_spi_trans_task, "qcc730_atcmd_spi_trans_task", 1024*2, qcc730_atcmd, tskIDLE_PRIORITY + 5, NULL);
    vTaskDelay(300/portTICK_PERIOD_MS);
    
    xTaskCreate(qcc730_atcmd_rx_task, "qcc730_atcmd_rx_task", 1024, qcc730_atcmd, tskIDLE_PRIORITY + 5, NULL);	
    vTaskDelay(300/portTICK_PERIOD_MS);
	
}

