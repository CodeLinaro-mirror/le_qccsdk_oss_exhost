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

#ifdef QCC730_ATCMD_ENABLE
#define AT_ARGC_MAX      128
#define AT_CMD_MAX_SIZE  16
#define ATCMD_PARSER_FUNC_LIST_MAX_SIZE 32

int qat_demo_parser_enable = 1;

typedef struct{
	char * cmd;
	int (* func)(int argc, char ** argv);
	char * description;
} atcmd_parser_func_t;

static int atcmd_parser_func_list_size = 0;
static atcmd_parser_func_t atcmd_parser_func_list[ATCMD_PARSER_FUNC_LIST_MAX_SIZE];

uint8_t atcmd_parser_exec_function_by_index(int argc, char * argv, int index)
{
	return atcmd_parser_func_list[index].func(argc, argv);
}

uint8_t atcmd_parser_find_command_index(int *index, char *header, int len)
{
	int i = 0;
	
	for(i = 0 ; i < atcmd_parser_func_list_size ; i++) {
		if (!strncmp(atcmd_parser_func_list[i].cmd, header, len)) {
			*index = i;
			return 0;
		}
	}
	return -1;
}

uint8_t atcmd_parser_func_add(char * cmd, int (* pfunc)(int argc, char ** argv), char * description) {
	if (atcmd_parser_func_list_size < ATCMD_PARSER_FUNC_LIST_MAX_SIZE) {
		atcmd_parser_func_list[atcmd_parser_func_list_size].cmd = cmd;
		atcmd_parser_func_list[atcmd_parser_func_list_size].func = pfunc;
		atcmd_parser_func_list[atcmd_parser_func_list_size].description = description;
		atcmd_parser_func_list_size++;
		return 0;
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


void atcmd_response_parser_RDMEM(int argc, char ** argv)
{
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
		printf("no need to parse\r\n");
	}
}

void atcmd_response_parser_CIPDHCPV4C(int argc, char ** argv)
{
	printf("+CIPDHCPV4C\r\n");
	printf("IP = %s\r\n", argv[0]);
	printf("Gateway = %s\r\n", argv[1]);
	printf("Mask = %s\r\n", argv[2]);
	printf("DNS0 = %s\r\n", argv[3]);
	printf("DNS1 = %s\r\n", argv[4]);
}

uint8_t atcmd_response_handler(uint16_t buf_len, char * cmd) {
	int argc;
	char * argv[AT_ARGC_MAX]={0};
	char *p;
	int comma_found=0;
	int func_found=0;
	// Separating header and arguments
	char header[AT_CMD_MAX_SIZE] = "";
	int i = 0;
	int h = 0;
	int arg_start = 0;

	for (i=0;i<buf_len;i++)
	{
		if (cmd[i] == '\r' || cmd[i] == '\0' || cmd[i] == '\n')
		{
			continue;
		}
		if (cmd[i] == ':')
		{
			break;
		}
		header[h] = cmd[i];
		h++;
	}
	header[h] = '\0';
	arg_start = i;
	// Searching for the command and parameters
	if(0 == atcmd_parser_find_command_index(&i, header, h))
	{
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
		printf("Response after parsing:\r\n");
		return atcmd_parser_exec_function_by_index(argc, argv, i);
	}
	
	return 0;
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

void print_atcmd_resp(uint8_t *data, uint32_t len)
{
    if (!strncmp(&data[2], "+CMD:", 5))
    {
        /* Only Work around for Uart and SPI conflict on STM32 platform under some conditiontion*/
        qc_osal_task_delay(1000/QC_OSAL_portTICK_PERIOD_MS);
    }
	
	printf("%s\r\n", data);	
}

void qcc730_atcmd_recv_callback(uint8_t *data, uint32_t len)
{    
	print_atcmd_resp(data, len);
	if (qat_demo_parser_enable) 
	{
	    atcmd_response_handler(len, data);
	}
}

#ifdef SHELL_FEATURE

#define STACK_SIZE 2048

void atcmd_add_command_parser(void)
{
	atcmd_parser_func_add("+RDMEM", (void*)atcmd_response_parser_RDMEM, "parse RDMEM response");
	atcmd_parser_func_add("+CIPDHCPV4C", (void*)atcmd_response_parser_CIPDHCPV4C, "parse CIPDHCPV4C response");
}
void vTaskShell(void * p) {
	cmd_shell_init(&(UART_DEVICE));
	cmd_shell_add("help", (void*)atcmd_help, "AT Command help!");
	cmd_shell_add("parser", (void*)atcmd_enable_parser, "AT Command help!");
	
	while(1){
		cmd_shell_char_received();
	}
}

void task_init_all(void)
{
	QC_OSAL_BaseType_t xReturned;
	qc_osal_task_handle_t xHandle = NULL;

	/* This will start ticks for OS based system*/
	qc_hal_inc_tick();

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

	atcmd_add_command_parser();
	
	qapi_spi_init();	
	qapi_spi_ring_init();
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
