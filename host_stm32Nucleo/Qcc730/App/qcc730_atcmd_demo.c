/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "qc_os_al.h"
#include "qc_plat_al.h"
#ifdef SHELL_FEATURE
#include "shell.h"
#endif
#include "qapi_spi.h"
#include "qapi_atcmd.h"
#include "dfu.h"

#ifdef QCC730_ATCMD_ENABLE
#define AT_ARGC_MAX      128
#define AT_CMD_MAX_SIZE  16
#define ATCMD_PARSER_FUNC_LIST_MAX_SIZE 32
#define AT_RESPONSE_MAX  2048
#define demo_print printf
#define MAX_LOOP	1000000
#define TEST_BUFFER_SIZE        1400
#define ONE_GB_BYTES (1024UL*1024*1024UL)

int qat_demo_parser_enable = 1;
uint32_t qcspi_ring_init_flag = 0;
extern int qcc_state;

typedef struct{
	int task_started;
	int num;
	int len;
	int mqtt_recv_count;
	int mqtt_recv_timeout;
	uint32_t start_tick;
	uint32_t end_tick;
} atcmd_mqtt_rx_test_t;

typedef struct{
	uint32_t interval;
	uint32_t len;
	uint32_t time;
	uint32_t bytes;
	uint32_t g_bytes;
} atcmd_tx_test_t;

typedef struct{
	char * cmd;
	int (* func)(int argc, char ** argv, char *orig_cmd);
	char * description;
} atcmd_parser_func_t;

typedef enum{
	QAT_RESP_TYPE,
	QAT_EVT_TYPE
} atcmd_parser_type;

static int atcmd_parser_resp_func_list_size = 0;
static int atcmd_parser_evt_func_list_size = 0;

static atcmd_parser_func_t atcmd_parser_resp_func_list[ATCMD_PARSER_FUNC_LIST_MAX_SIZE];
static atcmd_parser_func_t atcmd_parser_evt_func_list[ATCMD_PARSER_FUNC_LIST_MAX_SIZE];

atcmd_mqtt_rx_test_t *rx_mqtt_test_param = NULL;
atcmd_tx_test_t *tx_test_param = NULL;
uint8_t tx_quit = 0;
uint8_t rx_quit = 0;
uint8_t tx_test_start = 0;

//http test
static int http_mode = -1;
static int http_send_num_temp = 0;
static int http_send_num = 0;
static int http_send_num_max = 10;
static int http_Range = 400;
static int http_send_data_num_max = 10;
static int http_send_data_num =0;
static int http_cmd_wait_time = 500;  //ms
static int http_data_wait_time = 500;  //ms

typedef enum {
	HTTP_TEST_MODEL_CHECK_DATA_LENGTH,
	HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE,
	HTTP_TEST_MODEL_CHECK_BIG_DATA,
	HTTP_TEST_MODEL_CHECK_BIG_DATA_ON_DATA_MODEL,
}http_mode_t;
/*--------------------------------------------------http test-------------------------------------------------------------
httptest cmd is like:httptest argv1 argv2 argv3 ...
         argv1: it's http_mode value, range [0-3]: 0 [CMD_MODEL]: send http get cmd and verify data length only. 
                                                   1 [CMD_MODEL]: send http get cmd and verify data length and end value.
                                                   2 [CMD_MODEL]: send big data to 730 when test http post/put.
                                                   3 [DATA_MODEL]: send big data to 730 when test http post/put. 
         argv2: when http_mode is 0, the range is [0-1], 0 means start with test_buf1, 1 means start with test_buf2
                when http_mode is 1, the range is [0-3], 0 means start with test_buf3, 1 means start with test_buf4, and so on...
                when http_mode is 2, it means wait time to send next test_buf after receiving the response ">" of last data
                when http_mode is 3, it means the time interval between two data transmissions
         argv3: try to test how many times, it means the times of sending http data test_buf on data model for one test_buf_cmd
         
         argv4: only use for http_mode 2,it means the time to wait, then send next cmd after receiving the response "ok" of last test_buf_cmd 
         argv5: only use for http_mode 2,it means the times to send test_buf_cmd.

   for example:
   [used for AT+HTTPCLIENT, get operation]:
                httptest 0 1 200: it means verify the data length, start with test_buf2, exect 200 times.
                httptest 1 0 200: it means verify the data length and end value, start with test_buf3, exect 200 times.
                
    [used for AT+HTTPPOST/AT+HTTPPUT]:
         [cmd model]:  httptest 2 5 9 10 20
                       it means send test_buf_cmd 20 times, wait 10ms to send next cmd after receiving the response "ok" of last test_buf_cmd
                       for one test_buf_cmd, it will send test_buf 9 times, wait 5ms to send next test_buf after receiving the response ">" of last data
         [data model]: httptest 3 1000 200
                       it means send httpdata test_buf 200 times on data model, the interval of 2 test_buf sent is 1000ms
----------------------------------------------------------------------------------------------------------------*/
char test_buf_cmd[] =   "AT+HTTPPOST=\"https://192.168.0.200/index.asp\",9000\r";
// test data, the length is 1000
char test_buf[] =   "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\r";

char test_buf1[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1200.txt\"\r";
char test_buf2[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1400.txt\"\r";
//Keep lengths consistent between the following Ranges with http_Range(400), also you can change http_Range value.
char test_buf3[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1600.txt\",\"Range:bytes=0-399\"\r";
char test_buf4[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1600.txt\",\"Range:bytes=400-799\"\r";
char test_buf5[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1600.txt\",\"Range:bytes=800-1199\"\r";
char test_buf6[] =   "AT+HTTPCLIENT=2,0,\"https://192.168.0.200/get_test_1600.txt\",\"Range:bytes=1200-1599\"\r";
//you can change the data of following test buffer, keep consistent with your testing data
char test_buf_end0[] =   "000";  // end value of the httpdata relative test_buf3
char test_buf_end1[] =   "111";  // end value of the httpdata relative test_buf4
char test_buf_end2[] =   "222";  // end value of the httpdata relative test_buf5
char test_buf_end3[] =   "333";  // end value of the httpdata relative test_buf6
/*-----------------------------------------------http test---------------------------------------------------------------*/


void qcc730_atcmd_send_handler(uint8_t *cmd, uint32_t len);

extern uint8_t dfu_boot_flag;

uint8_t atcmd_parser_exec_function_by_index(int argc, char * argv, char *original_cmd, int index, int flag)
{
	if(flag == QAT_RESP_TYPE)
		return atcmd_parser_resp_func_list[index].func(argc, argv, original_cmd);
	else if(flag == QAT_EVT_TYPE)
		return atcmd_parser_evt_func_list[index].func(argc, argv, original_cmd);
}

uint8_t atcmd_parser_find_command_index(int *index, char *header, int len, int flag)
{
	int i = 0;
	if(flag == QAT_RESP_TYPE)
	{
		for(i = 0 ; i < atcmd_parser_resp_func_list_size ; i++) {
			if (!strcmp(atcmd_parser_resp_func_list[i].cmd, header)) {
				*index = i;
				return 0;
			}
		}
	}
	else if(flag == QAT_EVT_TYPE)
	{
		for(i = 0 ; i < atcmd_parser_evt_func_list_size ; i++) {
			if (!strcmp(atcmd_parser_evt_func_list[i].cmd, header)) {
				*index = i;
				return 0;
			}
		}
	}
	return -1;
}

uint8_t atcmd_parser_func_add(char * cmd, int (* pfunc)(int argc, char ** argv), char * description, int flag) 
{
	if(flag == QAT_RESP_TYPE)
	{
		if (atcmd_parser_resp_func_list_size < ATCMD_PARSER_FUNC_LIST_MAX_SIZE) {
			atcmd_parser_resp_func_list[atcmd_parser_resp_func_list_size].cmd = cmd;
			atcmd_parser_resp_func_list[atcmd_parser_resp_func_list_size].func = pfunc;
			atcmd_parser_resp_func_list[atcmd_parser_resp_func_list_size].description = description;
			atcmd_parser_resp_func_list_size++;
			return 0;
		}
	}
	else if(flag == QAT_EVT_TYPE)
	{
		if (atcmd_parser_evt_func_list_size < ATCMD_PARSER_FUNC_LIST_MAX_SIZE) {
			atcmd_parser_evt_func_list[atcmd_parser_evt_func_list_size].cmd = cmd;
			atcmd_parser_evt_func_list[atcmd_parser_evt_func_list_size].func = pfunc;
			atcmd_parser_evt_func_list[atcmd_parser_evt_func_list_size].description = description;
			atcmd_parser_evt_func_list_size++;
			return 0;
		}
	}

	return -1;
}

void atcmd_help(int argc, char ** argv)
{
    printf("AT+CMD?\r\n");
}

void atcmd_enable_parser(int argc, char ** argv)
{
	if(argc > 2)
	{
		printf("usage: parser enable|disable\r\n");
		return;
	}

	if(strncmp(argv[1], "enable", 6) == 0)
    {
		printf("response parser enabled\r\n");
		qat_demo_parser_enable = 1;
	}
	else if(strncmp(argv[1], "disable", 7) == 0)
	{
		printf("response parser disabled\r\n");
		qat_demo_parser_enable = 0;
	}
	else
	{
		printf("usage: parser enable|disable\r\n");
	}
	return;
}

uint32_t qatperf_duration_ms = 0;
uint32_t qatperf_pkt_len = AT_CMD_MAX_SIZE;

void atcmd_qat_perf(int argc, char ** argv)
{
	uint8_t index = 0;
	uint32_t speed = 0;
	uint32_t qatperf_duration_ms = 1000;
	uint32_t qatperf_pkt_len = 1400;
	uint32_t qatperf_bytes_sent = 0;
	uint8_t *qatperf_data = NULL;
	
	if (argc < 3)
    {
        printf("\nUsage: qatperf [options]\n");
        printf("  -l = The length of buffers to read or write\n");
        printf("  -t = The time in seconds to transmit for\n");					

        return;
    }

	for (index=0; index < argc; index++)
    {
        if (0 == strcmp(argv[index], "-t"))
        {
            index++;
            qatperf_duration_ms = atoi(argv[index]);
            index++;		
        }
	}
	for (index=0; index < argc; index++)
    {
    	if (0 == strcmp(argv[index], "-l"))
        {
			index++;
			qatperf_pkt_len = atoi(argv[index]);
			index++;		
        }
	}
	printf("qatperf started with data length = %d and duration = %d(ms)\r\n", qatperf_pkt_len, qatperf_duration_ms);
	
	qatperf_data = (uint8_t *)malloc(qatperf_pkt_len);
	
	srand(time(NULL));
	
	for (int i=0; i<qatperf_pkt_len; i++)
	{
		qatperf_data[i]=(rand()%100)+1;
	}
	
	qcc730_atcmd_ring_init();

	qc_osal_tick_type_t start = qc_osal_get_ticks();

	qc_osal_tick_type_t end = qc_osal_get_ticks();
	
	while(end - start <= qatperf_duration_ms)
	{
		qapi_atcmd_send(qatperf_data, qatperf_pkt_len);
		qatperf_bytes_sent += qatperf_pkt_len;
		end = qc_osal_get_ticks();
	}

	
	
	printf("sent done\r\n");
	printf("duration : %d(ms), take %d ticks\r\n", qatperf_duration_ms, end-start);
	printf("total sent data len : %d(bytes)\r\n",qatperf_bytes_sent);
	
	speed = qatperf_bytes_sent*8*1000 / qatperf_duration_ms;
	
	printf("speed : %d(bits/sec)\r\n",speed);

	free(qatperf_data);
	
	return;
}

static void atcmd_demo_tx(uint32_t num, uint32_t size)
 {
     uint32_t j=0, count=0;
     uint16_t info_type;
     TickType_t currentTick = 0;
     TickType_t endTick = 0;

     tx_test_start = 1;

     srand(time(NULL));

     count = (num==0 || num>MAX_LOOP)? 10:num;
     uint16_t buf_len = size;

     uint8_t *write_buf = (uint8_t *)malloc(buf_len);
     if (write_buf == NULL)
     {
         printf("malloc failed\r\n");
         tx_test_start = 0;
         return;
     }

     for(j=0; j<buf_len; j++)
     {
         write_buf[j] = (rand()%100)+1;
     }

     if (0 == qcspi_ring_init_flag)
     {
         qapi_spi_ring_init();
         qcspi_ring_init_flag = 1;
     }

     qc_osal_tick_type_t start = qc_osal_get_ticks();

     for(j=0; j<count; j++)
     {
         if(qapi_atcmd_send(write_buf, buf_len))
             printf("qapi_spi_ring_write error\r\n");
     }

     qc_osal_tick_type_t end = qc_osal_get_ticks();

     printf("tx speed = %d(bits/sec)\r\n", (count*buf_len*8)*1000/(end-start));
     printf("send %d bits time:[now]=%d,start=%d, take ticks = %d, interval:%d ms\r\n",
            count*buf_len*8, end, start, end-start,
            ((end-start)*1000/configTICK_RATE_HZ));

     free(write_buf);
     tx_test_start = 0;
 }

uint32_t obtain_mqtt_pub_recv_count(void)
{
	return rx_mqtt_test_param->mqtt_recv_count;
}
static void atcmd_demo_rx_mqtt(void)
{
	uint32_t j=0, count=0, start=0, received=0;
	uint16_t num = 0, len=0;
	uint32_t diff = 0;
	uint32_t bits = 0;
	uint32_t result = 0;
	//uint32_t timeout = 10000;

	//uint32_t end_timeout = qc_osal_get_ticks();
	
	num = (uint16_t)rx_mqtt_test_param->num;
	len = (uint16_t)rx_mqtt_test_param->len;
	
	//if(rx_mqtt_test_param->mqtt_recv_timeout > 0)	
	//	timeout = (uint32_t)rx_mqtt_test_param->mqtt_recv_timeout;
	
	rx_mqtt_test_param->task_started = 1;
	rx_mqtt_test_param->start_tick = 0;

	start = obtain_mqtt_pub_recv_count();
	
	//demo_print("wait %d(ms) to receive %d packets\r\n", timeout, num);
	demo_print("wait to receive %d packets\r\n", num);
	
	while(received < num)
	{
		received = obtain_mqtt_pub_recv_count() - start;

		/*
		if(rx_mqtt_test_param->start_tick > 0)
			end_timeout = qc_osal_get_ticks();
		
		if(end_timeout - rx_mqtt_test_param->start_tick > 0)
		{
			if( (end_timeout - rx_mqtt_test_param->start_tick > timeout) 
			&& (rx_mqtt_test_param->mqtt_recv_count > 0))
			{
				demo_print("rx_mqtt_test_param->mqtt_recv_count = %d\r\n", rx_mqtt_test_param->mqtt_recv_count);
				demo_print("end_timeout = %d\r\n", end_timeout);
				demo_print("rx_mqtt_test_param->start_tick = %d\r\n", rx_mqtt_test_param->start_tick);
				demo_print("timeout, packet loss may happened\r\n");
				break;
			}
		}
		*/
		
		if(rx_mqtt_test_param->task_started == 0)
		{
			demo_print("test running but stopped by cmd\r\n");
			break;
		}
	}
	
	rx_mqtt_test_param->end_tick = qc_osal_get_ticks();	
	diff = rx_mqtt_test_param->end_tick-rx_mqtt_test_param->start_tick;
	rx_mqtt_test_param->task_started = 0;
    bits = received*len*8;
	result = bits*1000/diff;
	
	demo_print("receive %d packets\r\n", received);
	demo_print("rx test done\r\n");
	demo_print("rx speed = %d(bits/sec)\r\n", result);
	qc_osal_thread_delete(NULL);
}

static void atcmd_demo_tx_loop(void)
{
	uint8_t *write_buf = malloc(tx_test_param->len);
	uint32_t j = 0, packets_num_gb = 0, packets_num_b = 0, packets_num_per_10s = 0, runtime = 0, runtime_per_10s = 0, count_per_10s = 0;
	uint32_t is_test_done = 0;

	qc_osal_tick_type_t start, start_per_10s, backoff_start;
	qc_osal_tick_type_t end, end_per_10s;

	tx_quit = 0;
	tx_test_start = 1;

	srand(time(NULL));

	for(j = 0; j < tx_test_param->len; j++)
	{
		write_buf[j]=(rand()%100)+1;
	}

    if (0 == qcspi_ring_init_flag)
    {
        qapi_spi_ring_init();
        qcspi_ring_init_flag = 1;
    }

	start = qc_osal_get_ticks();
	start_per_10s = qc_osal_get_ticks();
	while (!is_test_done) {
		if (tx_quit) {
			is_test_done = 1;
			break;
		}

		end = qc_osal_get_ticks();
		runtime = (end-start)/configTICK_RATE_HZ;
		if ((tx_test_param->time > 0) && runtime*1000 >= tx_test_param->time) {
			is_test_done = 1;
			break;
		}

		// ---Data transmission ---
		if(qapi_atcmd_send(write_buf, tx_test_param->len)) {
			printf("qapi_spi_ring_write error\r\n");
		} else {
			packets_num_b += tx_test_param->len;
			packets_num_per_10s += tx_test_param->len;

			// ---Handle GB rollover ---
			if(packets_num_b >= ONE_GB_BYTES){
				packets_num_gb++;
				packets_num_b -= ONE_GB_BYTES;
			}
		}

		if(tx_test_param->g_bytes > 0 || tx_test_param->bytes > 0){ // check only if a target is set
			if(packets_num_gb > tx_test_param->g_bytes){
				is_test_done = 1;
				break;
			} else if(packets_num_gb == tx_test_param->g_bytes){
				if(packets_num_b >= tx_test_param->bytes){
					is_test_done = 1;
					break;
				}
			}
		}

		end_per_10s = qc_osal_get_ticks();
		runtime_per_10s = (end_per_10s-start_per_10s)/configTICK_RATE_HZ;

		if(runtime_per_10s >= 10){
			printf("tx speed = %d(bits/sec) for %ds-%ds\r\n", (packets_num_per_10s * 8)/runtime_per_10s, count_per_10s, count_per_10s+10);
			printf("total bytes = %dG, %dB\r\n", packets_num_gb, packets_num_b);
			count_per_10s+=10;
			runtime_per_10s = 0;
			packets_num_per_10s = 0;
			end_per_10s = qc_osal_get_ticks();
			start_per_10s = qc_osal_get_ticks();
		}

		//delay if specified
		if (tx_test_param->interval) {
			qc_osal_task_delay(tx_test_param->interval/QC_OSAL_portTICK_PERIOD_MS);
		}
	}

	tx_test_start = 0;

	printf("tx speed = %d(bits/sec) for %ds-%ds\r\n", (packets_num_per_10s * 8)/runtime_per_10s, count_per_10s, count_per_10s+runtime_per_10s);

	memset(tx_test_param, 0, sizeof(atcmd_tx_test_t));

	free(write_buf);

	printf("delete thread\r\n");

	qc_osal_thread_delete(NULL);

}

void atcmd_response_parser_RDMEM(int argc, uint32_t ** argv, char *orig_cmd)
{
	printf("Response after parsing +RDMEM:\r\n");
	if(argc == 4)
	{
		printf("+RDMEM\r\n");
		printf("addr = %s\r\n", argv[0]);
		printf("size = %s\r\n", argv[1]);
		printf("value(hex) = %s\r\n", argv[2]);
		printf("value(dec) = %s\r\n", argv[3]);
	}
	else
	{
		printf("\r\n");
		printf("No need to parse, original response:\r\n");
		printf("%s\r\n",orig_cmd);
	}
}

void atcmd_response_parser_IPDHEX(int argc, uint32_t ** argv, char *orig_cmd)
{
	int i = 0;
	uint8_t *buf = NULL;
	uint32_t buf_len = 0;
	
	if(argc == 5)
	{
		buf_len = atoi(argv[3]);
		if(buf_len != 0)
		{
			buf = (uint8_t*)malloc(buf_len);
			memset(buf, 0, buf_len);
			memcpy(buf, argv[4], buf_len);
			
			printf("+IPDHEX:%s,%s,%d,%d,",argv[0],argv[1],atoi(argv[2]),atoi(argv[3]));
			
			for(i = 0; i<buf_len;i++)
				printf("%x",*buf++);

			free(buf);
		}
	}
	else
		printf("%s",orig_cmd);
	
	

}

void atcmd_response_parser_CIPDHCPV4C(int argc, uint32_t ** argv, char *orig_cmd)
{
	printf("Response after parsing +CIPDHCPV4C:\r\n");
	if(argc == 5)
	{
		printf("+CIPDHCPV4C\r\n");
		printf("IP = %s\r\n", argv[0]);
		printf("Gateway = %s\r\n", argv[1]);
		printf("Mask = %s\r\n", argv[2]);
		printf("DNS0 = %s\r\n", argv[3]);
		printf("DNS1 = %s\r\n", argv[4]);
	}
	else
	{
		printf("\r\n");
		printf("No need to parse, original response:\r\n");
		printf("%s\r\n",orig_cmd);
	}
}

void atcmd_response_parser_EVT_MQTTSUBRECV(int argc, uint32_t ** argv, char *orig_cmd)
{
	//printf("%s \r\n",orig_cmd);
	rx_mqtt_test_param->mqtt_recv_count++;

	if(rx_mqtt_test_param->mqtt_recv_count == 1)
		rx_mqtt_test_param->start_tick = qc_osal_get_ticks();
		
}
void atcmd_response_parser_EVT_OTAFWUP_FIN(int argc, uint32_t ** argv, char *orig_cmd)
{
	printf("OTAFWUP_FIN received, host will reset spi after 5s\r\n");
	qc_osal_task_delay(5000/QC_OSAL_portTICK_PERIOD_MS);
	//reset spi state
	atcmd_set_spi_state(QCC730_SPI_NOT_READY);
}
void atcmd_response_parser_EVT_MQTTSUBRECVHEX(int argc, uint32_t ** argv, char *orig_cmd)
{
	int i = 0;
	uint8_t *buf = NULL;
	uint32_t buf_len = 0;
	
	if(argc == 4)
	{
		buf_len = atoi(argv[2]);
		if(buf_len != 0)
		{
			buf = (uint8_t*)malloc(buf_len);
			memset(buf, 0, buf_len);
			memcpy(buf, argv[3], buf_len);
			
			printf("+EVT:MQTT_SUBRECVHEX:%s,%s,%s,",argv[0],argv[1],argv[2]);
			for(i = 0; i<buf_len;i++)
				printf("%x",*buf++);

			free(buf);
		}
	}
	else
		printf("%s",orig_cmd);
		
}


uint8_t atcmd_response_handler(uint16_t buf_len, char * cmd) {
	int argc;
	uint32_t * argv[AT_ARGC_MAX]={0};
	char original_cmd[AT_RESPONSE_MAX]={0};
	char *p;
	int comma_found=0;
	int evt_found=0;
	int func_found=0;
	// Separating header and arguments
	char header[AT_CMD_MAX_SIZE] = "";
	int i = 0;
	int h = 0;
	int arg_start = 0;
	int flag = QAT_RESP_TYPE;

	for (i=0;i<buf_len;i++)
	{
		if (cmd[i] == '\r' || cmd[i] == '\0' || cmd[i] == '\n')
		{
			continue;
		}
		if (cmd[i] == ':')
		{
			if(!strcmp(header, "+EVT"))
			{
				flag = QAT_EVT_TYPE;
				;
			}
			else
				break;
		}
		header[h] = cmd[i];
		h++;
	}
	header[h] = '\0';
	arg_start = i;
	// Searching for the command and parameters
	if(h != 0 && 0 == atcmd_parser_find_command_index(&i, header, h, flag))
	{

		memset(original_cmd, 0, AT_RESPONSE_MAX);
		memcpy(original_cmd, cmd, buf_len);

		argc = 1;
		cmd += arg_start+1;
		argv[0] = cmd;
		func_found = 1;
		for(p = cmd ; *p != '\0' && argc < AT_ARGC_MAX ; p++)
		{
			if(*p == '\r')
			{
				if(*(p+1) == '\n')
				{
					*p = '\0';
					p++;
				}
				*p = '\0';
				argv[argc] = p+1;
			}
			else if(*p == ',') {
				*p = '\0';
				argv[argc] = p+1;
				argc++;
			}
			else {
				;
			}
		}
		
		return atcmd_parser_exec_function_by_index(argc, argv, original_cmd, i, flag);
	}
	
	return -1;
}

void qcc730_atcmd_ring_init(void)
{
	struct device_id id = {0};
	int32_t  ret = 0;
	
    if (QCC730_SPI_NOT_READY == qapi_atcmd_get_spi_state())
    {   
	    if (QAPI_OK != qapi_spi_get_slaveid(&id))
	    {
    	    printf("qcspi init failed!\r\n");
			return;	        
	    }
			
    	if (0 != qapi_spi_ring_init())
    	{
    	    printf("qcspi ring init failed!\r\n");
			return;
    	}
    }
}

void qcc730_atcmd_send_handler(uint8_t *cmd, uint32_t len)
{
	struct device_id id = {0};
    uint16_t buf_len = ATCMD_BUF_LEN;	
	int32_t  ret = 0;
	
	if (NULL == cmd || 1 >= len )
    {
        return;
    }
#ifdef QCC730_DFU_SUPPORT
	if(TARGET_IN_SBL == qcc_state)
	{
		printf("\r\nQCC in SBL!\r\n");
		return;
	}
#endif
	if(strncasecmp(cmd, "AT+RST", 6) == 0)
	{

	    //reset spi state
	    atcmd_set_spi_state(QCC730_SPI_NOT_READY);
#ifdef QCC730_DFU_SUPPORT
	    qcc_reset_flag = 1;
	    qcc_state = TARGET_IN_SBL;
#endif
	}

#ifdef QCC730_DFU_SUPPORT
	if(dfu_boot_flag == 1)
	{
		qapi_spi_init();	
		qapi_spi_ring_init();

		dfu_boot_flag = 0;
	}
#endif

    if (QCC730_SPI_NOT_READY == qapi_atcmd_get_spi_state())
    {   
	    if (QAPI_OK != qapi_spi_get_slaveid(&id))
	    {
    	    printf("qcspi init failed!\r\n");
			return;	        
	    }
			
    	if (0 != qapi_spi_ring_init())
    	{
    	    printf("qcspi ring init failed!\r\n");
			return;
    	}
    }

    qapi_atcmd_send(cmd, len);
}

int test_atcmd_tx_loop(int argc, char ** argv){
	QC_OSAL_BaseType_t xReturned;
	qc_osal_task_handle_t xHandle = NULL;
	int index = 1;
	int interval, length, time, bytes = 0;
	if (argc < 2) {
		demo_print("\nUsage: tx_loop [options]\r\n");
		demo_print("  -i  = Sets the interval time in microseconds between periodic bandwidth\r\n");
		demo_print("  -l = The length of buffers to read or write\r\n");
		demo_print("  -t = The time in seconds to transmit for\r\n");
		demo_print("  -m = The MB number to transmit, no more than 1024MB\r\n");
		demo_print("  -g = The GB number to transmit\r\n");
		demo_print("  stop = stop the transmit test\r\n");
		return 0;
	}
	while (index < argc) {
		if (0 == strcmp(argv[index], "-i")) {
			sscanf(argv[++index],"%d",&tx_test_param->interval);
			index++;
		} else if (0 == strcmp(argv[index], "-l")) {
			sscanf(argv[++index],"%d",&tx_test_param->len);
			index++;
		} else if (0 == strcmp(argv[index], "-t")) {
			sscanf(argv[++index],"%d",&tx_test_param->time);
			tx_test_param->time = tx_test_param->time * 1000;
			index++;
		} else if (0 == strcmp(argv[index], "-m")) {
			sscanf(argv[++index],"%d",&tx_test_param->bytes);
			if(tx_test_param->bytes >= 1024){
				demo_print("value larger than 1024 is not allowed -m, please use -g instead \r\n");
				return 0;
			}
			tx_test_param->bytes = tx_test_param->bytes * 1024 * 1024;
			index++;
		} else if (0 == strcmp(argv[index], "-g")) {
			sscanf(argv[++index],"%d",&tx_test_param->g_bytes);
			index++;
		} else if (0 == strcmp(argv[index], "stop")) {
			tx_quit = 1;
			rx_quit = 1;
			return 0;
		} else {
			index++;
		}
	}

	xReturned = qc_osal_thread_create(atcmd_demo_tx_loop, "atcmd_demo_tx_loop", 1024, NULL, 6, &xHandle);

	if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
		printf("Task Shell creation error: Could not allocate required memory\r\n");
	}

	return 0;
}

int test_atcmd_tx(int argc, char ** argv){
	demo_print("tx test \r\n");
	int cnt=0,size=0;
	sscanf(argv[1],"%d",&cnt);
	sscanf(argv[2],"%d",&size);

	atcmd_demo_tx(cnt, size);

	return 0;
}

int test_atcmd_rx_mqtt(int argc, char ** argv)
{
	int cnt=0, len=0;
	QC_OSAL_BaseType_t xReturned;
	qc_osal_task_handle_t xHandle = NULL;

	if(qat_demo_parser_enable == 0)
	{
		demo_print("\r\nParser should be enabled before doing rx mqtt test!\r\n");
		return 0;
	}
	if(strncmp(argv[1], "stop", 4) == 0)
	{
		demo_print("\r\nMQTT rx test stop\r\n");
		rx_mqtt_test_param->task_started = 0;
	}
	else
	{
		demo_print("\r\nMQTT rx test start\r\n");
		
		memset(rx_mqtt_test_param, 0, sizeof(atcmd_mqtt_rx_test_t));
	
		sscanf(argv[1],"%d",&rx_mqtt_test_param->num);
	    sscanf(argv[2],"%d",&rx_mqtt_test_param->len);
		
		if(argv[3] != NULL)
			sscanf(argv[3],"%d",&rx_mqtt_test_param->mqtt_recv_timeout);
		
		xReturned = qc_osal_thread_create( atcmd_demo_rx_mqtt, "atcmd_demo_rx_mqtt", 1024, NULL, 1, &xHandle);
		
		if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
			printf("Task Shell creation error: Could not allocate required memory\r\n");
		}
	}
	
	return 0;
}

int test_send_http_cmd(int index){

    //demo_print("send http cmd mode:%d,index:%d\r\n",http_mode,index);
    if(http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH)
    {
       if(index%2 == 0)
	   {
		   qcc730_atcmd_send_handler(test_buf1, strlen(test_buf1));
		   http_send_num_temp =0;
	   }
	   else
	   {
		   qcc730_atcmd_send_handler(test_buf2, strlen(test_buf2));
		   http_send_num_temp =1;
	   }
    }
    else if(http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE)
    {
       if(index%4 == 0)
	   {
		   qcc730_atcmd_send_handler(test_buf3, strlen(test_buf3));
           http_send_num_temp =0;
	   }
	   else if(index%4 == 1)
	   {
		   qcc730_atcmd_send_handler(test_buf4, strlen(test_buf4));
		   http_send_num_temp =1;
	   }
	   else if(index%4 == 2)
	   {
		   qcc730_atcmd_send_handler(test_buf5, strlen(test_buf5));
		   http_send_num_temp =2;
	   }
	   else if(index%4 == 3)
	   {
		   qcc730_atcmd_send_handler(test_buf6, strlen(test_buf6));
		   http_send_num_temp =3;
	   }
    }
    else if(http_mode == HTTP_TEST_MODEL_CHECK_BIG_DATA)
    {
         qcc730_atcmd_send_handler(test_buf_cmd, strlen(test_buf_cmd));
    }

	return 0;
}

void test_http_process(uint8_t *data, uint32_t len){
    int datalen = strlen(data);
    if(len >=http_Range)
    {
        if(len != datalen)
        {
            demo_print("ERROR len %d is wrong!\r\n",datalen);
        }

        if((http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE) && (len == datalen))
        {
          int check_len = 0;
          if(http_send_num_temp%4 ==0)
          {
             check_len = strlen(test_buf_end0);
             if(check_len <= http_Range)
             {
                if((strncmp(data+http_Range-check_len, test_buf_end0, check_len) != 0))
        	        demo_print("ERROR data1 is wrong!\r\n");
             }
             else
             {
                    demo_print("warning check data is invalid!\r\n");
             }
          }
          else if(http_send_num_temp%4 ==1)
          {
             check_len = strlen(test_buf_end1);
             if(check_len <= http_Range)
             {
        		 if((strncmp(data+http_Range-check_len, test_buf_end1, check_len) != 0))
        		     demo_print("ERROR data2 is wrong!\r\n");
             }
             else
             {
                    demo_print("warning check data is invalid!\r\n");
             }
          }
          else if(http_send_num_temp%4 ==2)
          {
             check_len = strlen(test_buf_end2);
             if(check_len <= http_Range)
             {
        		 if((strncmp(data+http_Range-check_len, test_buf_end2, check_len) != 0))
        			 demo_print("ERROR data3 is wrong!\r\n"); 
             }
             else
             {
                    demo_print("warning check data is invalid!\r\n");
             }
          }
          else if(http_send_num_temp%4 ==3)
          {
             check_len = strlen(test_buf_end3);
             if(check_len <= http_Range)
             {
        		 if((strncmp(data+http_Range-check_len, test_buf_end3, check_len) != 0))
        			demo_print("ERROR data4 is wrong!\r\n");
             }
             else
             {
                    demo_print("warning check data is invalid!\r\n");
             }
          }
        }
    }

    if(http_mode == HTTP_TEST_MODEL_CHECK_BIG_DATA && len <50)
    {
        if(strncmp(data, ">", 1) == 0)
        {
         if(http_send_data_num<http_send_data_num_max)
         {
             //demo_print("try data, http_mode: %d,send_num_max: %d,send_num_total:%d,data send_num_max: %d,data send_num: %d,\r\n",
             //http_mode,http_send_num_max,http_send_num,http_send_data_num_max,http_send_data_num);
             qc_hal_delay(http_data_wait_time);
             qcc730_atcmd_send_handler(test_buf, strlen(test_buf));
             http_send_data_num++;
         }

        }   
        if(strncmp(data, "+HTTPC: size", 12) == 0)
        {
            printf("%s\r\n", data);
        }

        if(strncmp(data+2, "+HTTPCPOST: SEND OK", 19) == 0 || strncmp(data+2, "+HTTPCPUT: SEND OK", 18) == 0 || strncmp(data+2, "+HTTPCPOST: SEND FAIL, timeout", 30) == 0)
        {
            http_send_data_num = 0;
            if(http_send_num <http_send_num_max)
            {
                printf("%s\r\n", data);

                qc_hal_delay(http_cmd_wait_time);

                //demo_print("try cmd, http_mode:%d,send_cmd_max:%d,send_num:%d\r\n",
                //http_mode,http_send_num_max,http_send_num+1);

                test_send_http_cmd(0);
                http_send_num++;
            }
            else
			{
				http_mode = -1;
                printf("%s\r\n", data);
			}
        }
    }
     
    if((http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH|| http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE)
      && len < 10 && (strncmp(data+2, "OK", 2) == 0))
    {
       if(http_send_num <http_send_num_max)
       {
           qc_hal_delay(http_cmd_wait_time);
           
           demo_print("try next, http_mode: %d,send_num: %d, send_num_total:%d, send_num_max: %d\r\n",
            http_mode,http_send_num_temp+1,http_send_num,http_send_num_max);
           
    	   test_send_http_cmd(http_send_num_temp+1);
    	   http_send_num++;

       }
       else
       {
    	   http_mode = -1;
           printf("%s\r\n", data);
       }
    }

}


int test_http(int argc, char ** argv){
    int num = 0;
    int interval = http_data_wait_time;
    http_mode = -1;
    http_send_num_temp = 0;
    http_send_num =0;
    http_send_data_num =0;

    if(argc>1)
    {
        http_mode = atoi(argv[1]);
    }

    if(http_mode==HTTP_TEST_MODEL_CHECK_DATA_LENGTH || http_mode==HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE )
    {
        if(argc>2)
        {
            num = atoi(argv[2]);
        }
        if(argc>3)
        {
            http_send_num_max = atoi(argv[3]);
        }

        demo_print("httptest test_mode:%d,send_num_max:%d\r\n",http_mode,http_send_num_max);
        if(http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH)
        {
            if(num == 0)
            {
               qcc730_atcmd_send_handler(test_buf1, strlen(test_buf1));
            }
            else
            {
               qcc730_atcmd_send_handler(test_buf2, strlen(test_buf2));
            }
        }
        else if(http_mode == HTTP_TEST_MODEL_CHECK_DATA_LENGTH_AND_VALUE)
        {
           if(num == 0)
           {
               qcc730_atcmd_send_handler(test_buf3, strlen(test_buf3));
               http_send_num_temp =0;
           }
           else if(num == 1)
           {
                qcc730_atcmd_send_handler(test_buf4, strlen(test_buf4));
                http_send_num_temp =1;
           }
           else if(num == 2)
           {
                qcc730_atcmd_send_handler(test_buf5, strlen(test_buf5));
                http_send_num_temp =2;
           }
           else if(num == 3)
           {
                qcc730_atcmd_send_handler(test_buf6, strlen(test_buf6));
                http_send_num_temp =3;
              }
        }
        http_send_num ++;
    }
    else if(http_mode==HTTP_TEST_MODEL_CHECK_BIG_DATA )
    {
        if(argc>2)
        {
            http_data_wait_time = atoi(argv[2]);
        }
        if(argc>3)
        {
            http_send_data_num_max = atoi(argv[3]);
        }
        if(argc>4)
        {
            http_cmd_wait_time = atoi(argv[4]);
        }
        if(argc>5)
        {
            http_send_num_max= atoi(argv[5]);
        }

        if(strlen(test_buf_cmd)<= TEST_BUFFER_SIZE)
        {
            demo_print("httptest test_mode:%d, send_cmd_max:%d, cmd wait time:%dms, data_max:%d,data wait time:%dms, send_num:%d\r\n",
                http_mode,http_send_num_max,http_cmd_wait_time,http_send_data_num_max,http_data_wait_time,http_send_num+1);
            qcc730_atcmd_send_handler(test_buf_cmd, strlen(test_buf_cmd));
            http_send_num ++;
        }
        else
        {
            demo_print("error, test_buf_cmd lent:%d, it should not be bigger than %d\r\n",strlen(test_buf)-1,TEST_BUFFER_SIZE);
        }
    }
    else if(http_mode==HTTP_TEST_MODEL_CHECK_BIG_DATA_ON_DATA_MODEL )
    {
        if(argc>1)
        {
            interval = atoi(argv[2]);
        }
        if(argc>2)
        {
            http_send_num_max = atoi(argv[3]);
        }

        demo_print("httptest test_mode:%d, send_num_max:%d, test_buf lent:%d, interval:%dms\r\n",http_mode,http_send_num_max,strlen(test_buf)-1,interval);

        for(int i=0; i<http_send_num_max; i++)
        {
            demo_print("httptest num:%d\r\n",i+1);
            qcc730_atcmd_send_handler(test_buf, strlen(test_buf));

            if(i < http_send_num_max  -1)
              qc_hal_delay(interval);
        }
        
        http_mode = -1;
    }

	return 0;
}


void print_atcmd_resp(uint8_t *data, uint32_t len)
{
    if (!strncmp(&data[2], "+CMD:", 5))
    {
        /* Only Work around for Uart and SPI conflict on STM32 platform under some conditiontion*/
        qc_osal_task_delay(1000/QC_OSAL_portTICK_PERIOD_MS);
		printf("%s", data);	
		return;
    }
	if (!strncmp(&data[0], "+IPDHEX:", 8))
    {
        return;
    }
	if (!strncmp(&data[0], "+EVT:MQTT_SUBRECVHEX", 20))
    {
        return;
    }
    if(http_mode != -1)
    {
        test_http_process(data,len);
        return;
    }

    printf("%s\r\n", data);
}

void qcc730_atcmd_recv_callback(uint8_t *data, uint32_t len)
{    
	if(rx_mqtt_test_param->task_started == 0)
		print_atcmd_resp(data, len);
	if(qat_demo_parser_enable == 1)
		atcmd_response_handler(len, data);
}

#ifdef SHELL_FEATURE

#define STACK_SIZE 2048

void atcmd_add_evt_parser(void)
{
	atcmd_parser_func_add("+EVT:MQTT_SUBRECV", (void*)atcmd_response_parser_EVT_MQTTSUBRECV, "parse EVT_MQTTSUBRECV response", QAT_EVT_TYPE);
	atcmd_parser_func_add("+EVT:MQTT_SUBRECVHEX", (void*)atcmd_response_parser_EVT_MQTTSUBRECVHEX, "parse EVT_MQTTSUBRECVHEX response", QAT_EVT_TYPE);
	atcmd_parser_func_add("+EVT:OTAFWUP_FIN", (void*)atcmd_response_parser_EVT_OTAFWUP_FIN, "parse EVT_OTAFWUP_FIN response", QAT_EVT_TYPE);
}
void atcmd_add_command_parser(void)
{
	atcmd_parser_func_add("+CIPDHCPV4C", (void*)atcmd_response_parser_CIPDHCPV4C, "parse CIPDHCPV4C response", QAT_RESP_TYPE);
	atcmd_parser_func_add("+RDMEM", (void*)atcmd_response_parser_RDMEM, "parse RDMEM response", QAT_RESP_TYPE);
	atcmd_parser_func_add("+IPDHEX", (void*)atcmd_response_parser_IPDHEX, "parse +IPD response", QAT_RESP_TYPE);
}

int test_reset(int argc, char ** argv){
    qc_hal_gpio_write(QC_CHIP_ON_Port, QC_CHIO_ON_Pin, QC_HAL_GPIO_PIN_RESET);
    qc_hal_delay(7500);
    qc_hal_gpio_write(QC_CHIP_ON_Port, QC_CHIO_ON_Pin, QC_HAL_GPIO_PIN_SET);
#ifdef QCC730_DFU_SUPPORT
    qcc_reset_flag = 1;
#endif
    //reset spi state
    atcmd_set_spi_state(QCC730_SPI_NOT_READY);

}

int atcmd_wakeup_qcc730(int argc, char ** argv)
{
	printf("wakeup QCC730\r\n");

    qc_hal_gpio_write(QCC730_WAKEUP_GPIO_Port, QCC730_WAKEUP_Pin, QC_HAL_GPIO_PIN_RESET);
	qc_hal_delay(300);
	qc_hal_gpio_write(QCC730_WAKEUP_GPIO_Port, QCC730_WAKEUP_Pin, QC_HAL_GPIO_PIN_SET);
}

void vTaskShell(void * p) {
	cmd_shell_init(&(UART_DEVICE));
	cmd_shell_add("help", (void*)atcmd_help, "AT Command help!");
	cmd_shell_add("parser", (void*)atcmd_enable_parser, "Enable/disalbe AT response parser");
	cmd_shell_add("qatperf", (void*)atcmd_qat_perf, "performance test");
	cmd_shell_add("tx", (void*)test_atcmd_tx, "tx");
	cmd_shell_add("rx_mqtt", (void*)test_atcmd_rx_mqtt, "rx_mqtt");
	cmd_shell_add("tx_loop", (void*)test_atcmd_tx_loop, "tx stress test");
	cmd_shell_add("reset", (void*)test_reset, "reset QCC730");
    cmd_shell_add("httptest", (void*)test_http, "http test");
	cmd_shell_add("wakeup", (void*)atcmd_wakeup_qcc730, "wakeup QCC730 during power save mode");
	while(1){
		cmd_shell_char_received();
	}
}

void task_init_all(void)
{
	QC_OSAL_BaseType_t xReturned;
	qc_osal_task_handle_t xHandle = NULL;

	qapi_spi_init();
	/* This will start ticks for OS based system*/
	qc_hal_inc_tick();
	qc_hal_gpio_write(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin, QC_HAL_GPIO_PIN_SET);

	xReturned = qc_osal_thread_create(
			vTaskShell,      	/* Function that implements the task. */
			"Shell",         	/* Text name for the task. */
			STACK_SIZE,      	/* Stack size in words, not bytes. */
			(void *) NULL,    	/* Parameter passed into the task. */
			5,					/* Priority at which the task is created. */
			&xHandle);      	/* Used to pass out the created task's handle. */
	if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
		printf("Task Shell creation error: Could not allocate required memory\r\n");
	}
    
#ifdef QCC730_DFU_SUPPORT
    xReturned = qc_osal_thread_create(
        vDFU_task,         /* Function that implements the task. */
        "DFU",            /* Text name for the task. */
        2048,         /* Stack size in words, not bytes. */
        (void *) NULL,      /* Parameter passed into the task. */
        4,                  /* Priority at which the task is created. */
        NULL);          /* Used to pass out the created task's handle. */
    if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
        printf("Task DFU creation error\r\n");
    }
#endif

	memset(atcmd_parser_resp_func_list, 0, ATCMD_PARSER_FUNC_LIST_MAX_SIZE);
	memset(atcmd_parser_evt_func_list, 0, ATCMD_PARSER_FUNC_LIST_MAX_SIZE);

	rx_mqtt_test_param = malloc(sizeof(atcmd_mqtt_rx_test_t));
	memset(rx_mqtt_test_param, 0, sizeof(atcmd_mqtt_rx_test_t));

	tx_test_param = malloc(sizeof(atcmd_tx_test_t));
	memset(tx_test_param, 0, sizeof(atcmd_tx_test_t));
	
	atcmd_add_command_parser();
	atcmd_add_evt_parser();
	

	//qapi_spi_ring_init();
	qapi_atcmd_init();
	qapi_atcmd_set_recv_cb(qcc730_atcmd_recv_callback);
}

/*This handler is generated by GPIO pin. */
void qcc730_atcmd_F2A_int_handler()
{
    qapi_atcmd_release_semp();
}
#endif
#endif
