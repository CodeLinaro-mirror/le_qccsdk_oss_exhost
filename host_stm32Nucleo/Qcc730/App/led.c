/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "qc_plat_al.h"
#include "qc_os_al.h"
#include "led.h"


#define CLI_print printf

static void ledBlueTask(void* argument);
static void ledGreenTask(void* argument);
static void ledRedTask(void* argument);

static void ledGreenTask(void* argument)
{
	printf("Start LED Green Task.\r\n");

    while(1)
    {
        qc_hal_gpio_toggle(QC_HAL_LED_GREEN_GPIO_Port, QC_HAL_LED_GREEN_Pin);
        qc_osal_task_delay(1000/QC_OSAL_portTICK_PERIOD_MS);
//        printf("9");

    }

    qc_osal_thread_delete(NULL);
}

static void ledBlueTask(void* argument)
{
	printf("Start LED Blue Task.\r\n");

    while(1)
    {
        qc_hal_gpio_toggle(QC_HAL_LED_BLUE_GPIO_Port, QC_HAL_LED_BLUE_Pin);
        qc_osal_task_delay(500/QC_OSAL_portTICK_PERIOD_MS);
    }

    qc_osal_thread_delete(NULL);
}

static void ledRedTask(void* argument)
{
    CLI_print("Start LED Red Task.\r\n");

    while(1)
    {
        qc_hal_gpio_toggle(QC_HAL_LED_RED_GPIO_Port, QC_HAL_LED_RED_Pin);
        qc_osal_task_delay(1000/QC_OSAL_portTICK_PERIOD_MS);
    }

    qc_osal_thread_delete(NULL);
}

void LED_taskInit(void)
{
    qc_osal_thread_create(ledBlueTask, "ledBlueTask", 256, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    qc_osal_thread_create(ledGreenTask, "ledGreenTask", 256, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
    qc_osal_thread_create(ledRedTask, "redGreenTask", 256, NULL, QC_OSAL_tskIDLE_PRIORITY + 5, NULL);
}
