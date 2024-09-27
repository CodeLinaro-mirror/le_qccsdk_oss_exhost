/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "qcspi_hal.h"

#ifdef PLATFORM_STM32
#define STM32_SPI_TIMEOUT	300
HAL_StatusTypeDef qcspi_transfer(SPI_HandleTypeDef *hspi, const uint8_t *pTxData, uint8_t *pRxData,
                                          uint16_t Size)
{
	HAL_StatusTypeDef ret;
	//This is for STM32 board using software CS - low active
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
	ret = HAL_SPI_TransmitReceive(hspi, (uint8_t *)pTxData, (uint8_t *)pRxData, Size, STM32_SPI_TIMEOUT);
	//software CS - high idle
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);
	return ret;
}
#else
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

static int transfer(int fd, uint8_t const *tx, uint8_t const *rx, uint16_t len);

int qcspi_transfer(int fd, const uint8_t *pTxData, uint8_t *pRxData,
                                          uint16_t Size)
{
	return transfer(fd, pTxData, pRxData, Size);
}

static int transfer(int fd, uint8_t const *tx, uint8_t const *rx, uint16_t len)
{
	int ret;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = 0,
		.speed_hz = 5000000,
		.bits_per_word = 8,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if(ret>0)
		ret=0;
	return ret;
}
#endif
