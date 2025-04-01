/*
 * Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef TARGET_DFU_H
#define TARGET_DFU_H

#include "qcc730_config.h"
#ifdef QCC730_DFU_SUPPORT

#include "fermion_cmn_api.h"
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/*
 * Define
 */

typedef enum ring_init_status
{
    RING_INIT_FAIL = 0,
    RING_INIT_FAIL_SBL_FAIL,
    RING_INIT_ENTERED_SBL,
    RING_INIT_FAIL_SBL_OK,
} ring_init_status_e;


#define DFU_MSG_QUEUE_SZ    2

enum dfu_error_code
{
    DFU_ERR_SUCCESS         = 0x00,
    DFU_ERR_RING_EMPTY      = 0x01,
    DFU_ERR_RING_NOT_EMPTY  = 0x02,
    DFU_ERR_RING_NO_ROOM    = 0x03,
    DFU_ERR_RING_HAS_ROOM   = 0x04,
    DFU_ERR_WR              = 0x05,
    DFU_ERR_RD              = 0x06,
    DFU_ERR_FAIL            = 0xfe,
    DFU_ERR_MAX             = 0xff
};

extern int qcc_state;
typedef enum target_state
{
WAITING_ONBOARD = 0,
TARGET_IN_SBL,
TARGET_DFU_ONGOING,
TARGET_LOADING_APP,
TARGET_IN_APP
}target_state_e;

typedef enum dfu_ring_status_code
{
    DFU_MSG_SUCCESS,
    DFU_MSG_ID_NOT_EXIST,
    DFU_SOFT_ID_NOT
} dfu_ring_status_code_t;

//DFU ~ better rename to SBL
typedef struct target_dfu_defaults_ota
{
    uint32_t table_hdr; //maggic pattern
    uint16_t table_len;
    uint16_t reserved;

    //DFU
    uint32_t ferm_pbl_ver;
    uint32_t ferm_wlan_ver;
    uint32_t ferm_sbl_ver;
    uint32_t ferm_bdf_ver;
    uint32_t ferm_reg_db_ver;
    uint32_t ferm_ini_ver;

    uint8_t a2f_dfu_ring_elem_size;
    uint8_t f2a_dfu_ring_elem_size;
    uint8_t a2f_dfu_ring_num_elems;
    uint8_t f2a_dfu_ring_num_elems;

    uint32_t *p_a2f_dfu_read_idx;
    uint32_t *p_f2a_dfu_write_idx;

    uint32_t *p_f2a_dfu_read_idx;
    uint32_t *p_a2f_dfu_write_idx;

    void *p_a2f_dfu_ring_base;
    void *p_f2a_dfu_ring_base;
} target_dfu_defaults_ota_t;

FERM_STRUCT_4BYTE_ALLIGN_CHECK(target_dfu_defaults_ota_t); //This is not align check, this is size check
FERM_STRUCT_SIZE_SYNC(target_dfu_defaults_ota_t, (60));


typedef enum dfu_ring_state
{
    DFU_RING_INACTIVE = 0,
    DFU_RING_ACTIVE,
} dfu_ring_state_e;

typedef struct dfu_ring_handle {
    dfu_ring_state_e ring_state;
    uint8_t* ring_base;
    uint32_t* read_index_addr;
    uint32_t* write_index_addr;
    uint32_t read_index;
    uint32_t write_index;
    uint8_t num_elements;
    uint8_t element_size;
} dfu_ring_handle_t;

typedef enum soft_id
{
    IMG_NIL     = 0x00,
    WLAN_IMG_ID = 0x02,
    BDF_ID      = 0x04,
    CMD_BOOT    = 0x3F,
    CMD_RESET   = 0x7F,
    ID_MAX      = 0xFF,
} soft_id_t;


typedef enum a2f_dfu_msg_id
{
    A2F_DFU_REQ = 1,
    A2F_DFU_DATA_TRANSFER_CFM = 2,
    A2F_DFU_DATA_TRANSFER_DONE_RSP = 3,
} a2f_dfu_msg_id_t;

typedef struct msg_hdr
{
    uint16_t msg_id;
    uint8_t status;
    uint8_t reserved;
} msg_hdr_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(msg_hdr_t);
FERM_STRUCT_SIZE_SYNC(msg_hdr_t, 4);

typedef struct a2f_dfu_transfer_cfm
{
    msg_hdr_t header;
    uint32_t elf_offset;
    uint32_t program_len;
    uint8_t soft_id;
    uint8_t reserved[3];
    //uint8_t data[MAX_DFU_DATA_LEN];
} a2f_dfu_transfer_cfm_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_transfer_cfm_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_transfer_cfm_t, 16);

typedef void (*read_func_t)(uint32_t address, uint8_t *buffer, uint32_t size);



typedef struct a2f_dfu_start_req
{
    msg_hdr_t header;
    uint32_t dfu_imgsize;
    uint8_t soft_id_bitmap;
    uint8_t reserved[3];
} a2f_dfu_start_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_start_req_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_start_req_t, 12);

typedef struct f2a_dfu_start_cfm
{
    msg_hdr_t header;
}f2a_dfu_start_cfm_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_start_cfm_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_start_cfm_t, 4);

typedef struct f2a_dfu_transfer_req
{
    msg_hdr_t header;
    uint32_t elf_offset;
    uint32_t program_len;
    uint8_t soft_id;
    uint8_t reserved[3];
} f2a_dfu_transfer_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_transfer_req_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_transfer_req_t, 16);

typedef struct f2a_dfu_transfer_done_ind
{
    msg_hdr_t header;
    uint8_t soft_id;
    uint8_t reserved[3];
}f2a_dfu_transfer_done_ind_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_transfer_done_ind_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_transfer_done_ind_t, 8);

typedef struct f2a_dfu_reboot_req
{
    msg_hdr_t header;
}f2a_dfu_reboot_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_reboot_req_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_reboot_req_t, 4);

typedef enum f2a_dfu_msg_id
{
    F2A_DFU_CFM = 1,
    F2A_DFU_DATA_TRANSFER_REQ,
    F2A_DFU_DATA_TRANSFER_DONE_IND,
    F2A_DFU_REBOOT_REQ,
} f2a_dfu_msg_id_t;

typedef struct a2f_dfu_transfer_done_rsp
{
    msg_hdr_t header;
    uint8_t more_resource;
    uint8_t reserved[3];
} a2f_dfu_transfer_done_rsp_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_transfer_done_rsp_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_transfer_done_rsp_t, 8);

typedef enum loader_status
{
    LOADER_SUCCESS  = 0x00,
    LOADER_FAIL     = 0x01,
    NO_IMG_UPGRADE,
} loader_status_e;

typedef enum resource_update_state
{
    RESOURCE_READY = 0,
    RESOURCE_WAITING_FOR_DFU_START_CRM,
    RESOURCE_TRANSFER,
    RESOURCE_WAIT_FOR_REBOOT,
}resource_update_state_e;

typedef struct dfu_msg
{
    uint8_t id; //soft_id_t
    uint32_t fsize;
    read_func_t func;
}dfu_msg_t;



/*
 * Function
 */
void read_flash(uint32_t address, uint8_t *buffer, uint32_t size);

//for external usage
void dfu_app(uint32_t fsize);
void dfu_bdf(uint32_t fsize);
uint8_t onboard_target(void);
void send_dfu_boot(void);
void set_target_st_from_app(void);
target_state_e get_target_st(void);
void vDFU_task(void *pvParameters);
ring_init_status_e init_dfu_ring(void);
int is_dfu_cmd(char *buf);
int dfu_hook(char* buf);
void register_dfu_func(read_func_t func);
void init_dfu_cfg(soft_id_t id, uint32_t size);

#endif
#endif /*TARGET_DFU_H*/
