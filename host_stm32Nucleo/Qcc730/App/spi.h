/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
  */

#ifndef __SPI_H__
#define __SPI_H__
//#include "stm32u5xx_hal.h"

void SPI_taskInit(void);
uint32_t SPI_Write(uint8_t* snd_buf, uint16_t size, uint32_t address);
uint32_t SPI_Read(uint8_t* rcv_buf, uint16_t size, uint32_t address);

#endif /* SPI_H_ */
