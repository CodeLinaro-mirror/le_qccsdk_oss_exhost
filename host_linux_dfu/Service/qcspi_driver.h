/******************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear

 ******************************************************************************
 */
#ifndef __QCSPI_DRIVER_H__
#define __QCSPI_DRIVER_H__
#include "stdint.h"
#include "stdio.h"

/* SPI slave CMD */
#define SPI_CMDCODE_NOP 0x00
#define SPI_CMDCODE_WREN 0x06
#define SPI_CMDCODE_RESET_REQUEST_BYTE0 0xDA
#define SPI_CMDCODE_RESET_REQUEST_BYTE1 0xBA
#define SPI_CMDCODE_RESET_BYTE2 0x0
#define SPI_CMDCODE_REQUEST_BYTE2 0x80
#define SPI_CMDCODE_RDSR 0x05
#define SPI_CMDCODE_RDID 0x9F
#define SPI_CMDCODE_FREAD 0x0B
#define SPI_CMDCODE_WRITE 0x02
#define SPI_CMDCODE_IRR 0x81
#define SPI_CMDCODE_IRW 0x82
#define SPI_CMDCODE_READ_SINGLE 0x83

/* For internal registers */
#define SPI_SLAVE_DEVICE_ID	0x4
#define SPI_SLAVE_RDSR	0x5
#define SPI_SLAVE_STATUS	0x8
#define SPI_SLAVE_CONFIG	0xC
#define SPI_SLAVE_TRNS_LEN	0x50

#define IRW_CMD_LENGTH	6
#define IRR_CMD_LENGTH	8


#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))

#define QCSPI_DRIVER_TRANS_REGISTER(qcspi_driver_trans) \
        static void qcspi_driver_trans(void) __attribute__((constructor)); \
        static void qcspi_driver_trans(void)

typedef enum
{
  DISABLE = 0,
  ENABLE = !DISABLE
} FunctionalState;

//int32_t qcspi_init(int deviceNumber, uint32_t *handle_ptr, int mode);
int32_t qcspi_read_addr_align(uint32_t handle, uint16_t bytes_to_read, uint32_t addr, uint8_t* read_data_buf);
int32_t qcspi_read(uint32_t handle, uint16_t bytes_to_read, uint32_t addr, uint8_t *data_buf); // bytes_to_read: 1~65534
int32_t qcspi_write_addr_align(uint32_t handle, uint16_t bytes_to_write, uint32_t addr, uint8_t* write_data_buf);
int32_t qcspi_write(uint32_t handle, uint16_t bytes_to_write, uint32_t addr, uint8_t *data_buf); // bytes_to_read: 1~65535
int32_t qcspi_interrupt_m4(uint32_t handle);
uint32_t qcspi_RDSR(uint32_t handle, uint32_t *status);
uint32_t qcspi_IRR(uint32_t handle, uint8_t regAddr, uint32_t *regVal);
uint32_t qcspi_IRW(uint32_t handle, uint8_t regAddr, uint32_t regVal);
void qcspi_get_slaveid();
void qcspi_mutex_init(void);
//void qcspi_fini(uint32_t handle);

void qcspi_word_to_byte_big_endian(uint8_t *byte_value_buf, uint32_t word_value);
//static void qcspi_word_to_byte_big_endian_array(uint8_t * dst_buf, uint8_t *src_buf, uint16_t length);

#endif
