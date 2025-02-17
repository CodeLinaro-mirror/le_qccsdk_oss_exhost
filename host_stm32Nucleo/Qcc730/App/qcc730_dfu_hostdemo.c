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
#include "dfu.h"
#include "spi.h"

#ifdef SHELL_FEATURE
#include "shell.h"
#endif

#ifdef QCC730_DFU_DEMO_ENABLE

#define demo_print printf
extern QueueHandle_t xDFU_msg_queue;

#ifdef SHELL_FEATURE

//#define STACK_SIZE 3072

void demo_spi_write_test(uint32_t address, uint32_t value)
{
    uint32_t val=value;
    SPI_Write((uint8_t*)&val, sizeof(val), address);
    demo_print("SPI write val=0x%x to address 0x%x done.\r\n", val, address);
}

void demo_spi_read_test(uint32_t address)
{
    uint32_t val=0;
    SPI_Read((uint8_t *)&val, sizeof(val), address);
    demo_print("SPI read val=0x%x from address 0x%x done.\r\n", val, address);
}

int test_dfu_qcspi_write(int argc, char ** argv){
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

int test_dfu_qcspi_read(int argc, char ** argv){
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

int test_dfu_boot(int argc, char** argv) {
    printf("\r\n handle dfuboot\r\n");
    dfu_msg_t msg;
    msg.id = CMD_BOOT;

    if (xQueueSend(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
    {
        printf("\r\n send CMD_BOOT to xDFU_msg_queue success\r\n");
    }
    else
    {
        printf("\r\n send CMD_BOOT to xDFU_msg_queue fail\r\n");
    }
    return 0;
}

int test_dfu_reset(int argc, char** argv) {
    printf("\r\n handle dfureset\r\n");
    dfu_msg_t msg;
    msg.id = CMD_RESET;

    if (xQueueSend(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
    {
        printf("\r\n send CMD_RESET to xDFU_msg_queue success\r\n");
    }
    else
    {
        printf("\r\n send CMD_RESET to xDFU_msg_queue fail\r\n");
    }
    return 0;
}

int test_dfu_app(int argc, char** argv) {
    printf("\r\nhandle dfu app\r\n");
    dfu_msg_t msg;
    msg.id = WLAN_IMG_ID;
    msg.fsize = 0x127E68;
    msg.func = read_flash;

    if (xQueueSend(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
    {
        printf("\r\n send WLAN_IMG_ID to xDFU_msg_queue success\r\n");
    }
    else
    {
        printf("\r\n send WLAN_IMG_ID to xDFU_msg_queue fail\r\n");
    }
    return 0;
}

int test_dfu_bdf(int argc, char** argv) {
    printf("\r\nhandle dfu bdf\r\n");
    dfu_msg_t msg;
    msg.id = BDF_ID;
    msg.fsize = 0x5E00;
    msg.func = read_flash;

    if (xQueueSend(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
    {
        printf("\r\n send BDF_ID to xDFU_msg_queue success\r\n");
    }
    else
    {
        printf("\r\n send BDF_ID to xDFU_msg_queue fail\r\n");
    }
    return 0;
}

void vTaskShell(void * p) {
	cmd_shell_init(&(UART_DEVICE));

	cmd_shell_add("write", (void*)test_dfu_qcspi_write, "write [addr] [val]");
	cmd_shell_add("read", (void*)test_dfu_qcspi_read, "read [addr]");
	cmd_shell_add("boot", (void*)test_dfu_boot, "boot32 APP");
	cmd_shell_add("reset", (void*)test_dfu_reset, "reset SBL");
	cmd_shell_add("AT+DFUAPP", (void*)test_dfu_app, "DFU APP");
	cmd_shell_add("AT+DFUBDF", (void*)test_dfu_app, "DFU BDF");
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

	xReturned = qc_osal_thread_create(
			vTaskShell,      	/* Function that implements the task. */
			"Shell",         	/* Text name for the task. */
			2048,      	/* Stack size in words, not bytes. */
			(void *) NULL,    	/* Parameter passed into the task. */
			1,					/* Priority at which the task is created. */
			&xHandle);      	/* Used to pass out the created task's handle. */
	if( xReturned == QC_OSAL_ERR_MEM_ALLOCATE_FAILED ) {
		printf("Task Shell creation error: Could not allocate required memory\r\n");
	}

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

}
#endif
#endif
