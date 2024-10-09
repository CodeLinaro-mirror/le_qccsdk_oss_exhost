/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/netdevice.h>

#define DRVNAME_730                "qcc730"

/* Network device operations */
#define QCC730_MAX_MTU_LEN      1600
#define TX_QUEUE_HIGH_WATER		20	/* Tx queue high water mark */
#define TX_QUEUE_LOW_WATER		8	/* Tx queue low water mark */

/*QCC730 HTC configure information*/
#define QCC730_HTC_TABLE_ADDR_HOLDER  (0x000001F4)
#define QCC730_HTC_TABLE_HDR_PATTERN  (0xFE0DEFA)
#define QCC730_SPI_MAX_TX_BUFF_LEN 2048
#define QCC730_SPI_MAX_RX_BUFF_LEN 2048
#define QCC730_SPI_MAX_RING_NUM 4
#define QCC730_SPI_CTRL_RING_IDX 0
#define QCC730_SPI_DATA_RING_IDX 1

/* QCC730 SPI slave command */
#define QCC730_SPI_CMDCODE_NOP 0x00
#define QCC730_SPI_CMDCODE_WRITE 0x02
#define QCC730_SPI_CMDCODE_RDSR 0x05
#define QCC730_SPI_CMDCODE_WREN 0x06
#define QCC730_SPI_CMDCODE_FREAD 0x0B
#define QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE0 0xDA
#define QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE1 0xBA
#define QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE2 0x80
#define QCC730_SPI_CMDCODE_IRR 0x81
#define QCC730_SPI_CMDCODE_IRW 0x82
#define QCC730_SPI_CMDCODE_READ_SINGLE 0x83
#define QCC730_SPI_CMDCODE_RDID 0x9F

/*QCC730 SPI slave command length*/
#define QCC730_SPI_CMDCODE_LEN 1
#define QCC730_SPI_ADDR_LEN 4
#define QCC730_SPI_REG_ADDR_LEN 1
#define QCC730_SPI_CMDCODE_IRR_DUMMY_BYTES 2
#define QCC730_SPI_CMDCODE_FREAD_DUMMY_BYTES 10

/* QCC730 internal registers */
#define QCC730_SPI_SLAVE_REG_DEVICE_ID	0x4
#define QCC730_SPI_SLAVE_REG_STATUS	0x8
#define QCC730_SPI_SLAVE_REG_CONFIG	0xC
#define QCC730_SPI_SLAVE_REG_TRNS_LEN	0x50

