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

#include "led.h"
#include "transport_layer.h"
#include "qapi_spi.h"
#include "qcc730_config.h"

#define TX_TO_SLAVE_QUEUE_SIZE               5
#define RX_FROM_SLAVE_QUEUE_SIZE             5

osSemaphoreId_t osSemaphore;
static osMutexId_t mutex_spi_trans;
/* Queue declaration */
static QueueHandle_t tx_to_slave_queue = NULL;
static QueueHandle_t rx_from_slave_queue = NULL;
static uint8_t *sendbuf = NULL;
volatile uint32_t int_count=0;

static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf, uint16_t *tx_len);
static STM_RET run_spi_transaction();

void handle_event(uint8_t *buffer, uint16_t size);

void transport_init()
{
	/* spi handshake semaphore */
//#if (osCMSIS >= 0x20000U)
	osSemaphore = osSemaphoreNew(6, 1, NULL);
//#else
//	osSemaphore = osSemaphoreCreate(osSemaphore(SEM) , 1);
//#endif
	assert(osSemaphore);
	mutex_spi_trans = xSemaphoreCreateMutex();
	assert(mutex_spi_trans);

	/* Queue - tx */
	tx_to_slave_queue = xQueueCreate(TX_TO_SLAVE_QUEUE_SIZE,
			sizeof(interface_handle_t));
	assert(tx_to_slave_queue);

	/* Queue - rx */
	rx_from_slave_queue = xQueueCreate(RX_FROM_SLAVE_QUEUE_SIZE,
			sizeof(interface_handle_t));
	assert(rx_from_slave_queue);
}

STM_RET tx_to_slave(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen)
{

	interface_handle_t buf_handle = {0};

	if (!wbuffer || !wlen || (wlen > MAX_SPI_BUFFER_SIZE)) {
		printf("write fail: buff(%p) 0? OR (0<len(%u)<=max_poss_len(%u))?\n\r",
				wbuffer, wlen, MAX_SPI_BUFFER_SIZE);
		if(wbuffer) {
			free(wbuffer);
			wbuffer = NULL;
		}
		return STM_FAIL;
	}
	memset(&buf_handle, 0, sizeof(buf_handle));

	buf_handle.if_type = iface_type;
	buf_handle.if_num = iface_num;
	buf_handle.payload_len = wlen;
	buf_handle.payload = wbuffer;
	/*
	printf("tx_to_slave_queue tx payload len=%d add %x, %x %x\r\n",buf_handle.payload_len,
			buf_handle.payload, buf_handle.payload[100],buf_handle.payload[200]);

	for(int i=0;i<20;i++)
	{
		printf("%x ", wbuffer[i]);
	}
	*/
	if (pdTRUE != xQueueSend(tx_to_slave_queue, &buf_handle, portMAX_DELAY)) {
		printf("Failed to send buffer tx_to_slave_queue\n\r");
		if(wbuffer) {
			free(wbuffer);
			wbuffer = NULL;
		}
		return STM_FAIL;
	}

	return run_spi_transaction();
}
uint8_t gpio_rx_data_ready = 0;
/* TODO: tmp length of return value */
uint32_t ret_read_size = 0;

uint32_t t_cnt=0, r_cnt=0;
static STM_RET run_spi_transaction()
{
	uint8_t * txbuff = NULL;

	uint8_t is_valid_tx_buf = 0;
	uint16_t size = 0;

#if 0
	/* data ready line SET -> slave wants to send something */
	gpio_rx_data_ready = HAL_GPIO_ReadPin(SPI_RX_READY_GPIO_Port
			SPI_RX_READY_Pin);
#endif
	if (1) {
		/* Get next tx buffer to be sent */
		txbuff = get_tx_buffer(&is_valid_tx_buf, &size);
//		printf("@ gpio_rx=%d, is_tx=%d\r\n", gpio_rx_data_ready, is_valid_tx_buf);
		if ((is_valid_tx_buf) ) {

			/* Execute transaction only if EITHER holds true-
			 * a. A valid tx buffer to be transmitted towards slave
			 * b. Slave wants to send something (Rx for host)
			 */
			xSemaphoreTake(mutex_spi_trans, portMAX_DELAY);

#ifdef SPI_RAW_TXRX
			SPI_Write(txbuff, size, 0x10000);
#else
			t_cnt++;
			if(qapi_spi_ring_write(txbuff, size, A2F_DATA_RING_ID, 0)){
				printf("qapi_spi_ring_write error,cnt=%d\r\n", t_cnt);
				vTaskDelay(100/portTICK_PERIOD_MS);
			}
#endif
			xSemaphoreGive(mutex_spi_trans);

			free(txbuff);

			return STM_OK;
		}
		else
		{
			printf("get txbuffer, but not free?\r\n");
			return STM_ERROR_NULLPTR;
		}
		//handle RX case
//		gpio_rx_data_ready = GPIO_PIN_SET;
#if 0
		if(gpio_rx_data_ready == 1)
		{
			if(ret_read_size>0)
				handle_rx_data();
		}
#endif
	}
}

void handle_rx_data(uint8_t unread, uint16_t size)
{
	interface_handle_t buf_handle = {0};
	uint8_t rxbuff[MAX_SPI_BUFFER_SIZE]; //multiple malloc/free is not good
	struct  target_payload_header *payload_header;
	//uint16_t size = ret_read_size;
	uint16_t info_type;
	uint8_t recv_tmp[4];
	int8_t remaining = unread;
	int32_t ret;

	/* Allocate rx buffer */
	memset(rxbuff, 0, MAX_SPI_BUFFER_SIZE);
	xSemaphoreTake(mutex_spi_trans, portMAX_DELAY);


#if 0
	xSemaphoreTake(mutex_spi_trans, portMAX_DELAY);
	size = ((struct target_payload_header *)txbuff)->len;
	SPI_Read(rxbuff, MAX_SPI_BUFFER_SIZE, 0x10000);
	xSemaphoreGive(mutex_spi_trans);
#endif

#ifdef SPI_RAW_TXRX
	SPI_Read(rxbuff, MAX_SPI_BUFFER_SIZE, 0x10000);
#else

	r_cnt++;
	//if(remaining>2)
	//	printf("handle_rx_data remaining %d, size=%d\r\n", remaining, size);
	while(remaining>0)
	{
		remaining--;
		ret = qapi_spi_ring_read(rxbuff, &size, 1/*ring_id*/, &info_type);
		if(ret)
		{
			printf("RX int=%d [r_cnt %d]:qapi_spi_ring_read error size=%d, remain=%d, ret=%d\r\n", int_count,r_cnt,size,  remaining,ret);
			xSemaphoreGive(mutex_spi_trans);
			return;
		}
	}
	xSemaphoreGive(mutex_spi_trans);
#endif
	payload_header = (struct target_payload_header *)rxbuff;

	buf_handle.if_type = 1;
	buf_handle.if_num = 0;
	buf_handle.payload_len = size/*payload_header->len*/;
	buf_handle.payload = rxbuff;
	//printf("RX int=%d [r_cnt %d]:len=%d, rxbuff = [0x%x]=%x %x ... %x %x\r\n", int_count,r_cnt, buf_handle.payload_len, &rxbuff[0],rxbuff[0], rxbuff[1],rxbuff[size-2], rxbuff[size-1]);
	recv_tmp[0] = rxbuff[0];
	recv_tmp[1] = rxbuff[1];
	recv_tmp[2] = rxbuff[size-2];
	recv_tmp[3] = rxbuff[size-1];
	//if(rxbuff[0]!=rxbuff[1])
	//	assert(0);
#if 1
	//free(rxbuff);
	//printf("RX int=%d [r_cnt %d]:len=%d, rxbuff=%x %x..%x %x\r\n", int_count,r_cnt, buf_handle.payload_len, recv_tmp[0], recv_tmp[1],recv_tmp[2], recv_tmp[3]);
	//printf("%d ",r_cnt);
#else
  //for further usage, but the rxbuffer is free in next function
	if (pdTRUE != xQueueSend(rx_from_slave_queue, &buf_handle, portMAX_DELAY))
	{
		printf("Failed to send buffer rx_from_slave_queue\n\r");
		return;
	}
#endif
}

static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf, uint16_t *tx_len)
{

	uint16_t len = 0;
	interface_handle_t buf_handle = {0};
	struct pbuf *tmp_buf;
	*is_valid_tx_buf = 0;

	/* Check if higher layers have anything to transmit, non blocking.
	 * If nothing is expected to send, queue receive will fail.
	 * In that case only payload header with zero payload
	 * length would be transmitted.
	 */
	if (pdTRUE == xQueueReceive(tx_to_slave_queue, &buf_handle, portMAX_DELAY)) {
		len = buf_handle.payload_len;
	}

	if (len) {
		sendbuf = (uint8_t *) malloc(len);
		if (!sendbuf) {
			printf("malloc failed\n\r");
			goto done;
		}

		memset(sendbuf, 0, len);

		*is_valid_tx_buf = 1;
		*tx_len = len;
		tmp_buf = (struct pbuf *)buf_handle.payload;
/*
		printf("tx_to_slave_queue recv payload len=%d, payload add %x, new sndbuf add=%x, payload[]=%x %x\r\n",
				buf_handle.payload_len, buf_handle.payload, sendbuf,
				buf_handle.payload[100],buf_handle.payload[200]);
*/
		memcpy(sendbuf, tmp_buf, len);
	}

done:

	return sendbuf;
}

void process_rx_task(void * pvParameters)
{
	STM_RET ret = STM_OK;
	interface_handle_t buf_handle = {0};
	uint8_t *recv_buf = NULL;

	while (1) {
//		printf("process rx\r\n");
		ret = xQueueReceive(rx_from_slave_queue, &buf_handle, portMAX_DELAY);

		if (ret != pdTRUE) {
			continue;
		}
		recv_buf = (struct pbuf *)buf_handle.payload;
		/*
		printf("recv queue msg from slave, addr=%x, recv_buf %02x %02x %02x %02x \r\n",
				recv_buf, recv_buf[0],recv_buf[100],recv_buf[1198],recv_buf[1199]);
		*/
		/* process received buffer for all possible interface types */
		if (buf_handle.if_type == 10) {

		} else if((buf_handle.if_type == 2) ||
				(buf_handle.if_type == 1)) {

			handle_event(buf_handle.payload, buf_handle.payload_len);

		}  else {
			printf("unknown type %d \n\r", buf_handle.if_type);
		}

	}
}

uint8_t reset_flag=0;
void transaction_task(void * pvParameters)
{
	printf("enter transaction_task\r\n");
	uint8_t unread = 1;
	uint16_t ring_len = 0;
	for (;;) {

		if (osSemaphore !=	NULL) {
			//printf("before acquire semaphore cnt=%d\r\n",osSemaphoreGetCount(osSemaphore));
			if(reset_flag == 1) /*For demo's reset command, it will raise board GPIO interrupt */{
				if (osSemaphoreAcquire(osSemaphore , osWaitForever) == osOK){
					vTaskDelay(10000/portTICK_PERIOD_MS);
				}
			}
			/* Wait till slave is ready for next transaction - osSemaphoreAcquire*/
			if (osSemaphoreAcquire(osSemaphore , osWaitForever) == osOK) {

				ret_read_size = qapi_spi_ring_ready(1, &unread, &ring_len);

				if(ring_len>0 && ring_len<=MAX_SPI_BUFFER_SIZE && unread>0)
				{
					gpio_rx_data_ready = 1;
					handle_rx_data(unread, ring_len);
					gpio_rx_data_ready = 0;
				}
				else
				{
					//printf("ring read size error %d\r\n", ret_read_size);
				}

				//printf("loop end transaction_task accquire semaphore %d, ret_read_size=%d\r\n",osSemaphoreGetCount(osSemaphore),ret_read_size);
			}
		}
		else
		{
			printf("Null semaphore\r\n");
			for(int i=0;i<100000;i++)
				;
		}

	}
}

/* This can be up layer CB function to handle received buffer */
void handle_event(uint8_t *buffer, uint16_t size)
{
	if(!buffer)
		printf("null buffer\r\n");

	printf("[%d-%d]END RX size=%d:  payload= %02x %02x %02x %02x \r\n",int_count,t_cnt,size, buffer[0],buffer[1],buffer[size-2],buffer[size-1]);

	free(buffer);
	return;
}

/*This handler is generated by GPIO pin. */
void F2A_int_handler()
{
	/* Post semaphore to notify SPI slave is ready for next transaction
	 * This should be from GPIO event as designed. TODO......
	 * */
	int_count++;
#ifdef QCC730_ATCMD_ENABLE
    extern void qapi_atcmd_release_semp(void);
    qapi_atcmd_release_semp();
#else	
	//printf("[%d]osSemaphoreRelease ready=%d sem count=%d\r\n",int_count,gpio_rx_data_ready,osSemaphoreGetCount(osSemaphore));
	if (osSemaphore != NULL) {

		osSemaphoreRelease(osSemaphore);

	}
#endif
}

