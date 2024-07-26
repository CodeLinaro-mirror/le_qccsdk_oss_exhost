/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __QCAPI_HAL_H__
#define __QCAPI_HAL_H__

#include "FreeRTOS.h"
#include "task.h"
#include "stm32u5xx_hal.h"

extern SPI_HandleTypeDef hspi1;

/*Error handling*/
typedef enum stm_ret_s {
	STM_OK                = 0,
	STM_FAIL		      = 1,
	STM_ERROR_RDSR        = 2,
	STM_ERROR_NULLPTR     = 3,
	STM_FAIL_NO_MEMORY    = 4,
	STM_FAIL_NOT_FOUND    = 5,
	STM_FAIL_NOT_FINISHED = 6,
	STM_FAIL_ALIGNMENT    = 7,
}STM_RET;

/* For QCC730 slave device */
#define QCSPI_ID1	0x7F
#define QCSPI_ID2	0x90

#define QCSPI_MAX_INIT_TRY_TIMES 50

#define BIT_CHECK(var, pos) (((var) & (1 << (pos))) != 0)



#define htole16(x)                        ((uint16_t)(x))
#define le16toh(x)                        ((uint16_t)(x))

#define FREERTOS_TASK_SUPPORT
#ifdef FREERTOS_TASK_SUPPORT

//#define QC_DELAY(x)	vTaskDelay(x/portTICK_PERIOD_MS)
#define QC_DELAY(x)	HAL_Delay(x)
#define malloc                            pvPortMalloc
#define free                              vPortFree

#else
#define QC_DELAY(x)
#endif
HAL_StatusTypeDef qcspi_transfer(SPI_HandleTypeDef *hspi, const uint8_t *pTxData, uint8_t *pRxData,
                                          uint16_t Size);
#endif
