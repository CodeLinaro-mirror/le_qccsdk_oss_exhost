/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef INC_SHELL_H_
#define INC_SHELL_H_

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"


extern UART_HandleTypeDef huart1;
#define UART_DEVICE huart1

extern char c;
extern QueueHandle_t qShell;

uint8_t uart_write(char *s, uint16_t size);
uint8_t cmd_shell_init(UART_HandleTypeDef* huart);
uint8_t cmd_shell_add(char * cmd, int (* pfunc)(int argc, char ** argv), char * description);
uint8_t cmd_shell_char_received();
uint8_t cmd_shell_exec(char * cmd);

#endif /* INC_SHELL_H_ */
