/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __SW_RING_H__
#define __SW_RING_H__

#pragma pack (push, 1)
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "fermion_table_api.h"

/* Enumeration for Config info */
typedef enum {
    CONFIG_INFO_INVALID, /* Not to be used */
    CONFIG_INFO_WIFI,    /* Wi-Fi control */
    CONFIG_INFO_RAW_ETH,
    CONFIG_INFO_IP_CONNECT,
    CONFIG_INFO_IP_SECURITY,
	CONFIG_INFO_HFC,
    CONFIG_INFO_MAX,
} CONFIG_INFO_ENUM;

typedef enum {
	RING_ERROR=-1,
    RING_OK,
    RING_EMPTY,
    RING_FULL,
    RING_INDEX_ERROR,
	RING_OUTDEX_ERROR,
    RING_MAX,
} RING_STATUS;

#define SW_RING_RX_BUFFER_STATUS_ADDR 0xABCDEF00

#define SW_RING_A2F_OUTDEX_RING0_ADDR   0xABCDEF00
#define SW_RING_A2F_INDEX_RING0_ADDR    0xABCDEF00
#define SW_RING_A2F_MAXSIZE_RING0_ADDR  0xABCDEF00
#define SW_RING_A2F_BASEADDR_RING0_ADDR 0xABCDEF00

#define SW_RING_F2A_OUTDEX_RING0_ADDR 0xABCDEF00
#define SW_RING_F2A_INDEX_RING0_ADDR  0xABCDEF00
#define SW_RING_F2A_MAXSIZE_RING0_ADDR 0xABCDEF00
#define SW_RING_F2A_BASEADDR_RING0_ADDR 0xABCDEF00

#define SW_RING_TX_BUFFER_STATUS_ADDR 0xABCDEF00
#define SW_RING_LOG_BUFFER_STATUS_ADDR 0xABCDEF00

#define A2F_CONFIG_RING_ID 0
#define F2A_CONFIG_RING_ID 0
#ifdef NEW_RING1
#define A2F_DATA_RING_ID 1
#define F2A_DATA_RING_ID 1
#endif

#define MAX_NUM_RINGS 16

#define RING_DIRECTION_A2F 1
#define RING_DIRECTION_F2A 2
#define SW_RING_STATUS_RETRY_LIMIT 500

typedef struct sw_ring_element{
    uint32_t buf_addr; /* Pointer to Fermion memory buffer*/
    uint16_t len;  /* Length of the packet */
    uint16_t info; /* Info describing the packet type*/
} sw_ring_element_t;

typedef struct sw_ring_status {

	uint8_t ring_state ; // 0-NOT IN USE, 1-VALID
    uint32_t ring_addr;

    uint32_t index_addr;
    uint8_t index;
    uint32_t outdex_addr;
    uint8_t outdex;

    uint8_t numelems;
    uint8_t elemsize;
    sw_ring_element_t element;
} sw_ring_status_t;

/* Enumeration for Config info */
typedef enum {
    RING_STATE_INVALID = 0,
    RING_STATE_IN_USE
} RING_STATE_ENUM;

typedef int32_t (*SW_RING_TRANS_READ)(uint32_t handle, uint16_t bytes_to_read, uint32_t addr, uint8_t *data_buf);
typedef int32_t (*SW_RING_TRANS_WRITE)(uint32_t handle, uint16_t bytes_to_write, uint32_t addr, uint8_t *data_buf);
typedef int32_t (*SW_RING_TRANS_A2F_INT)(uint32_t handle);

int32_t sw_ring_query_read_data_length(uint32_t handle, uint8_t ring_id, uint8_t *unread, uint16_t *len);
int32_t sw_ring_ready(uint32_t handle, uint8_t ring_id);
//int32_t sw_ring_wait_for_F2A(uint32_t handle, sw_ring_status_t* ringStatus, uint8_t ring_id);
//int32_t sw_ring_wait_for_A2F(uint32_t handle, uint16_t size, sw_ring_status_t *buffer_status);
//int32_t sw_ring_A2F_space_check(uint32_t handle, uint16_t size, sw_ring_status_t* ringStatus);
int32_t sw_ring_read_buffer(uint32_t handle, uint8_t *data, uint16_t *read_len, uint8_t ring_id, uint16_t *info);
int32_t sw_ring_write_buffer(uint32_t handle, uint8_t *data, uint16_t write_len, uint16_t waitMsSec, uint8_t ring_id, uint16_t info);
int32_t sw_ring_init_table(uint32_t handle);

int32_t sw_ring_init(uint32_t handle, SW_RING_TRANS_READ trans_read, SW_RING_TRANS_WRITE trans_write, SW_RING_TRANS_A2F_INT trans_a2f_int);

#pragma pack(pop)

#endif /* __SW_RING_H__ */
