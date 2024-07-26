/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "main.h"

#include "led.h"

#include "FreeRTOS.h"
#include "task.h"

#define CLI_print printf

static void ledBlueTask(void* argument);
static void ledGreenTask(void* argument);
static void ledRedTask(void* argument);

static void ledGreenTask(void* argument)
{
	printf("Start LED Green Task.\r\n");

    while(1)
    {
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        vTaskDelay(1000/portTICK_PERIOD_MS);
//        printf("9");

    }

    vTaskDelete(NULL);
}

static void ledBlueTask(void* argument)
{
	printf("Start LED Blue Task.\r\n");

    while(1)
    {
        HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

static void ledRedTask(void* argument)
{
    CLI_print("Start LED Red Task.\r\n");

    while(1)
    {
        HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void LED_taskInit(void)
{
    xTaskCreate(ledBlueTask, "ledBlueTask", 256, NULL, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(ledGreenTask, "ledGreenTask", 256, NULL, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(ledRedTask, "redGreenTask", 256, NULL, tskIDLE_PRIORITY + 5, NULL);
}
