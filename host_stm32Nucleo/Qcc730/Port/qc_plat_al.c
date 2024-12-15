/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <qc_plat_al.h>

#ifdef QCC730_STM32_PLATFORM_SUPPORT
#define STM32_SPI_TIMEOUT	300
QC_HAL_StatusTypeDef qcspi_transfer(QC_HAL_SPI_HandleTypeDef *hspi, const uint8_t *pTxData, uint8_t *pRxData,
                                          uint16_t Size)
{
	QC_HAL_StatusTypeDef ret;
	//This is for STM32 board using software CS - low active
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
	ret = HAL_SPI_TransmitReceive(hspi, (uint8_t *)pTxData, (uint8_t *)pRxData, Size, STM32_SPI_TIMEOUT);
	//software CS - high idle
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);
	return ret;
}
#endif
