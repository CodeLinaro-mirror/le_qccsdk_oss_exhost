/*========================================================================
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
* @file fermion_dfu_api.h
* @brief Fermion DFU APIs and structures for transfering commands and data
*        with host.
*========================================================================*/
#ifndef FERMION_DFU_API_H
#define FERMION_DFU_API_H

#include "fermion_cmn_api.h"
    
#define MAX_DFU_DATA_LEN (0x5F00)
    
/* These IDs are used to identify the resource */
typedef enum soft_id 
{
    IMG_NIL = 0x00,              /* no resource */
    SBL_ID = 0x01,               /* ID for SBL resource */
    WLAN_IMG_ID = 0x02,          /* ID for WLAN IMG */
    BDF_ID = 0x04,               /* ID for BDF */
    REGDB_ID = 0x08,             /* ID for REG_DB */
    WLAN_INI = 0x10,             /* ID for INI */
    IMG_ALL = 0x1F,              /* ID for All image */
    CMD_RESET = 0x7F,              /* CMD for reset */
    CMD_BOOT = 0x3F,              /* CMD for boot */
}soft_id_t;

typedef enum a2f_dfu_msg_id
{
    A2F_DFU_REQ = 1,                   /* msg id for DFU request on a2f DFU ring */
    A2F_DFU_DATA_TRANSFER_CFM,         /* msg id for data transfer confim on a2f DFU ring */
    A2F_DFU_DATA_TRANSFER_DONE_RSP,    /* msg id for data transfer done response on a2f DFU ring */
}a2f_dfu_msg_id_t;

typedef enum f2a_dfu_msg_id
{
    F2A_DFU_CFM = 1,                   /* msg id for DFU confirm on f2a DFU ring  */
    F2A_DFU_DATA_TRANSFER_REQ,         /* msg id for DFU transfer request on f2a DFU ring */
    F2A_DFU_DATA_TRANSFER_DONE_IND,    /* msg id for DFU transfer done indication on f2a DFU ring */
    F2A_DFU_REBOOT_REQ,                /* msg id for reboot on f2a DFU ring */
}f2a_dfu_msg_id_t;

typedef enum dfu_ring_status_code
{
    DFU_MSG_SUCCESS,                  /* msg processed successfully */
    DFU_MSG_ID_NOT_EXIST,             /* msg id not exist */
    DFU_SOFT_ID_NOT_EXIST,            /* soft id not exist */
    DFU_OFFSET_NOT_IN_RANGE,          /* elf offset not in range */
    DFU_DATA_LENGTH_NOT_IN_RANGE,     /* data length not in range */
    DFU_UNKNOWN_ERROR = 0xFF,         /* unknown error */
}dfu_ring_status_code_t;

typedef struct msg_hdr
{
    uint16_t msg_id;          /* The message id identifying the dfu message refer f2a_dfu_msg_id and a2f_dfu_msg_id*/
    uint8_t status;           /* Only used in a CFM or RSP messsage. Set to 0 in REQ and IND. */
    uint8_t reserved;
}msg_hdr_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(msg_hdr_t);
FERM_STRUCT_SIZE_SYNC(msg_hdr_t, 4);

/* A2F messages on DFU ring */
typedef struct a2f_dfu_start_req
{
    msg_hdr_t header;         /* message header that contains msg id*/
    uint32_t dfu_imgsize;         /* message header that contains msg id*/
    uint8_t soft_id_bitmap;   /* refer soft_id_t to create the bitmap(this can have bitmap for multiple resources) */
    uint8_t reserved[3];
} a2f_dfu_start_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_start_req_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_start_req_t, 12);
//FERM_STRUCT_SIZE_SYNC(a2f_dfu_start_req_t, 8);

typedef struct a2f_dfu_transfer_cfm
{
    msg_hdr_t header;               /* message header that contains the msg_id and status */
    uint32_t elf_offset;            /* offset of the elf the host has read (this has to match with the asked offset)*/
    uint32_t program_len;           /* length upto which the host has read from the elf_offset (this has to match with the asked length)*/
    uint8_t soft_id;                /* its the soft_id for which the program has read from host; refer soft_id_t */
    uint8_t reserved[3];
    uint8_t data[MAX_DFU_DATA_LEN]; /* the elf program needs to be placed here */
} a2f_dfu_transfer_cfm_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_transfer_cfm_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_transfer_cfm_t, 16+MAX_DFU_DATA_LEN);

typedef struct a2f_dfu_transfer_done_rsp
{
    msg_hdr_t header;          /* message header that contains the msg_id and status */
    uint8_t more_resource;     /* if there are more resources to update, set this to 1 else to 0*/
    uint8_t reserved[3];
} a2f_dfu_transfer_done_rsp_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(a2f_dfu_transfer_done_rsp_t);
FERM_STRUCT_SIZE_SYNC(a2f_dfu_transfer_done_rsp_t, 8);

/* F2A messages on DFU ring */
typedef struct f2a_dfu_start_cfm
{
    msg_hdr_t header;          /* message header that contains the msg_id and status */
} f2a_dfu_start_cfm_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_start_cfm_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_start_cfm_t, 4);

typedef struct f2a_dfu_transfer_req
{
    msg_hdr_t header;          /* message header that contains msg id*/
    uint32_t elf_offset;       /* the elf offset that is being requested for a read */
    uint32_t program_len;      /* length upto which the host is asked to read from the elf_offset */
    uint8_t soft_id;           /* its the soft_id for which the program is being requested for; refer soft_id_t */
    uint8_t reserved[3];
}f2a_dfu_transfer_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_transfer_req_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_transfer_req_t, 16);

typedef struct f2a_dfu_transfer_done_ind
{
    msg_hdr_t header;         /* message header that contains msg id*/
    uint8_t soft_id;          /* its the soft_id for which the all the program has transfered */
    uint8_t reserved[3];
} f2a_dfu_transfer_done_ind_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_transfer_done_ind_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_transfer_done_ind_t, 8);

typedef struct f2a_dfu_reboot_req
{
    msg_hdr_t header;         /* message header that contains msg id*/
} f2a_dfu_reboot_req_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(f2a_dfu_reboot_req_t);
FERM_STRUCT_SIZE_SYNC(f2a_dfu_reboot_req_t, 4);


#endif /* FERMION_DFU_API_H */
