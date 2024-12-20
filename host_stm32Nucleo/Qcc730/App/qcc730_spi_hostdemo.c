/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "qcc730_config.h"
#include "qc_os_al.h"
#include "qc_plat_al.h"
#include "qcc730_spi_host.h"
#include "qapi_spi.h"
#include "transport_layer.h"

#ifdef SHELL_FEATURE
#include "shell.h"
#endif

#ifdef QCC730_SPI_DEMO_ENABLE
#define demo_print printf

#define DEMO_RAW_SPI_TEST
#define DEMO_TXRX_TEST
/* Recommend demo testing 04-25 */
#define DEMO_BASIC_RING_TEST

#define MAX_LOOP	1000000

uint32_t qcspi_ring_init_flag = 0;

#ifdef DEMO_TXRX_TEST
static void app_txrx_demo(uint32_t num, uint32_t size)
{
	demo_print("app_txrx_demo size=%d\r\n", size);
#define BUFF_LEN	1400
	uint8_t data1=0;
	uint32_t seed=2;
	uint32_t i;
	uint32_t count;
	uint16_t buf_len = 0;
	struct pbuf g_buffer ={0};
	uint8_t user_buf[BUFF_LEN]={0};

	buf_len = (size==0 || size>1400)? 1400:size;
	count = (num==0 || num>MAX_LOOP)? 10:num;

    if (0 == qcspi_ring_init_flag)
    {   
    	qapi_spi_ring_init();
        qcspi_ring_init_flag = 1;
    }

	qc_osal_tick_type_t time1 = qc_osal_get_ticks();
	demo_print("Start write RING %d\r\n",(int)time1);

	g_buffer.header.len = buf_len;
	g_buffer.header.flags = 0xa5;
	switch (seed%3){
		case 1:
			data1=0x37;
			break;
		case 2:
			data1=0xa5;
			break;
		default:
			data1=0x52;
			break;
	}
	for(i=0;i<buf_len;i++)
	{
		user_buf[i]= data1;
	}
	g_buffer.payload = user_buf;
	printf("APP tx add %x, tx len=%d data: %02x %02x %02x %02x \r\n", g_buffer.payload, g_buffer.header.len,
			g_buffer.payload[0], *(g_buffer.payload+1), *(g_buffer.payload+2), *(g_buffer.payload+3));



	for(i=0;i<count;i++)
	{
		//data1 = i%255;
		//memset(g_buffer.payload, data1, buf_len);
		if(tx_to_slave(1, 0, g_buffer.payload, buf_len))
		{
			printf("tx error cnt=%d \r\n",i);
			break;
		}

	}
	demo_print("send %d bytes time:[now]=%d,time1=%d,interval:%d ms\r\n", i*g_buffer.header.len,qc_osal_get_ticks(), time1, (qc_osal_get_ticks()-time1)*1000/configTICK_RATE_HZ);

//	qc_osal_thread_delete(NULL);
}
#endif

#ifdef DEMO_BASIC_RING_TEST

static void app_demo_tx(uint32_t num, uint32_t size)
{
	demo_print("app_demo tx\r\n");
	uint32_t j=0, k=0, count=0;
	uint8_t write_buf[1400]={0};
	uint16_t info_type;
	hfc_data_test_stats test_result={0};	
	uint16_t result_size=sizeof(hfc_data_test_stats);

	for(j=0;j<sizeof(write_buf);j++)
	{
		write_buf[j]=0xb7;
	}
	uint16_t buf_len = 0;
	uint8_t ring_id = F2A_DATA_RING_ID;
	uint8_t info = 0x0; //for future usage

    if (0 == qcspi_ring_init_flag)
    {   
    	qapi_spi_ring_init();
        qcspi_ring_init_flag = 1;
    }

	count = (num==0 || num>MAX_LOOP)? 10:num;
	buf_len = (size==0 || size>1400)? 1400:size;

	/* ======= Prepare========================== */
	hfc_data_test_ctrl ctrl_buf={0};

	ctrl_buf.hdr.msg_id = HFC_TEST_DATA_START;//HFC_TEST_DATA_START;
	ctrl_buf.mode = HFC_DATA_TEST_TX;

	if(qapi_spi_ring_write((uint8_t *)&ctrl_buf, sizeof(ctrl_buf), A2F_CONFIG_RING_ID, CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");

	demo_print("Start write RING [%d]:\r\n",count);
	{
		for(j=0;j<buf_len;j++)
			demo_print("%x ",write_buf[j]);
		demo_print("\r\n");
	}

	/* ======= END Prepare========================== */

	qc_osal_tick_type_t time1 = qc_osal_get_ticks();
	for(j=0;j<count;j++)
	{
		if(qapi_spi_ring_write(write_buf, buf_len, ring_id, HFC_TEST_DEMO_DATA))
			demo_print("qapi_spi_ring_write error\r\n");

	}


	demo_print("send %d bytes time:[now]=%d,time1=%d,interval:%d ms\r\n", j*buf_len,qc_osal_get_ticks(), time1, ((qc_osal_get_ticks()-time1)/configTICK_RATE_HZ))*1000;

	/* ======= get result ========================== */
	ctrl_buf.hdr.msg_id = HFC_TEST_DATA_END;
	ctrl_buf.mode = HFC_DATA_TEST_TX;

	if(qapi_spi_ring_write((uint8_t *)&ctrl_buf, sizeof(ctrl_buf), A2F_CONFIG_RING_ID, CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");

	if(qapi_spi_ring_read((uint8_t *)&test_result, &result_size, F2A_CONFIG_RING_ID, &info_type))
		demo_print("qapi_spi_ring_read error\r\n");

	demo_print("END: info_type=%d,type=%d,size=%d, mode=%d, recv_bytes=%d, send_bytes=%d\r\n", info_type, test_result.hdr.msg_id, result_size, test_result.mode,test_result.recv_bytes, test_result.send_bytes);
	/* ======= END ========================== */
}
extern qc_osal_semaphore_handle_t osSemaphore;
static void app_demo_rx(uint32_t num, uint32_t size, uint32_t int_flag)
{
	demo_print("app_demo rx\r\n");
	uint32_t j=0, retry=0, count=0;
	uint8_t recv_buf[1400]={0};
	uint16_t info_type;
	hfc_data_test_stats test_result={0};	
	uint16_t result_size=sizeof(hfc_data_test_stats);
	uint32_t ret;

	uint32_t buf_len = 0;
	uint8_t ring_id = F2A_DATA_RING_ID;
	uint8_t info = 0x0; //for future usage

    if (0 == qcspi_ring_init_flag)
    {    
    	qapi_spi_ring_init();
        qcspi_ring_init_flag = 1;
    }
	count = (num==0 || num>MAX_LOOP)? 10:num;
	buf_len = (size==0 || size>1400)? 1400:size;

	/* ======= Prepare========================== */
	hfc_data_test_ctrl ctrl_buf={0};

	ctrl_buf.hdr.msg_id = HFC_TEST_DATA_START;//HFC_TEST_DATA_START;
	ctrl_buf.mode = HFC_DATA_TEST_RX;

	{
		ctrl_buf.f2a_pkt_count = count;
		ctrl_buf.f2a_pkt_size = buf_len;
	}
	demo_print("prepare mode %d, count %d, size %d \r\n", ctrl_buf.mode, ctrl_buf.f2a_pkt_count, ctrl_buf.f2a_pkt_size);
	if(qapi_spi_ring_write((uint8_t *)&ctrl_buf, sizeof(ctrl_buf), A2F_CONFIG_RING_ID, CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");
	/* ======= END Prepare========================== */

	qc_osal_tick_type_t time1 = qc_osal_get_ticks();



	demo_print("Start read RING %d [%d] with mode=%d\r\n",time1,count,int_flag);
	if(int_flag == 0){
		for(j=0;j<count;j++)
		{

			/* in polling mode, qapi_spi_ring_read might get empty data as slave is not ready*/
			while(ret=qapi_spi_ring_read(recv_buf, &buf_len, ring_id, &info_type))
			{
				if(ret!=RING_EMPTY)
				{
					demo_print("qapi_spi_ring_read error, count=%d, ret=%d\r\n",j, ret);
					break;
				}
				retry++;
				qc_hal_delay(10);
				if(retry>200)
				{
					retry=0;
					demo_print(".");
					break;
				}
			}
			retry = 0;
			//qc_osal_task_delay(100/QC_OSAL_portTICK_PERIOD_MS);

		}
	}
	else // interrupt mode
	{
		int rx_num=0;
		while(rx_num<count)
		{
			if (qc_osal_semaphore_take(osSemaphore , QC_OSAL_osWaitForever) == QC_OSAL_osOK) {
				rx_num++;

				ret = qapi_spi_ring_read(recv_buf, &buf_len, ring_id, &info_type);
				if(ret){
					printf("qapi_spi_ring_read error, count=%d\r\n",j);
				}

				//demo_print("RX[%d]%x %x-%x %x\r\n",rx_num,recv_buf[0],recv_buf[1], recv_buf[buf_len-2],recv_buf[buf_len-1]);
			}
		}
		//demo_print("INT cnt=%d rx_num=%d ringid=%d \r\n", int_count, rx_num, ring_id);
	}
	demo_print("recv %d bytes time:[now]=%d,time1=%d,interval:%d ms\r\n", j*buf_len,qc_osal_get_ticks(), time1, (qc_osal_get_ticks()-time1)*1000/configTICK_RATE_HZ);
	{
		for(j=0;j<buf_len;j++)
			demo_print("%x ",recv_buf[j]);
		demo_print("\r\n");
	}

	/* ======= get result ========================== */
	ctrl_buf.hdr.msg_id = HFC_TEST_DATA_END;
	ctrl_buf.mode = HFC_DATA_TEST_RX;

	{
		ctrl_buf.f2a_pkt_count = count;
		ctrl_buf.f2a_pkt_size = buf_len;
	}

	if(qapi_spi_ring_write((uint8_t *)&ctrl_buf, sizeof(ctrl_buf), A2F_CONFIG_RING_ID, CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");
	qc_osal_task_delay(100/QC_OSAL_portTICK_PERIOD_MS);
	if(qapi_spi_ring_read((uint8_t *)&test_result, &result_size, F2A_CONFIG_RING_ID, &info_type))
		demo_print("qapi_spi_ring_read error result_size=%d\r\n",result_size);

	demo_print("END: info_type=%d,type=%d,size=%d, mode=%d, recv_bytes=%d, send_bytes=%d\r\n", info_type, test_result.hdr.msg_id, result_size, test_result.mode,test_result.recv_bytes, test_result.send_bytes);
	/* ======= END ========================== */
}

static void app_demo(uint32_t num, uint32_t size)
{
	demo_print("app_demo\r\n");
	uint32_t j=0, k=0, count=0;
	uint8_t recv_buf[1400]={0};
	uint8_t write_buf[1400]={0};
	uint16_t info_type;
	uint32_t ret;

	for(j=0;j<sizeof(write_buf);j++)
	{
		write_buf[j]=0xb7;
	}
	uint16_t buf_len = 0;
	uint8_t ring_id = F2A_DATA_RING_ID;
	uint8_t info = 0x0; //for future usage

    if (0 == qcspi_ring_init_flag)
    {
    	qapi_spi_ring_init();
        qcspi_ring_init_flag = 1;
    }

	qc_osal_tick_type_t time1 = qc_osal_get_ticks();

	count = (num==0 || num>MAX_LOOP)? 10:num;
	buf_len = (size==0 || size>1400)? 1400:size;

	demo_print("Start write RING %d [%d]\r\n",time1,count);
	for(j=0;j<count;j++)
	{
		if(qapi_spi_ring_write(write_buf, buf_len, ring_id, HFC_TEST_DEMO_DATA))
			demo_print("qapi_spi_ring_write error\r\n");
//		demo_print("write time cost: [now]=%d, time1=%d,  interval:%d ms\r\n", qc_osal_get_ticks(), time1, (qc_osal_get_ticks()-time1)*1000/configTICK_RATE_HZ);

		while(ret=qapi_spi_ring_read(recv_buf, &buf_len, ring_id, &info_type))
		{
			if(ret!=RING_EMPTY)
			{
				demo_print("qapi_spi_ring_read error, count=%d, ret=%d\r\n",j, ret);
				break;
			}
			//demo_print(".");
		}
//		qc_osal_task_delay(500/QC_OSAL_portTICK_PERIOD_MS);

		for(k=0;k<buf_len;k++)
		{
			if(write_buf[k]!=recv_buf[k])
			{
				demo_print("Unexpected result with read [%d]=%d, index[%d], w=[%x], r=[%x]\r\n",count,j,k,write_buf[k],recv_buf[k]);
				return;
			}
		}
	}


	demo_print("send %d bytes time:[now]=%d,time1=%d,interval:%d ms\r\n", j*buf_len,qc_osal_get_ticks(), time1, (qc_osal_get_ticks()-time1)*1000/configTICK_RATE_HZ);
	{
		for(j=0;j<buf_len;j++)
			demo_print("%x ",recv_buf[j]);
		demo_print("\r\n");
	}

//    qc_osal_thread_delete(NULL);
}

#endif
#ifdef DEMO_RAW_SPI_TEST
static void spiRxTask(void* argument)
{
	demo_print("Start spiRxTask.\r\n");
    while(1)
    {
        qc_hal_gpio_toggle(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin);
        qc_osal_task_delay(2000/QC_OSAL_portTICK_PERIOD_MS);
        qc_hal_gpio_toggle(QC_HAL_LED_BLUE_GPIO_Port, QC_HAL_LED_GREEN_Pin);
        qc_osal_task_delay(3000/QC_OSAL_portTICK_PERIOD_MS);
        demo_print("R ");

    }

    qc_osal_thread_delete(NULL);
}
static void spiTxTask(void* argument)
{
	demo_print("k Start spiTxTask.\r\n");

    while(1)
    {
        qc_hal_gpio_toggle(QC_HAL_LED_RED_GPIO_Port, QC_HAL_LED_RED_Pin);
        qc_osal_task_delay(3000/QC_OSAL_portTICK_PERIOD_MS);
        demo_print("T ");
    }

    qc_osal_thread_delete(NULL);
}

void spi_raw_test(uint32_t num, uint32_t size)
{
	struct device_id id = {0};
	uint32_t j;
	uint32_t count=0, buf_len=0;
	if (QAPI_OK != qapi_spi_get_slaveid(&id))
	{
	    demo_print("Slave Id=%x %x %x\r\n", id.array[0],id.array[1],id.array[2]);
		return;
	}

	#define ACCESS_LEN 1400
	#define ACCESS_ADDR	0x33830

	uint8_t buf[ACCESS_LEN]={0};
	uint8_t w_buf[ACCESS_LEN]={0};

	count = (num==0 || num>MAX_LOOP)? 10:num;
	buf_len = (size==0 || size>ACCESS_LEN)? 1400:size;

	demo_print("format write buffer for loop=%d, len=%d\r\n",count, buf_len);
	for(j=0;j<buf_len;j++)
		w_buf[j] = 0xa5;

	qc_osal_tick_type_t time1 = qc_osal_get_ticks();
	for(j=0;j<count/*TEST_CYCLE*/;j++)
	{
		SPI_Write(w_buf, buf_len, ACCESS_ADDR);

		SPI_Read(buf, buf_len, ACCESS_ADDR);
        if (memcmp(w_buf, buf, buf_len))
        {
            demo_print("There is a mismatch between received data and sent data.\r\n");
        }
	}
	demo_print("send %d bytes time:[now]=%d,time1=%d,interval:%d ms\r\n", j*buf_len,qc_osal_get_ticks(), time1, (qc_osal_get_ticks()-time1)*1000/configTICK_RATE_HZ);
	for(int i=0;i<buf_len;i++)
		demo_print("%x ",buf[i]);
	demo_print("\r\n");
//	qc_osal_thread_delete(NULL);

}
#endif


void SPI_taskInit(void)
{
	demo_print("new Start SPI_taskInit.\r\n");
	qapi_spi_init();
#ifdef DEMO_RAW_SPI_TEST
    qc_osal_thread_create(spiTxTask, "spiTxTask", 256+512, NULL, QC_OSAL_tskIDLE_PRIORITY + 3, NULL);
    qc_osal_thread_create(spiRxTask, "spiRxTask", 256, NULL, QC_OSAL_tskIDLE_PRIORITY + 4, NULL);
    qc_osal_thread_create(spi_raw_test, "spi_raw_test", 1024*4, NULL, QC_OSAL_tskIDLE_PRIORITY + 1, NULL);
//    spi_raw_test();
#endif
#ifdef DEMO_TXRX_TEST
    qc_osal_thread_create(transaction_task, "transaction_task", 1024, NULL, QC_OSAL_tskIDLE_PRIORITY + 6, NULL);
    qc_osal_thread_create(process_rx_task, "process_rx_task", 1024, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    qc_osal_thread_create(app_txrx_demo, "app_txrx_demo", 1024*4, NULL, QC_OSAL_tskIDLE_PRIORITY + 3, NULL);

//    app_txrx_demo(0);
#endif
#ifdef DEMO_BASIC_RING_TEST
    qc_osal_thread_create(app_demo, "app_demo", 1024*4, NULL, QC_OSAL_tskIDLE_PRIORITY + 1, NULL);
#endif
}
#ifdef SHELL_FEATURE

#define STACK_SIZE 3072

void demo_start_test(int mode)
{
	hfc_data_test_ctrl write_buf={0};
	uint8_t *p_buf;

	if (0 == qcspi_ring_init_flag)
	{		
		qapi_spi_ring_init();
		qcspi_ring_init_flag = 1;
	}

	write_buf.hdr.msg_id = HFC_TEST_DATA_START;//HFC_TEST_DATA_START;
	write_buf.mode = HFC_DATA_TEST_LOOP_BACK;//only demo loopback mode.

	p_buf = (uint8_t *)&write_buf;
	if(qapi_spi_ring_write(p_buf, sizeof(write_buf), A2F_CONFIG_RING_ID, CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");

}

void demo_stop_test(int mode)
{
	hfc_data_test_ctrl write_buf={0};
	uint16_t size=sizeof(hfc_data_test_stats);
	uint16_t info_type;
	hfc_data_test_stats read_buf={0};
	uint8_t *p_buf;

	write_buf.hdr.msg_id = HFC_TEST_DATA_END;
	write_buf.mode = mode;

	p_buf = (uint8_t *)&write_buf;
	if(qapi_spi_ring_write(p_buf, sizeof(write_buf), A2F_CONFIG_RING_ID,CONFIG_INFO_HFC))
		demo_print("qapi_spi_ring_write error\r\n");

	qc_osal_task_delay(100/QC_OSAL_portTICK_PERIOD_MS);
	p_buf =(uint8_t *)&read_buf;
	if(qapi_spi_ring_read(p_buf, &size, F2A_CONFIG_RING_ID, &info_type))
		demo_print("qapi_spi_ring_read error\r\n");

	demo_print("END: info_type=%d,type=%d,size=%d, mode=%d, recv_bytes=%d, send_bytes=%d\r\n", info_type, read_buf.hdr.msg_id, size, read_buf.mode,read_buf.recv_bytes, read_buf.send_bytes);
}

void demo_reset_test(int num)
{
	uint32_t val=0;
#if 1
#define PMU_SMPS1_5	0x0204304C
#define PMU_SMPS2_5	0x02043058
#define PMU_CORE_12 0x0204307c
//#define SMPS1_GDRV_OVR_OFFSET	22
#define SMPS1_GDRV_OVR_LOW	(2<<22)
#define SMPS1_GDRV_OVR_HIGH	(3<<22)
#define PMU_CORE_LDOAO_VIN_MODE_OFFSET (17)
#define PMU_CORE_LDOAO_VIN_MODE_VBATT (0)
	extern uint8_t reset_flag;
	reset_flag = 1;
	qc_hal_delay(50);
	qcspi_mutex_init();
	/* do SW WR for chip_on */
	SPI_Read((uint8_t *)&val, sizeof(val), PMU_CORE_12);
	demo_print("read pmu core %x\r\n", val);
	val &= 0xfff9ffff;
	demo_print("set pmu core %x\r\n", val);
	SPI_Write(&val, sizeof(val), PMU_CORE_12);
	SPI_Read((uint8_t *)&val, sizeof(val), PMU_CORE_12);
	demo_print("2 read pmu core %x\r\n", val);

	SPI_Read((uint8_t *)&val, sizeof(val), PMU_SMPS1_5);
	demo_print("read smps1-5 %x\r\n", val);
//	SPI_Read((uint8_t *)&val, sizeof(val), PMU_SMPS2_5);
//	demo_print("read smps2-5 %x\r\n", val);
//	val=0;
	val |= SMPS1_GDRV_OVR_LOW;
	demo_print("set smps1-5 %x\r\n", val);
	SPI_Write(&val, sizeof(val), PMU_SMPS1_5);
//	SPI_Write(&val, sizeof(val), PMU_SMPS2_5);

//	SPI_Read((uint8_t *)&val, sizeof(val), PMU_SMPS1_5);
//	demo_print("2:read smps1-5 %x\r\n", val);
#endif
	/* host toggle */
#define DELAY_TIMING	30
	qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_RESET); //chip_on test
	//qc_osal_task_delay(DELAY_TIMING/QC_OSAL_portTICK_PERIOD_MS);
	qc_hal_delay(DELAY_TIMING);
	qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_SET); //chip_on test
	reset_flag = 0;
	demo_print("END RESET\r\n");

}

void demo_wakeup_test(int num)
{
	uint32_t val=0;

	/* host toggle */
	if(num==1)
		qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_SET); //chip_on test
	else
		qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_RESET); //chip_on test

}

void demo_spi_write_test(uint32_t address, uint32_t value)
{
	uint32_t val=value;
	SPI_Write(&val, sizeof(val), address);
	demo_print("SPI write val=0x%x to address 0x%x done.\r\n", val, address);
}
void demo_spi_read_test(uint32_t address)
{
	uint32_t val=0;
	SPI_Read((uint8_t *)&val, sizeof(val), address);
	demo_print("SPI read val=0x%x from address 0x%x done.\r\n", val, address);
}

int test_qcspi_start(int argc, char ** argv){
	demo_print("test_qcspi_start arc=%d %s [%d]\r\n",argc, argv[1], atoi(argv[1]));
	demo_start_test(atoi(argv[1]));

	return 0;
}

int test_qcspi_stop(int argc, char ** argv){
	demo_print("demo_stop_test=%d %s [%d]\r\n",argc, argv[1], atoi(argv[1]));
	demo_stop_test(atoi(argv[1]));

	return 0;
}

int test_qcspi_reset(int argc, char ** argv){
//	demo_print("demo_reset_test=%d %s [%d]\r\n",argc, argv[1], atoi(argv[1]));
	demo_reset_test(atoi(argv[1]));

	return 0;
}

int test_wakeup_reset(int argc, char ** argv){
//	demo_print("demo_reset_test=%d %s [%d]\r\n",argc, argv[1], atoi(argv[1]));
	demo_wakeup_test(atoi(argv[1]));

	return 0;
}

int test_qcspi_raw(int argc, char ** argv){
//	demo_print("test_qcspi_raw arc=%d %s [%s]\r\n",argc, argv[1], argv[2]);
	int cnt=0,size=0;

	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);
	spi_raw_test(cnt, size);

	return 0;
}

int test_qcspi_tx(int argc, char ** argv){
	demo_print("basic spi write/read \r\n");
	int cnt=0,size=0;
	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);

	app_demo_tx(cnt, size);

	return 0;
}

int test_qcspi_rx(int argc, char ** argv){
	demo_print("basic spi read test \r\n");
	int cnt=0,size=0;
	int int_enable=0;
	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);
	if(argv[3])
		sscanf(argv[3],"%d",&int_enable);

	app_demo_rx(cnt, size, int_enable);

	return 0;
}

int test_qcspi_wr(int argc, char ** argv){
	demo_print("basic spi write/read \r\n");
	int cnt=0,size=0;
	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);

	app_demo(cnt, size);

	return 0;
}
int test_qcspi_wr_int(int argc, char ** argv){
	demo_print("spi w/r with interrupt\r\n");
	int cnt=0,size=0;

	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);

	app_txrx_demo(cnt, size);

	return 0;
}
int test_qcspi_write(int argc, char ** argv){
	uint32_t addr=0,val=0;

	if(argc<3)
	{
		demo_print("Miss parameters\r\n");
		return 0;
	}
	sscanf(argv[1],"%x",&addr);
	sscanf(argv[2],"%x",&val);
	demo_spi_write_test(addr, val);

	return 0;
}
int test_qcspi_read(int argc, char ** argv){
	uint32_t addr=0;
	if(argc<2)
	{
		demo_print("Miss parameters\r\n");
		return 0;
	}
	sscanf(argv[1],"%x",&addr);
	demo_spi_read_test(addr);

	return 0;
}
void vTaskShell(void * p) {
	cmd_shell_init(&(UART_DEVICE));
	cmd_shell_add("write", (void*)test_qcspi_write, "write [addr] [val]");
	cmd_shell_add("read", (void*)test_qcspi_read, "read [addr]");
	cmd_shell_add("start", (void*)test_qcspi_start, "start [mode=0/1/2]");

	cmd_shell_add("raw", (void*)test_qcspi_raw, "raw [cnt] [size]");

	cmd_shell_add("tx", (void*)test_qcspi_tx, "tx [cnt] [size]");
	cmd_shell_add("rx", (void*)test_qcspi_rx, "rx [cnt] [size]");
	cmd_shell_add("spi1", (void*)test_qcspi_wr, "qcspi [cnt] [size]");

	cmd_shell_add("spi2", (void*)test_qcspi_wr_int, "qcspi [cnt] [size] (INT gpio8)");

	cmd_shell_add("stop", (void*)test_qcspi_stop, "stop [mode=0/1/2]");
	cmd_shell_add("reset", (void*)test_qcspi_reset, "reset(chip_on pin wired)");
	cmd_shell_add("wk", (void*)test_wakeup_reset, "wakeup");

	while(1){
		cmd_shell_char_received();
	}
}

void task_init_all(void)
{
	QC_OSAL_BaseType_t xReturned;
	qc_osal_task_handle_t xHandle = NULL;

	qapi_spi_init();
	/* This will start ticks for OS based system */
	qc_hal_inc_tick();
	qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_SET); //chip_on test
#ifdef DEMO_TXRX_TEST
    qc_osal_thread_create(transaction_task, "transaction_task", 1024*3, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    qc_osal_task_delay(300/QC_OSAL_portTICK_PERIOD_MS);

    //qc_osal_thread_create(process_rx_task, "process_rx_task", 1024*3, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    //qc_osal_task_delay(300/QC_OSAL_portTICK_PERIOD_MS);
#endif

	xReturned = qc_osal_thread_create(
			vTaskShell,      	/* Function that implements the task. */
			"Shell",         	/* Text name for the task. */
			STACK_SIZE,      	/* Stack size in words, not bytes. */
			(void *) NULL,    	/* Parameter passed into the task. */
			1,					/* Priority at which the task is created. */
			&xHandle);      	/* Used to pass out the created task's handle. */
	if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
		printf("Task Shell creation error: Could not allocate required memory\r\n");
	}

}
#endif
#endif
