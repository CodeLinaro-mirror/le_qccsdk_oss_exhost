/*
 * Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "dfu.h"
#include "qcspi_driver.h"
#include <stdio.h>
#include "qapi_spi.h"
#include "qcc730_config.h"
#include "qc_os_al.h"

//Define
#define DBG_ENABLE

#ifdef DBG_EANBLE
#define dbg_log(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#else
#define dbg_log(fmt, ...)
#endif

#define A2F_SBL_ELF_UPDATE          (0xA2F00001)
#define F2A_SBL_ELF_UPDATE_SUCCESS  (0xF2A00001)
#define F2A_SBL_ELF_UPDATE_FAILED   (0xF2A00002)
#define FERM_SBL_CORRUPTED_PATTERN  (0xF2A0DEAD)
#define FERM_SBL_ENTERED_PATTERN    (0xF2AFACE1)
#define FERM_TABLE_HDR_PATTERN      (0xFE0DEFA)

#define FERM_DFU_TABLE_ADDR 0x90000

#define MAX_DFU_DATA_LEN (0x5F00) //23k
#define BIN_ADDR_AT_FLASH 0x08040000

#define MAX_TRY 3
#define RESET_DELAY pdMS_TO_TICKS(5000) //5s
int qcc_reset_flag = 0;
uint8_t dfu_boot_flag = 0;
int qcc_state = TARGET_IN_SBL;

typedef struct {
    soft_id_t id;
    uint32_t size;
    read_func_t func;
} dfu_cfg_t;

static dfu_cfg_t dfu_cfg;

typedef struct dfu_msg_element
{
    void* p_message;
    uint8_t* p_data;
} dfu_msg_element_t;

typedef struct ring_element
{
    uint32_t* p_buf;
    uint16_t len;
    uint16_t info;
    void* p_buf_start;
} ring_element_t;

qc_osal_queue_handle_t xDFU_msg_queue;

static target_state_e target_st = WAITING_ONBOARD;
static target_dfu_defaults_ota_t gp_ferm_default_table; //SBL <---> Host
static uint32_t spi_handle = 0;
static dfu_ring_handle_t a2f_dfu_ring_handle, f2a_dfu_ring_handle;

uint32_t get_spi_handle(void);
void read_flash(uint32_t address, uint8_t *buffer, uint32_t size);
ring_init_status_e init_dfu_ring(void);
static uint8_t dfu_ring_check_occupancy(dfu_ring_handle_t* p_dfu_ring_handle);
static uint8_t dfu_ring_check_empty(dfu_ring_handle_t* p_dfu_ring_handle);
static uint8_t a2f_dfu_ring_write_element(uint8_t* pdata, uint32_t len);
static uint8_t f2a_dfu_ring_read_element(uint8_t* pdata, uint32_t* len);
static void dfu_ring_handle_init(void);
static uint8_t send_dfu_start_request(soft_id_t soft_id);
//static void read_byte_from_target(uint32_t address, uint8_t* pdata);
static int32_t write_byte_to_target(uint32_t address, uint8_t* pdata);
static void read_config_table_hdr(uint32_t* dword);
static void set_target_st(target_state_e st);

uint32_t get_spi_handle(void)
{
    return spi_handle;
}

target_state_e get_target_st(void)
{
    return target_st;
}

static void set_target_st(target_state_e st)
{
    target_st = st;
}

void set_target_st_from_app(void)
{
    set_target_st(TARGET_IN_APP);
}

static int32_t write_byte_to_target(uint32_t address, uint8_t* pdata)
{
    return qcspi_write_addr_align(spi_handle, sizeof(uint8_t), address, pdata);
}

static char* get_dfucmd_string(uint8_t cmd)
{
    switch(cmd) {
    case WLAN_IMG_ID:
        return "+DFUAPP";
    case BDF_ID:
        return "+DFUBDF";
    case CMD_BOOT:
        return "+DFUBOOT";
    case CMD_RESET:
        return "+DFURESET";
    default:
        return "+DFUUNKNOWN";
    }
}

static void read_config_table_hdr(uint32_t* dword)
{
    if(0 != qcspi_read_addr_align(spi_handle, sizeof(target_dfu_defaults_ota_t), FERM_DFU_TABLE_ADDR, (uint8_t*)&gp_ferm_default_table))
    {
        printf("\r\n%s:FAIL:read table_hdr address 0x%x failed\r\n", get_dfucmd_string(dfu_cfg.id), FERM_DFU_TABLE_ADDR);
        assert(0);
    }
    *dword = gp_ferm_default_table.table_hdr;
    dbg_log("gp_ferm_default_table.table_hdr:0x%08lx\r\n", gp_ferm_default_table.table_hdr);
}

static void dfu_ring_handle_init(void)
{
    target_dfu_defaults_ota_t *p_target_dfu_param;

    dbg_log("%s\r\n",__func__);
    p_target_dfu_param = &(gp_ferm_default_table);

    //a2f, host -> target
    a2f_dfu_ring_handle.ring_base        = p_target_dfu_param->p_a2f_dfu_ring_base;
    a2f_dfu_ring_handle.read_index_addr  = p_target_dfu_param->p_a2f_dfu_read_idx;
    a2f_dfu_ring_handle.write_index_addr = p_target_dfu_param->p_a2f_dfu_write_idx;
    a2f_dfu_ring_handle.num_elements     = p_target_dfu_param->a2f_dfu_ring_num_elems;
    a2f_dfu_ring_handle.element_size     = p_target_dfu_param->a2f_dfu_ring_elem_size;
    a2f_dfu_ring_handle.ring_state       = DFU_RING_ACTIVE;
    //read_byte_from_target((uint32_t)a2f_dfu_ring_handle.write_index_addr, &(a2f_dfu_ring_handle.write_index));
    qcspi_read_addr_align(spi_handle, (uint16_t)4, (uint32_t)a2f_dfu_ring_handle.write_index_addr, (uint8_t*)&(a2f_dfu_ring_handle.write_index));

    //f2a, target->host
    f2a_dfu_ring_handle.ring_base        = p_target_dfu_param->p_f2a_dfu_ring_base;
    f2a_dfu_ring_handle.read_index_addr  = p_target_dfu_param->p_f2a_dfu_read_idx;
    f2a_dfu_ring_handle.write_index_addr = p_target_dfu_param->p_f2a_dfu_write_idx;
    f2a_dfu_ring_handle.num_elements     = p_target_dfu_param->f2a_dfu_ring_num_elems;
    f2a_dfu_ring_handle.element_size     = p_target_dfu_param->f2a_dfu_ring_elem_size;
    f2a_dfu_ring_handle.ring_state       = DFU_RING_ACTIVE;
    //read_byte_from_target((uint32_t)f2a_dfu_ring_handle.read_index_addr, &(f2a_dfu_ring_handle.read_index));
    qcspi_read_addr_align(spi_handle, (uint16_t)4, (uint32_t)f2a_dfu_ring_handle.read_index_addr, (uint8_t*)&(f2a_dfu_ring_handle.read_index));

    dbg_log("%s: --- F2A ---\r\n",__func__);
    dbg_log("base %p; read index addr %p; write index addr %p num_e=%d, ele size=%d\r\n",
             gp_ferm_default_table.p_f2a_dfu_ring_base,
             gp_ferm_default_table.p_f2a_dfu_read_idx,
             gp_ferm_default_table.p_f2a_dfu_write_idx,
             f2a_dfu_ring_handle.num_elements,
             f2a_dfu_ring_handle.element_size);
    dbg_log("%s: --- A2F ---\r\n",__func__);
    dbg_log("base %p; read index addr %p; write index addr %p num_e=%d, ele size=%d\r\n",
              gp_ferm_default_table.p_a2f_dfu_ring_base,
              gp_ferm_default_table.p_a2f_dfu_read_idx,
              gp_ferm_default_table.p_a2f_dfu_write_idx,
              a2f_dfu_ring_handle.num_elements,
              a2f_dfu_ring_handle.element_size);
}

ring_init_status_e init_dfu_ring(void)
{
    ring_init_status_e dfu_ring_status = RING_INIT_FAIL;
    uint32_t pattern;

    //must init spi first
    memset(&gp_ferm_default_table, 0, sizeof(target_dfu_defaults_ota_t));
    read_config_table_hdr(&pattern);
    {
        switch(gp_ferm_default_table.table_hdr)
        {
        case FERM_SBL_CORRUPTED_PATTERN:
            dbg_log("FERM_SBL_CORRUPTED_PATTERN\r\n");
            dfu_ring_status = RING_INIT_FAIL_SBL_FAIL;
            break;
        case FERM_SBL_ENTERED_PATTERN:
            dbg_log("FERM_SBL_ENTERED_PATTERN\r\n");
            dfu_ring_status = RING_INIT_ENTERED_SBL;
            dfu_ring_handle_init();
            set_target_st(TARGET_IN_SBL);
            break;
        case FERM_TABLE_HDR_PATTERN:
            dbg_log("FERM_TABLE_HDR_PATTERN\r\n");
            dfu_ring_status = RING_INIT_FAIL_SBL_OK;
            break;
        default:
            dbg_log("\r\nPATTERN FAIL: 0x%08lx\r\n", pattern);
            break;
        }
    }
    if(RING_INIT_ENTERED_SBL != dfu_ring_status)
    {
        printf("\r\n%s:FAIL:RING INIT FAIL:%d\r\n", get_dfucmd_string(dfu_cfg.id), dfu_ring_status);
    }
    return dfu_ring_status;
}

void read_flash(uint32_t address, uint8_t *buffer, uint32_t size)
{
    dbg_log("read_flash %p size:0x%lx\r\n",address, size);
    //check address
    for(uint32_t i = 0; i< size; i++)
    {
        buffer[i] = *(uint8_t *)(BIN_ADDR_AT_FLASH+address + i);
    }
}

void init_dfu_cfg(soft_id_t id, uint32_t size)
{
    //memset(&dfu_cfg, 0, sizeof(dfu_cfg));
    dfu_cfg.id   = id;
    dfu_cfg.size    = size;
    //dfu_cfg.func    = read_flash;//TBD
}

void register_dfu_func(read_func_t func)
{
    dbg_log("register_dfu_func\r\n");
    dfu_cfg.func = func;
}

void set_dfu_id(soft_id_t id)
{
    dfu_cfg.id = id;
}

void set_dfu_size(uint32_t size)
{
    dfu_cfg.size = size;
}

static uint8_t dfu_ring_check_occupancy(dfu_ring_handle_t* p_dfu_ring_handle)
{
    uint32_t write_index = 0;
    uint32_t read_index = 0;
    uint16_t bytes_to_read = 4;

    qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)(p_dfu_ring_handle->write_index_addr), (uint8_t*)&write_index);
    qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)(p_dfu_ring_handle->read_index_addr), (uint8_t*)&read_index);
    dbg_log("write_index:%ld, read_index%ld\r\n", write_index, read_index);
    dbg_log("p_dfu_ring_handle->write_index:%d\r\n", p_dfu_ring_handle->write_index);
    dbg_log("p_dfu_ring_handle->num_elements:%d\r\n", p_dfu_ring_handle->num_elements);
    if(write_index^p_dfu_ring_handle->write_index)
    {
        printf("\r\n+EVT:DFU_DATATRANSFERFAIL:There was a write from target on the write index\r\n");
        assert(0);
    }
    else
    {

        uint8_t num_elements = p_dfu_ring_handle->num_elements;

        if (num_elements != 0 && (((write_index+1)%num_elements) == read_index))
        {
            printf("\r\n+EVT:DFU_DATATRANSFERFAIL:DFU_ERR_RING_NO_ROOM\r\n");
            return DFU_ERR_RING_NO_ROOM;
        }
        else
        {
            dbg_log("DFU_ERR_RING_HAS_ROOM\r\n");
            return DFU_ERR_RING_HAS_ROOM;
        }
    }
}

static uint8_t a2f_dfu_ring_write_element(uint8_t* pdata, uint32_t len)
{
    //printf("%s\r\n",__func__);
    dbg_log("%s len:0x%lx\r\n",__func__, len);
    uint8_t status = DFU_ERR_WR;
    uint32_t data_len = 0, bytes_to_write;
    msg_hdr_t* p_msg_hdr;
    dfu_msg_element_t* p_dfu_msg_element = (dfu_msg_element_t*)pdata;
    int32_t wr_len = 0;

    p_msg_hdr = (msg_hdr_t*) p_dfu_msg_element->p_message;
    if(NULL != p_msg_hdr)
    {
        dbg_log("p_msg_hdr->msg_id is 0x%x, len is 0x%lx\r\n", p_msg_hdr->msg_id, len);
        if(A2F_DFU_REQ  == p_msg_hdr->msg_id)
        {
            a2f_dfu_start_req_t* p_msg = (a2f_dfu_start_req_t*) p_dfu_msg_element->p_message;
            dbg_log("soft_id_bitmap is 0x%0x\r\n", p_msg->soft_id_bitmap);
        }
    }
    else
    {
        dbg_log("p_msg_hdr is null\r\n");
    }
    a2f_dfu_transfer_cfm_t* p_a2f_dfu_transfer_cfm;
    ring_element_t* p_ring_element;
    ring_element_t ferm_ring_element;
    uint8_t* p_ferm_data;

    switch(p_msg_hdr->msg_id)
    {
    case A2F_DFU_DATA_TRANSFER_CFM:
        p_a2f_dfu_transfer_cfm = (a2f_dfu_transfer_cfm_t*)p_dfu_msg_element->p_message;
        data_len = p_a2f_dfu_transfer_cfm->program_len;
        break;
    default:
        data_len = 0;
        break;
    }

    bytes_to_write = len - data_len;
    dbg_log("bytes_to_write:0x%x\r\n",bytes_to_write);
    if(DFU_ERR_RING_HAS_ROOM == dfu_ring_check_occupancy(&a2f_dfu_ring_handle))
    {
        p_ring_element = (ring_element_t*)a2f_dfu_ring_handle.ring_base + a2f_dfu_ring_handle.write_index;

        if(0!= qcspi_read_addr_align(spi_handle, sizeof(ring_element_t), (uint32_t)p_ring_element, (uint8_t*)&ferm_ring_element))
        {
            dbg_log("table address %p, %d bytes fail.\n", p_ring_element, sizeof(ring_element_t));
            assert(0);
        }

        dbg_log("len:0x%lx ferm_ring_element.len:0x%x\r\n", len, ferm_ring_element.len);
        dbg_log("wrt to addr:%p\r\n",ferm_ring_element.p_buf);
        if(len <= ferm_ring_element.len)
        {
            p_ferm_data = (uint8_t*)ferm_ring_element.p_buf;
            qcspi_write_addr_align(spi_handle,bytes_to_write, (uint32_t)p_ferm_data, p_dfu_msg_element->p_message);
            p_ferm_data = p_ferm_data + bytes_to_write;
            bytes_to_write = data_len;

            dbg_log("To write data...\r\n");
            if((bytes_to_write) && (p_dfu_msg_element->p_data))
            {
                qcspi_write_addr_align(spi_handle, bytes_to_write, (uint32_t)p_ferm_data, p_dfu_msg_element->p_data);
            }
            dbg_log("END of write data.\r\n");
            //update write_index
            a2f_dfu_ring_handle.write_index = (a2f_dfu_ring_handle.write_index+1)%(a2f_dfu_ring_handle.num_elements);
            uint32_t wrt_index = a2f_dfu_ring_handle.write_index;
            dbg_log("a2f_dfu_ring_handle.write_index:%ld\r\n",wrt_index);
            dbg_log("a2f_dfu_ring_handle.write_index_addr:%p\r\n", a2f_dfu_ring_handle.write_index_addr);
            qcspi_write_addr_align(spi_handle, (int16_t)4, (uint32_t)a2f_dfu_ring_handle.write_index_addr, (uint8_t*)&(wrt_index));
            status = DFU_ERR_SUCCESS;
        }
    }
    else
    {
        status = DFU_ERR_RING_NO_ROOM;
    }

    return status;
}


static uint8_t dfu_ring_check_empty(dfu_ring_handle_t* p_dfu_ring_handle)
{
    uint32_t write_index = 0;
    uint32_t read_index = 0;
    uint16_t bytes_to_read = 4;

    //read_byte_from_target((uint32_t)(p_dfu_ring_handle->read_index_addr), &read_index);
    qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)(p_dfu_ring_handle->read_index_addr), (uint8_t*)&read_index);

    if(read_index^(p_dfu_ring_handle->read_index))
    {
        printf("\r\n+EVT:DFU_DATATRANSFERFAIL:There was a read from target on the read index 0x%x ^ 0x%x\r\n", read_index, p_dfu_ring_handle->read_index);
        assert(0);
    }
    else
    {
        //read_byte_from_target((uint32_t)(p_dfu_ring_handle->write_index_addr), &write_index);
        qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)(p_dfu_ring_handle->write_index_addr), (uint8_t*)&write_index);
        if (read_index == write_index)
            return DFU_ERR_RING_EMPTY;
        else
            return DFU_ERR_RING_NOT_EMPTY;
    }
}

static uint8_t f2a_dfu_ring_read_element(uint8_t* pdata, uint32_t* len)
{
    //printf("f2a_dfu_ring_read_element\r\n");
    uint8_t status = DFU_ERR_RING_EMPTY;
    uint32_t bytes_to_read;
    msg_hdr_t msg_hdr;
    void* p_msg = NULL;

    ring_element_t ferm_ring_element;
    ring_element_t* p_ring_element;
    dfu_msg_element_t* p_dfu_msg_element = (dfu_msg_element_t*)pdata;

    uint8_t* p_ferm_data;

    if(DFU_ERR_RING_NOT_EMPTY == dfu_ring_check_empty(&f2a_dfu_ring_handle))
    {
        p_ring_element = (ring_element_t*)f2a_dfu_ring_handle.ring_base + f2a_dfu_ring_handle.read_index;

        if( 0 != qcspi_read_addr_align(spi_handle, sizeof(ring_element_t), (uint32_t)p_ring_element, (uint8_t*)(&ferm_ring_element)))
        {
            printf("\r\n+EVT:DFU_DATATRANSFERFAIL:table address %p, %d bytes fail.\r\n",p_ring_element, sizeof(ring_element_t));
            assert(0);
        }

        bytes_to_read = sizeof(msg_hdr_t);
        if (bytes_to_read <= ferm_ring_element.len)
        {

            if( 0 != qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)(ferm_ring_element.p_buf), (uint8_t*)(&msg_hdr)))
            {
                printf("\r\n+EVT:DFU_DATATRANSFERFAIL:ring address %p fail.\r\n", ferm_ring_element.p_buf);
                //assert(0);
            }

            switch (msg_hdr.msg_id)
            {
            case F2A_DFU_CFM:
                bytes_to_read = sizeof(f2a_dfu_start_cfm_t);
                break;
            case F2A_DFU_DATA_TRANSFER_REQ:
                bytes_to_read = sizeof(f2a_dfu_transfer_req_t);
                break;
            case F2A_DFU_DATA_TRANSFER_DONE_IND:
                bytes_to_read = sizeof(f2a_dfu_transfer_done_ind_t);
                break;
            case F2A_DFU_REBOOT_REQ:
                bytes_to_read = sizeof(f2a_dfu_reboot_req_t);
                break;

            default:
                bytes_to_read = 0;
                break;
            }

            if(bytes_to_read <= ferm_ring_element.len)
            {
                if (bytes_to_read)
                {
                    p_msg = (uint8_t*)malloc(bytes_to_read);
                    if(p_msg)
                    {
                        p_ferm_data = (uint8_t*)ferm_ring_element.p_buf;
                        if(0!= qcspi_read_addr_align(spi_handle, bytes_to_read, (uint32_t)p_ferm_data, p_msg))
                        {
                            printf("\r\n+EVT:DFU_DATATRANSFERFAIL:ring address %p fail\r\n", p_ferm_data);
                            assert(0);
                        }
                        p_dfu_msg_element->p_message = p_msg;
                        *len = bytes_to_read;
                        status = DFU_ERR_SUCCESS;
                    }
                    else
                    {
                        printf("\r\n+EVT:DFU_DATATRANSFERFAIL:malloc p_msg fail\r\n");
                    }
                }
                else
                {
                    status = DFU_ERR_RD;
                    printf("\r\n+EVT:DFU_DATATRANSFERFAIL:no msg information\r\n");
                }

            }
            else
            {
                printf("\r\n+EVT:DFU_DATATRANSFERFAIL:the bytes to read is more than the initialized size.\r\n");
            }
            p_dfu_msg_element->p_data = NULL;
            //update the index
            f2a_dfu_ring_handle.read_index = (f2a_dfu_ring_handle.read_index +1)%(f2a_dfu_ring_handle.num_elements);
            uint32_t read_index = f2a_dfu_ring_handle.read_index;
            qcspi_write_addr_align(spi_handle, (uint16_t)4, (uint32_t)f2a_dfu_ring_handle.read_index_addr, (uint8_t*)&read_index);
            //write_byte_to_target((uint32_t)f2a_dfu_ring_handle.read_index_addr, &(f2a_dfu_ring_handle.read_index));
        }
        else
        {
            status = DFU_ERR_RD;
        }
    }

    return status;
}

static uint8_t recieve_dfu_start_confirm(void)
{
    dbg_log("recieve_dfu_start_confirm\r\n");
    uint32_t len;
    uint8_t read_element_status = DFU_ERR_RD, dfu_start_cfm_status = DFU_ERR_MAX;
    dfu_msg_element_t dfu_msg_element;
    f2a_dfu_start_cfm_t* p_f2a_dfu_start_cfm;

    memset(&dfu_msg_element, 0 , sizeof(dfu_msg_element_t));
    read_element_status = f2a_dfu_ring_read_element((uint8_t*)&dfu_msg_element, &len);

    if(DFU_ERR_SUCCESS == read_element_status)
    {
        p_f2a_dfu_start_cfm = dfu_msg_element.p_message;

        if( F2A_DFU_CFM == p_f2a_dfu_start_cfm->header.msg_id)
        {
            dbg_log("F2A_DFU_CFM recieved!!!\r\n");
            dfu_start_cfm_status = DFU_ERR_SUCCESS;
        }
        else
        {
            printf("\r\n+EVT:DFU_DATATRANSFERFAIL:not rcv F2A_DFU_CFM, rcv msg_id:0x%x\r\n", p_f2a_dfu_start_cfm->header.msg_id);
            //dfu_start_cfm_status = false;
        }

        free(dfu_msg_element.p_message);

        if(dfu_msg_element.p_data == NULL)
        {
            dbg_log("WARNING: there is no data: which is expected\r\n");
        }
        else
        {
            dbg_log("WARNING: there is data: which is not expected\r\n");
            free(dfu_msg_element.p_data);
        }
    }

    return dfu_start_cfm_status;
}

static uint8_t send_dfu_transfer_confirm(a2f_dfu_transfer_cfm_t* p_a2f_dfu_transfer_cfm, uint8_t* pdata)
{
    uint8_t status = DFU_ERR_WR;
    uint32_t len;
    dfu_msg_element_t dfu_msg_element;

    memset(&dfu_msg_element, 0 , sizeof(dfu_msg_element_t));
    dfu_msg_element.p_data = pdata;
    dfu_msg_element.p_message = p_a2f_dfu_transfer_cfm;
    len = sizeof(a2f_dfu_transfer_cfm_t) + p_a2f_dfu_transfer_cfm->program_len;

    dbg_log("%s len =0x%lx, program_len=0x%lx\r\n", __func__, len ,
            p_a2f_dfu_transfer_cfm->program_len);

    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element, len);

    return status;
}

static uint8_t send_dfu_transfer_done_cfm(void)
{
    uint8_t status = DFU_ERR_WR;
    dfu_msg_element_t dfu_msg_element;
    a2f_dfu_transfer_done_rsp_t a2f_dfu_transfer_done_rsp;

    memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));
    a2f_dfu_transfer_done_rsp.header.msg_id = A2F_DFU_DATA_TRANSFER_DONE_RSP;
    a2f_dfu_transfer_done_rsp.header.status = DFU_MSG_SUCCESS;
    a2f_dfu_transfer_done_rsp.more_resource = 0;

    dfu_msg_element.p_data = NULL;
    dfu_msg_element.p_message = &a2f_dfu_transfer_done_rsp;

    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element, sizeof(a2f_dfu_transfer_done_rsp_t));

    return status;
}

static void recieve_and_process_dfu_frame(f2a_dfu_msg_id_t* p_msg_id)
{
    uint32_t len, data_len, elf_offset;
    uint8_t* pdata;
    uint8_t read_element_status = DFU_ERR_RD;
    dfu_msg_element_t dfu_msg_element;
    msg_hdr_t* p_msg_hdr;
    f2a_dfu_transfer_req_t* p_f2a_dfu_transfer_req;
    soft_id_t soft_id;
    a2f_dfu_transfer_cfm_t a2f_dfu_transfer_cfm;

    memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));

    read_element_status = f2a_dfu_ring_read_element((uint8_t*)&dfu_msg_element, &len);

    if(DFU_ERR_SUCCESS == read_element_status)
    {
        p_msg_hdr = (msg_hdr_t*)dfu_msg_element.p_message;
        *p_msg_id = p_msg_hdr->msg_id;


        dbg_log("in %s msg_id:0x%x p_message:%p p_data:%p\r\n",
                __func__, p_msg_hdr->msg_id,
                dfu_msg_element.p_message,
                dfu_msg_element.p_data
        );

        switch(p_msg_hdr->msg_id)
        {
        case F2A_DFU_DATA_TRANSFER_REQ:
            p_f2a_dfu_transfer_req = (f2a_dfu_transfer_req_t*)(dfu_msg_element.p_message);
            elf_offset  = p_f2a_dfu_transfer_req->elf_offset;
            soft_id     = p_f2a_dfu_transfer_req->soft_id;

            dbg_log("F2A_DFU_DATA_TRANSFER_REQ rcv elf_offset:0x%lx program_len=0x%lx soft_id=0x%x\r\n",
                    p_f2a_dfu_transfer_req->elf_offset,
                    p_f2a_dfu_transfer_req->program_len,
                    p_f2a_dfu_transfer_req->soft_id);

            data_len = p_f2a_dfu_transfer_req->program_len;
            //data_len = 0x400;
            memset(&a2f_dfu_transfer_cfm, 0, sizeof(a2f_dfu_transfer_cfm_t));
            a2f_dfu_transfer_cfm.program_len    = data_len;
            //a2f_dfu_transfer_cfm.program_len    = 0x400;
            a2f_dfu_transfer_cfm.elf_offset     = elf_offset;
            a2f_dfu_transfer_cfm.soft_id        = soft_id;

            a2f_dfu_transfer_cfm.header.msg_id  = A2F_DFU_DATA_TRANSFER_CFM;
            a2f_dfu_transfer_cfm.header.status  = DFU_MSG_SUCCESS;

            //call the dfu_cfg.func to read data_len
            pdata = (uint8_t*)malloc(data_len);
            if(NULL == dfu_cfg.func)
                printf("\r\n+EVT:DFU_DATATRANSFERFAIL:dfu_cfg.func is NULL !!!\r\n");
            else
            {
                //printf("call dfu_cfg.func\r\n");
                dfu_cfg.func(elf_offset, pdata, data_len);
            }

            //then call send_dfu_transfer_confirm
            send_dfu_transfer_confirm(&a2f_dfu_transfer_cfm, pdata);
            free(pdata);

            break;
        case F2A_DFU_DATA_TRANSFER_DONE_IND:
            dbg_log("F2A_DFU_DATA_TRANSFER_DONE_IND rcv\r\n");
            //p_f2a_dfu_transfer_done_ind = (f2a_dfu_transfer_done_ind_t*)(dfu_msg_element.p_message);
            //soft_id = p_f2a_dfu_transfer_done_ind->soft_id;
            send_dfu_transfer_done_cfm();
            break;
        default:
            printf("\r\n+EVT:DFU_DATATRANSFERFAIL:msg_id:0x%x is not expected\n", p_msg_hdr->msg_id);
            break;
        }
        if(NULL !=p_msg_hdr)
        {
            free(p_msg_hdr);
            p_msg_hdr = NULL;
        }
    }

}

static uint8_t send_dfu_start_request(soft_id_t soft_id)
{
    dbg_log("%s soft_id:0x%0x\r\n",__func__, soft_id);
    uint8_t status = DFU_ERR_WR;
    a2f_dfu_start_req_t a2f_dfu_start_req;
    dfu_msg_element_t dfu_msg_element;

    memset(&a2f_dfu_start_req, 0, sizeof(a2f_dfu_start_req_t));
    a2f_dfu_start_req.header.msg_id = A2F_DFU_REQ;
    a2f_dfu_start_req.soft_id_bitmap = soft_id;
    a2f_dfu_start_req.dfu_imgsize = dfu_cfg.size;
    if(CMD_BOOT == soft_id || CMD_RESET == soft_id)
        a2f_dfu_start_req.dfu_imgsize = 0;

    dfu_msg_element.p_message = &a2f_dfu_start_req;
    dfu_msg_element.p_data = NULL;

#if 0
    if(CMD_BOOT == soft_id || CMD_RESET == soft_id || WLAN_IMG_ID == soft_id)
    {
        uint8_t *p = (uint8_t *) &a2f_dfu_start_req;
        for(int i=0;i<12;i++)
        {
            dbg_log("%d:0x%02x\r\n",i,*(p+i));
        }
    }
#endif

    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element, sizeof(a2f_dfu_start_req_t));

    return status;
}

void send_dfu_reset(void)
{
    dbg_log("%s\r\n",__func__);
    send_dfu_start_request(CMD_RESET);
}

void send_dfu_boot(void)
{
    dbg_log("in %s\r\n", __func__);
    //if(TARGET_IN_SBL == target_st || TARGET_DFU_ONGOING == target_st)
    {
        send_dfu_start_request(CMD_BOOT);
        set_target_st(TARGET_LOADING_APP);
    }
    dfu_boot_flag = 1;
    //else
    {
        //dbg_log("target status is %d\r\n", target_st);
    }
}

static loader_status_e dfu_loader(soft_id_t id)
{
    loader_status_e status = LOADER_FAIL;
    resource_update_state_e resource_update_state = RESOURCE_READY;
    uint8_t loader_loop_on = 1;
    f2a_dfu_msg_id_t msg_id;

    if (IMG_NIL == id)
    {
        dbg_log("No image update\r\n");
        status = NO_IMG_UPGRADE;
        loader_loop_on = 0;
    }
    else
    {
        dbg_log("enter %s RESOURCE_READY\r\n", __func__);
    }

    while(loader_loop_on)
    {
        switch (resource_update_state)
        {
        case RESOURCE_READY:
            dbg_log("%s id:0x%x\r\n", __func__, id);
            set_target_st(TARGET_DFU_ONGOING);
            if (DFU_ERR_SUCCESS == send_dfu_start_request(id))
            {
                resource_update_state = RESOURCE_WAITING_FOR_DFU_START_CRM;
            }
            break;
        case RESOURCE_WAITING_FOR_DFU_START_CRM:
            dbg_log("%s RESOURCE_WAITING_FOR_DFU_START_CRM\r\n", __func__);
            if ( DFU_ERR_SUCCESS == recieve_dfu_start_confirm())
            {
                resource_update_state = RESOURCE_TRANSFER;
            }
            break;
        case RESOURCE_TRANSFER:
            recieve_and_process_dfu_frame(&msg_id);
            switch(msg_id)
            {
            case F2A_DFU_DATA_TRANSFER_DONE_IND:
                dbg_log("F2A_DFU_DATA_TRANSFER_DONE_IND rcv\r\n");
                printf("+EVT:DFU_DATATRANSFERSUC:OK\r\n");
                resource_update_state = RESOURCE_WAIT_FOR_REBOOT;
                break;
            case F2A_DFU_REBOOT_REQ:
                send_dfu_boot();
                status = LOADER_SUCCESS;
                loader_loop_on = 0;
                dbg_log("Wrong here: F2A_DFU_REBOOT_REQ\n");
                break;
            default:
                dbg_log("msg_id:%d\n", msg_id);
                break;
            }
            break;
        case RESOURCE_WAIT_FOR_REBOOT: //BOOT -> load APP
            //delay 1s
            qc_osal_task_delay(1000);
            send_dfu_boot();
            status = LOADER_SUCCESS;
            loader_loop_on = 0;
            break;
        }
    }
    return status;
}

void dfu_app(uint32_t fsize)
{
    target_state_e st;
    loader_status_e status = LOADER_FAIL;

    //check ferm st
#if 0
    st = get_target_st();
    if (TARGET_IN_SBL != st)
    {
        dbg_log("DFU is not allowed, st:%d\n", st);
        return;
    }
#endif

    //init dfu_cfg
    init_dfu_cfg(WLAN_IMG_ID, fsize);

    //start dfu_loader
    status = dfu_loader(WLAN_IMG_ID);

    //check result and send evt (AT RSP evt)
    if(LOADER_FAIL == status)
    {
        dbg_log("dfu_app fail\r\n");
    }
    else if(LOADER_SUCCESS == status)
    {
        dbg_log("dfu_app success\r\n");
    }
}

void dfu_bdf(uint32_t fsize)
{
    target_state_e st;
    loader_status_e status = LOADER_FAIL;

    //check target st
#if 0
    st = get_target_st();
    if (TARGET_IN_SBL != st)
    {
        dbg_log("DFU is not allowed, st:%d\n", st);
        return;
    }
#endif

    //init dfu_cfg
    init_dfu_cfg(BDF_ID, fsize);

    //start dfu_loader
    status = dfu_loader(BDF_ID);

    //check result and send evt (AT RSP evt)
    if(LOADER_FAIL == status)
    {
        dbg_log("dfu_bdf fail\r\n");
    }
    else if(LOADER_SUCCESS == status)
    {
        dbg_log("dfu_bdf success\r\n");
    }
}

uint8_t onboard_target(void)
{
    target_state_e st;
    ring_init_status_e ring_st;
    uint8_t ret = DFU_ERR_FAIL;

    st = get_target_st();

    if( TARGET_IN_SBL == st)
    {
        dbg_log("%s success\r\n",__func__);
        ret = DFU_ERR_SUCCESS;
        goto exit;
    }

    if( WAITING_ONBOARD != st )
    {
        dbg_log("onboard_target fail, default is st:%d\r\n", st);
        ret = DFU_ERR_FAIL;
        goto exit;
    }

    ring_st = init_dfu_ring();
    if (RING_INIT_ENTERED_SBL == ring_st)
    {
        dbg_log("init_dfu_ring success\r\n");
        set_target_st(TARGET_IN_SBL);
        ret = DFU_ERR_SUCCESS;
    }
    else
    {
        dbg_log("%s init_dfu_ring fail, ring_st:%d\r\n",__func__, ring_st);
        ret = DFU_ERR_FAIL;
    }
exit:

        return ret;
}

int is_dfu_cmd(char* buf) {
    if( strncasecmp(buf, "AT+DFUBOOT", 10) == 0 || strncasecmp(buf, "AT+DFURESET", 11) == 0 || strncasecmp(buf, "AT+DFUAPP", 9) == 0 || strncasecmp(buf, "AT+DFUBDF", 9) == 0 )
    {
        dbg_log("DFU CMD: %s\r\n", buf);
        return 0;
    }
    return -1;
}

int is_valid_number(const char* str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }

    // Check for hexadecimal number
    if (strncasecmp(str, "0x", 2) == 0) {
        str += 2;
        while (*str) {
            if (!isxdigit(*str)) {
                return 0;
            }
            str++;
        }
        return 1;
    }

    // Check for decimal number
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

#define SHELL_ARGC_MAX 2
int check_cmd(char* buf, uint32_t* size)
{
    dbg_log("check_cmd: buf:%s\r\n", buf);

    int argc = 0;
    char* argv[SHELL_ARGC_MAX];
    char* p = buf;

    while(*p ==' ') {
        p++;
    }

    argv[argc++] = p;

    while(*p != '\0' && argc < SHELL_ARGC_MAX) {
        if(*p == '=') {
            *p = "\0";
            p++;
            if(*p != '\0') {
                argv[argc++] = p;
            }
        } else {
            p++;
        }
    }

    if (argc < 2) {
        if (strncasecmp(argv[0], "at+dfuapp", 9) == 0) {
            printf("\r\n+DFUAPP:ERROR:NO VALID SIZE INFO\r\n");
        } else if (strncasecmp(argv[0], "at+dfubdf", 9) == 0) {
            printf("\r\n+DFUBDF:ERROR:NO VALID SIZE INFO\r\n");
        }
        return -1;
    }

    if (is_valid_number(argv[1])) {
        if(strncasecmp(argv[1], "0x", 2) == 0 )
        {
            *size = (unsigned int)strtol(argv[1], NULL, 16);

        } else {
            *size = (unsigned int)strtol(argv[1], NULL, 10);
        }
        return 0;
    } else {
        if (strncasecmp(argv[0], "at+dfuapp", 9) == 0) {
            printf("\r\n+DFUAPP:ERROR:NO VALID SIZE INFO\r\n");
        }
        if (strncasecmp(argv[0], "at+dfubdf", 9) == 0) {
            printf("\r\n+DFUBDF:ERROR:NO VALID SIZE INFO\r\n");
        }
        return -1;
    }
    dbg_log("size:0x%x\r\n", *size);
}

int dfu_hook(char* buf)
{
    int status = -1;
    dfu_msg_t msg;
    msg.fsize = 0;
    //msg.func = NULL;
    int ret  = -1;

    if(strncasecmp(buf, "AT+DFUBOOT", 10) == 0)
    {
        printf("\r\n+DFUBOOT:Handle DFUBOOT\r\n");
        msg.id = CMD_BOOT;
        if (qc_osal_queue_send(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
            status = 0;
    }
    else if(strncasecmp(buf, "AT+DFURESET", 11) == 0)
    {
        printf("\r\n+DFUBOOT:Handle DFURESET\r\n");
        msg.id = CMD_RESET;
        if (qc_osal_queue_send(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
            status = 0;
    }
    else if(strncasecmp(buf, "AT+DFUAPP", 9) == 0)
    {
        dbg_log("\r\nHandle DFUAPP\r\n");
        msg.id = WLAN_IMG_ID;
        ret = check_cmd(buf, &(msg.fsize));
        if (-1 == ret)
        {
            return status;
        }
        dbg_log("msg.fsize:0x%x\r\n", msg.fsize);
        if (qc_osal_queue_send(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
            status = 0;
    }
    else if(strncasecmp(buf, "AT+DFUBDF", 9) == 0)
    {
        dbg_log("\r\nHandle DFUBDF\r\n");
        msg.id = BDF_ID;
        ret = check_cmd(buf, &(msg.fsize));
        if (-1 == ret)
        {
            return status;
        }
        dbg_log("msg.fsize:0x%x\r\n", msg.fsize);
        if (qc_osal_queue_send(xDFU_msg_queue, (void*)&msg, portMAX_DELAY) == pdPASS)
            status = 0;
    }

    return status;
}

void vDFU_task(void *pvParameters)
{
    dbg_log("\r\nvDFU_task created\r\n");

    qcc_reset_flag = 0;

    xDFU_msg_queue = qc_osal_queue_create(DFU_MSG_QUEUE_SZ, sizeof(dfu_msg_t));
    if (NULL == xDFU_msg_queue)
    {

        printf("xDFU_msg_queue fail to create\r\n");
    }
    else
    {
        dbg_log("\r\nxDFU_msg_queue is created\r\n");
    }
#ifdef QCC730_DFU_DEMO_ENABLE
    ring_init_status_e ring_st = init_dfu_ring();
    if (RING_INIT_ENTERED_SBL == ring_st)
    {
        dbg_log("init_dfu_ring success\r\n");
        set_target_st(TARGET_IN_SBL);
    }
    else
    {
        printf("\r\ninit_dfu_ring fail\r\n");
    }
    uint8_t status;


    if (NULL == xDFU_msg_queue || RING_INIT_ENTERED_SBL != ring_st)
    {
        printf("xDFU_msg_queue create fail or DFU RING init fail, delete vDFU_task!!!\r\n");
        qc_osal_thread_delete(NULL);
        return;
    }
#endif

#ifdef QCC730_ATCMD_ENABLE
    if (NULL == xDFU_msg_queue )
     {
         printf("xDFU_msg_queue create fail delete vDFU_task!!!\r\n");
         qc_osal_thread_delete(NULL);
         return;
     }
#endif

    dfu_msg_t rcv_msg;
    int try_count;
    ring_init_status_e ring_status;
    for (;;) {
        if (qc_osal_queue_msg_receive(xDFU_msg_queue, &rcv_msg, portMAX_DELAY) == pdPASS)
        {
            qcspi_init();

            dbg_log("rcv msg.id:%d\r\n", rcv_msg.id);
            if(TARGET_IN_APP == qcc_state)
            {
                printf("QCC in APP!\r\n");
            }
            else
                {
                for(try_count = 0; try_count < MAX_TRY; try_count++)
                {
                    extern void qcspi_init(void);
                    if(qcc_reset_flag == 1)
                    {
                        qcspi_init();
                        qcc_reset_flag = 0;
                        qcc_state = TARGET_IN_SBL;
                    }

                    dfu_cfg.id = rcv_msg.id;
                    ring_status = init_dfu_ring();
                    if (RING_INIT_ENTERED_SBL == ring_status)
                    {
                        switch (rcv_msg.id)
                        {
                        case WLAN_IMG_ID:
                            dbg_log("DFUAPP\r\n");
                            register_dfu_func(read_flash);
                            dfu_app(rcv_msg.fsize);

                            break;
                        case BDF_ID:
                            dbg_log("DFUBDF\r\n");
                            register_dfu_func(read_flash);
                            dfu_bdf(rcv_msg.fsize);
                            break;
                        case CMD_BOOT:
                            dbg_log("DFUBOOT\r\n");
                            send_dfu_boot();
                            printf("+DFUBOOT:OK\r\n");
                            break;
                        case CMD_RESET:
                            dbg_log("DFURESET\r\n");
                            send_dfu_reset();
                            qcc_reset_flag = 1;
                            qcc_state = TARGET_IN_SBL;
                            printf("+DFURESET:OK\r\n");
                            break;
                        default:
                            printf("Unknown ID:0x%x\r\n", rcv_msg.id);
                            break;
                        }
                        break;
                    }
                    else
                    {
                        qc_osal_task_delay(RESET_DELAY);
                    }
                }
                if (try_count >= MAX_TRY)
                {
                    switch (rcv_msg.id)
                    {
                    case WLAN_IMG_ID:
                        printf("+DFUAPP:FAIL,");
                        break;
                    case BDF_ID:
                        printf("+DFUBDF:FAIL,");
                        break;
                    case CMD_BOOT:
                        printf("+DFUBOOT:FAIL,");
                        break;
                    case CMD_RESET:
                        printf("+DFURESET:FAIL,");
                        break;
                    }
                    printf("DFU RING INIT FAIL\r\n");
                }
            }

        }
    }
}

