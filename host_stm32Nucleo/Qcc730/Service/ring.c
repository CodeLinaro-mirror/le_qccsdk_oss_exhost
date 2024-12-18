/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <qc_os_al.h>
#include <qc_plat_al.h>
#include "ring.h"
#include "fermion_table_api.h"
#include "qcspi_driver.h"


//#define RING_DBG
#ifdef RING_DBG
#define RING_LOG printf
#else
#define RING_LOG(x, ...)
#endif
//static unsigned short waitSpiMsSec = 0;

static fermion_defaults_t gp_ferm_default_table;

static sw_ring_status_t A2F_Ring0Status[MAX_NUM_RINGS];
static sw_ring_status_t F2A_Ring0Status[MAX_NUM_RINGS];

static SW_RING_TRANS_READ sw_ring_transport_read = NULL;
static SW_RING_TRANS_WRITE sw_ring_transport_write = NULL;
static SW_RING_TRANS_A2F_INT sw_ring_transport_a2f_int = NULL;

volatile uint32_t debug_buffer_addr ;
volatile uint32_t debug_buffer_pos_addr;

static int32_t sw_ring_A2F_Update_SPI_Interrupt(uint32_t handle, sw_ring_status_t * ringStatus)
{
    RING_LOG("%s.\r\n", __FUNCTION__);

    if (0 != sw_ring_transport_a2f_int(handle))
    {
        RING_LOG("%s: failed.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-1);
    }

    return 0;
}
static int32_t sw_ring_status_get(uint32_t handle, sw_ring_status_t *ringStatus)
{
    uint8_t index;
    uint32_t ret;

//    RING_LOG("%s.\r\n", __FUNCTION__);

    // Read Index.
    ret = sw_ring_transport_read(handle, sizeof(uint8_t), ringStatus->index_addr, (uint8_t*)&index);

    if (0 != ret)
    {
    	RING_LOG("%s: read index failed ret=%d.\r\n", __FUNCTION__,ret);
        return -5;
//        SW_RING_ASSERT_OR_RETURN(-1);
    }
    ringStatus->index = index & 0xFF;

    // Read Outdex.
    ret = sw_ring_transport_read(handle, sizeof(uint8_t), ringStatus->outdex_addr, (uint8_t*)&index);

    if (0 != ret)
    {
    	RING_LOG("%s: read outdex failed outdex [%x]=%d, ret=%d\r\n", __FUNCTION__,ringStatus->outdex_addr, index, ret);
//        SW_RING_ASSERT_OR_RETURN(-1);
        return -5;
    }
    ringStatus->outdex = index & 0xFF;

    return 0;
}
static int32_t sw_ring_wait_for_F2A(uint32_t handle, sw_ring_status_t* ringStatus, uint8_t ring_id)
{
    uint32_t buffer_status_try_cnt = 0;
    uint8_t unread=0;
	uint16_t len=0;
	int32_t ret = 0;
    RING_LOG("%s.\r\n", __FUNCTION__);

    while (buffer_status_try_cnt < SW_RING_STATUS_RETRY_LIMIT)
    {
    	//If no F2A data to read, don't retry
    	ret = sw_ring_query_read_data_length(handle, ring_id, &unread, &len);
        if (ret > 0)
        {
            return RING_OK;
        }
        else if(ret==0 && buffer_status_try_cnt>=SW_RING_STATUS_RETRY_LIMIT){
        	//printf("sw_ring_wait_for_F2A no data \r\n");
        	//For interrupt mode, need improvement/enhancement
        	return RING_EMPTY;
        }
        buffer_status_try_cnt++;

        if (buffer_status_try_cnt == SW_RING_STATUS_RETRY_LIMIT)
        {
            //printf("%s failed, stop writing! len=%d\r\n", __FUNCTION__, len);
            return RING_ERROR;
        }

        RING_LOG("%s wait delay: %d.\r\n", __FUNCTION__, buffer_status_try_cnt);
//        SW_RING_DELAY_AFTER_BUFFER_FULL(); // delay for slave to release the rx buffer
        qc_hal_delay(1);
    }

    return RING_ERROR;
}
// return 0: empty or non-empty but also not full.
//         -1: full when size is not 0, or non-empty when size is zero.
//         -2: error.
static int32_t sw_ring_A2F_space_check(uint32_t handle, uint16_t size, sw_ring_status_t* ringStatus)
{
    uint32_t elementAddr;
    sw_ring_element_t* element = &ringStatus->element;
    uint32_t ret;

//    RING_LOG("%s: size=%d\r\n", __FUNCTION__, size);

    // Get the buffer status
    ret = sw_ring_status_get(handle, ringStatus);
    if (0 != ret)
    {
        RING_LOG("%s: failed.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    RING_LOG("%s: index=%d, outdex=%d, numelems=%d.\r\n", __FUNCTION__, ringStatus->index, ringStatus->outdex, ringStatus->numelems);

    if ((ringStatus->index >=  ringStatus->numelems) ||
        (ringStatus->outdex >=  ringStatus->numelems) ||
        (ringStatus->numelems == 0))
    {
    	printf("%s: corrupted index.in[%x]=%d out[%x]=%d num=%d\r\n", __FUNCTION__,ringStatus->index_addr,ringStatus->index, ringStatus->outdex_addr,ringStatus->outdex, ringStatus->numelems);
	//        SW_RING_ASSERT_OR_RETURN(-2);
        return -2;
    }

    if (ringStatus->index == ringStatus->outdex)
    {
        RING_LOG("%s: ring is empty.\r\n", __FUNCTION__);
    }
    else
    {
        if ((ringStatus->numelems > 0) &&
            ((ringStatus->index + 1) % ringStatus->numelems == ringStatus->outdex))
        {
            RING_LOG("%s: ring is Full !\r\n", __FUNCTION__);
            return -1;
        }
    }

    if(size == 0)
    {
        // return 0:  empty.
        // return -1: non-empty and also not full.
        // Only perform index/outdex check. No need to check buffer size.
        return (ringStatus->index == ringStatus->outdex) ? 0 : -1;
    }

    // check sw_ring_element_t in ring.
    elementAddr = ringStatus->ring_addr + (ringStatus->index * ringStatus->elemsize);
    memset(element, 0, sizeof(sw_ring_element_t));
    ret = sw_ring_transport_read(handle, sizeof(sw_ring_element_t), elementAddr, (uint8_t*)element);
    if (0 != ret)
    {
        RING_LOG("%s: read element failed.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    RING_LOG("%s: ring_element: buf=0x%08X, length=%d, info=%d.\r\n", __FUNCTION__, element->buf_addr, element->len, element->info);
    if (element->buf_addr == 0)
    {
        RING_LOG("%s: element->buf_addr is NULL.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    if (element->len < size)
    {
        RING_LOG("%s: element->len=%d < size=%d.\r\n", __FUNCTION__, element->len, size);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    return 0;
}

static int32_t sw_ring_wait_for_A2F(uint32_t handle, uint16_t size, sw_ring_status_t *buffer_status)
{
    uint32_t buffer_status_try_cnt = 0;
    int32_t ret;

//    RING_LOG("%s.\r\n", __FUNCTION__);

    while (buffer_status_try_cnt < SW_RING_STATUS_RETRY_LIMIT)
    {

        ret = sw_ring_A2F_space_check(handle, size, buffer_status);
//        RING_LOG("A2F RING loop ret=%d\r\n",ret);
        if (ret == 0) // empty.
        {
        	RING_LOG("A2F RING empty \r\n");
            return 0;
        }
        else if (ret == -2) // error.
        {
        	RING_LOG("%s failed, error !\r\n", __FUNCTION__);
//            SW_RING_ASSERT_OR_RETURN(-1);
        }

        buffer_status_try_cnt++;

        if (buffer_status_try_cnt == SW_RING_STATUS_RETRY_LIMIT)
        {
        	RING_LOG("%s failed, stop writing!\r\n", __FUNCTION__);
//            SW_RING_ASSERT_OR_RETURN(-1);
        }

//        SW_RING_DELAY_AFTER_BUFFER_FULL(); // delay for slave to release the rx buffer
//        qc_osal_task_delay(50/QC_OSAL_portTICK_PERIOD_MS);
        qc_hal_delay(1);

    }

    return -1;
}

static int32_t sw_ring_F2A_space_check(uint32_t handle, sw_ring_status_t* ringStatus)
{
    uint32_t elementAddr;
    sw_ring_element_t* element = &ringStatus->element;
    uint32_t ret;

//    RING_LOG("%s.\r\n", __FUNCTION__);

    // Get the buffer status
    if (0 != sw_ring_status_get(handle, ringStatus))
    {
    	RING_LOG("%s: failed.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-2);
        return -5;
    }

    RING_LOG("%s: index=%d, outdex=%d, numelems=%d.\r\n", __FUNCTION__, ringStatus->index, ringStatus->outdex, ringStatus->numelems);

    if ((ringStatus->index >=  ringStatus->numelems) ||
        (ringStatus->outdex >=  ringStatus->numelems) ||
        (ringStatus->numelems == 0))
    {
        RING_LOG("sw_ring_F2A_space_check corrputed, in=%d out=%d num=%d\r\n", ringStatus->index,ringStatus->outdex,ringStatus->numelems );
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    if (ringStatus->index == ringStatus->outdex)
    {
        RING_LOG("%s: ring is empty.\r\n", __FUNCTION__);
        return -1;
    }

    // check sw_ring_element_t in ring.
    elementAddr = ringStatus->ring_addr + (ringStatus->outdex * ringStatus->elemsize);
    memset(element, 0, sizeof(sw_ring_element_t));
    ret = sw_ring_transport_read(handle, sizeof(sw_ring_element_t), elementAddr, (uint8_t*)element);
    if (0 != ret)
    {
    	RING_LOG("%s: read element failed.ret=%d\r\n", __FUNCTION__,ret);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    RING_LOG("%s: ring_element: buf=0x%08X, length=%d, info=%d.\r\n", __FUNCTION__, element->buf_addr, element->len, element->info);
    if (element->buf_addr == 0)
    {
        RING_LOG("%s: element->buf_addr is NULL.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-2);
    }

    RING_LOG("%s: element->len=%d, element->info=%d.\r\n", __FUNCTION__, element->len, element->info);

    return 0;
}

int32_t sw_ring_write_buffer(uint32_t handle, uint8_t *data, uint16_t write_len, uint16_t waitMsSec, uint8_t ring_id, uint16_t info)
{
    sw_ring_status_t* ringStatus = &A2F_Ring0Status[ring_id];
    sw_ring_element_t* element = &ringStatus->element;
    uint32_t elementAddr;
//    uint32_t buffer_status_try_cnt = 0;
	if(sw_ring_transport_write == NULL)
	{
		RING_LOG("%s: RING is not init.\r\n", __FUNCTION__);
		return 0;
	}
    RING_LOG("%s: write_len=%d.\r\n", __FUNCTION__, write_len);

    if (sw_ring_wait_for_A2F(handle, write_len, ringStatus))
    {
//        SW_RING_ASSERT_OR_RETURN(-1);
    	RING_LOG("%s: sw_ring_wait_for_A2F failed.\r\n", __FUNCTION__);
    	return -1;
    }

    // write user's data to buffer.
    RING_LOG("%s: write A2F user's data to buffer: 0x%08X, write_len=%d.\r\n", __FUNCTION__, element->buf_addr, write_len);
    if (0 != sw_ring_transport_write(handle, write_len, element->buf_addr, data))
    {
    	RING_LOG("%s: write data %d bytes failed.\r\n", __FUNCTION__, write_len);
        return -1;
    }

    // update length and info in ring element.
    elementAddr = ringStatus->ring_addr + (ringStatus->index * ringStatus->elemsize);
    element->len = write_len;
    element->info = info;
    RING_LOG("%s: update A2F ring info: 0x%08X, write_len=%d.\r\n", __FUNCTION__, elementAddr, sizeof(sw_ring_element_t));
    if (0 != sw_ring_transport_write(handle, sizeof(sw_ring_element_t), elementAddr, (uint8_t*)element))
    {
    	RING_LOG("%s: update A2F ring0 element %d bytes failed.\r\n", __FUNCTION__, sizeof(sw_ring_element_t));
        return -1;
    }

    // update A2F index.
    ringStatus->index = ((ringStatus->index + 1) >= ringStatus->numelems) ? 0 : (ringStatus->index + 1);
    RING_LOG("%s: update A2F index: 0x%08X, index=%d.\r\n", __FUNCTION__, ringStatus->index_addr, ringStatus->index);
    if (0 != sw_ring_transport_write(handle, sizeof(uint8_t), ringStatus->index_addr, (uint8_t *)&ringStatus->index))
    {
    	RING_LOG("%s: update A2F index=%d failed.\r\n", __FUNCTION__, ringStatus->index);
        return -1;
    }

    // Raise A2F Ring Update SPI Interrupt.
    RING_LOG("%s: Raise A2F Ring Update SPI Interrupt.\r\n", __FUNCTION__);
    if (0 != sw_ring_A2F_Update_SPI_Interrupt(handle, ringStatus))
    {
    	RING_LOG("%s: Raise A2F Ring Update SPI Interrupt failed.\r\n", __FUNCTION__);
        return -1;
    }

    if (waitMsSec > 0)
    {
    	RING_LOG("%s: wait %d ms ...\r\n", __FUNCTION__, waitMsSec);
//        msleep(waitMsSec);
//        qc_osal_task_delay(waitMsSec/QC_OSAL_portTICK_PERIOD_MS);
    	RING_LOG("%s: Redo sw_ring_init_table.\r\n", __FUNCTION__);
        sw_ring_init_table(handle);
        return 0;
    }

#ifdef QCC730_SPI_DEMO_ENABLE 
    // Wait A2F data is handled by Fermion. Only update index/outdex with size = 0.
    RING_LOG("[%d]: Wait A2F data is handled by Fermion.\r\n", qc_osal_get_ticks());
    if (sw_ring_wait_for_A2F(handle, 0, ringStatus))
    {
//        SW_RING_ASSERT_OR_RETURN(-1);
    	RING_LOG("%s: sw_ring_wait_for_A2F error.\r\n", __FUNCTION__);
    	return 0;
    }
#endif

    return 0;
}

int32_t sw_ring_read_buffer(uint32_t handle, uint8_t *data, uint16_t *read_len, uint8_t ring_id, uint16_t *info)
{
    uint16_t bytes_to_read;
//    uint8_t *data_tmp = data;
    sw_ring_status_t* ringStatus = &F2A_Ring0Status[ring_id];
    sw_ring_element_t* element = &ringStatus->element;
    uint8_t wData;
    int32_t ret;

    if(NULL == read_len || NULL == info)
    {
    	RING_LOG("%s NULL ptr.\r\n", __FUNCTION__);
    	return -1;
    }
//    RING_LOG("%s.\r\n", __FUNCTION__);

    // Wait F2A Ring Update GPIO Interrupt.
    //if (0 != sw_ring_wait_for_F2A_GPIO_Interrupt(handle, ringStatus))
    //{
    //    RING_LOG("%s: Wait F2A Ring Update GPIO Interrupt failed.\r\n", __FUNCTION__);
    //    return -1;
    //}

    // Read Index/Outdex
    RING_LOG("%s: check F2A buffer status for read_len=%d.\r\n", __FUNCTION__, *read_len);
    ret = sw_ring_wait_for_F2A(handle, ringStatus, ring_id);
    if (0 != ret)
    {
        RING_LOG("%s: Wait F2A Index/Outdex failed.\r\n", __FUNCTION__);
        return ret;
    }

    /*Get info type from slave */
    *info = element->info;

    // read user's data to buffer.
    bytes_to_read = (*read_len > element->len) ? element->len : *read_len;
    RING_LOG("%s: read F2A user's data to buffer: 0x%08X, bytes_to_read=%d.\r\n", __FUNCTION__, element->buf_addr, bytes_to_read);
    if (0 != sw_ring_transport_read(handle, bytes_to_read, element->buf_addr, data))
    {
        RING_LOG("%s: read data %d bytes failed.\r\n", __FUNCTION__, bytes_to_read);
        return -1;
    }

    // update F2A outdex.
    ringStatus->outdex = ((ringStatus->outdex + 1) >= ringStatus->numelems) ? 0 : (ringStatus->outdex + 1);
    wData = ringStatus->outdex;
    RING_LOG("%s: update F2A index: 0x%08X, index=%d.\r\n", __FUNCTION__, ringStatus->outdex_addr, ringStatus->outdex);
    if (0 != sw_ring_transport_write(handle, sizeof(wData), ringStatus->outdex_addr, (uint8_t *)&wData))
    {
        RING_LOG("%s: write outdex=%d failed.\r\n", __FUNCTION__, ringStatus->outdex);
        return -1;
    }

    // Raise A2F Ring Update SPI Interrupt.
    RING_LOG("%s: finish F2A, raise A2F Ring Update SPI Interrupt.\r\n", __FUNCTION__);
    if (0 != sw_ring_A2F_Update_SPI_Interrupt(handle, ringStatus))
    {
        RING_LOG("%s: Raise A2F Ring Update SPI Interrupt failed.\r\n", __FUNCTION__);
        return -1;
    }

    *read_len = bytes_to_read;
    return 0;
}

int32_t sw_ring_query_read_data_length(uint32_t handle, uint8_t ring_id, uint8_t *unread, uint16_t *len)
{
    sw_ring_status_t* ringStatus = &F2A_Ring0Status[ring_id];
    sw_ring_element_t* element = &ringStatus->element;
    int32_t ret;
    int32_t remaining = 1;
    static uint8_t pre_outdex=0;

    RING_LOG("%s: F2A ring query.\r\n", __FUNCTION__);
    *unread = 0;
    *len = 0;
    if(sw_ring_transport_read == NULL)
    {
    	RING_LOG("%s: RING is not init.\r\n", __FUNCTION__);
    	return 0;
    }
    ret = sw_ring_F2A_space_check(handle, ringStatus);
    if (ret == -1)
    {
        RING_LOG("%s: no F2A data to read.\r\n", __FUNCTION__);
        return 0;
    }

    if (ret == -2)
    {
        RING_LOG("%s: F2A error in status.\r\n", __FUNCTION__);
        return 0;
    }
    if (ret == -5)
	{
    	RING_LOG("%s: read in=%x, out=%x.\r\n", __FUNCTION__,ringStatus->index,ringStatus->outdex);
		return 0;
	}
    if(ringStatus->outdex-pre_outdex>1 || pre_outdex-ringStatus->outdex>1){
    	if(ringStatus->outdex-pre_outdex>1)
    		remaining = ringStatus->outdex - pre_outdex;
    	else if(pre_outdex-ringStatus->outdex>1)
    	{
    		remaining = ringStatus->outdex + ringStatus->numelems - pre_outdex;
    	}
    	//if (remaining>1)
    		//printf("sw_ring_query_read_data_length %d-%d,len=%d num %d, elem %d\r\n",ringStatus->outdex,pre_outdex,element->len, ringStatus->numelems,ringStatus->elemsize);
    }

    if(ring_id == F2A_DATA_RING_ID){
		pre_outdex = ringStatus->outdex;
		RING_LOG("%s: F2A buffer: 0x%08X, length=%d.\r\n", __FUNCTION__, element->buf_addr, element->len);
		*unread = remaining;
		*len = element->len;
    }
    return element->len;
}




int32_t sw_ring_init_table(uint32_t handle)
{
    uint32_t bytes_to_read = sizeof(fermion_defaults_t);
    sw_ring_status_t* ringStatus;
    uint32_t addrTableHolder = FERM_TABLE_ADDR_HOLDER;
    uint32_t addrTable = 0;

    RING_LOG("%s: read table holder address 0x%08X.\r\n", __FUNCTION__, addrTableHolder);

    // read table address holder
    bytes_to_read = sizeof(uint32_t);
    if (0 != sw_ring_transport_read(handle, bytes_to_read, addrTableHolder, (uint8_t*)&addrTable))
    {
        RING_LOG("%s: read table holder address 0x%08X, %d bytes failed.\r\n", __FUNCTION__, addrTableHolder, bytes_to_read);
//        SW_RING_ASSERT_OR_RETURN(-1);
        return 0;
    }

    RING_LOG("%s: read table address 0x%08X.\r\n", __FUNCTION__, addrTable);

    bytes_to_read = sizeof(fermion_defaults_t);
    memset(&gp_ferm_default_table, 0, bytes_to_read);
    // read user's data to buffer.
    if (0 != sw_ring_transport_read(handle, bytes_to_read, addrTable, (uint8_t *)&gp_ferm_default_table))
    {
        RING_LOG("%s: read table address 0x%08X, %d bytes failed.\r\n", __FUNCTION__, addrTable, bytes_to_read);
//        SW_RING_ASSERT_OR_RETURN(-1);
    }

    RING_LOG("%s: table_hdr=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.table_hdr);
    // check table_hdr
    if (gp_ferm_default_table.table_hdr != FERM_TABLE_HDR_PATTERN)
    {
        RING_LOG("%s: table_hdr=0x%08X failed, FERM_TABLE_HDR_PATTERN=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.table_hdr, FERM_TABLE_HDR_PATTERN);
//        SW_RING_ASSERT_OR_RETURN(-1);
        return -1;
    }

    RING_LOG("%s: ferm_maj_ver=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.ferm_maj_ver);
    // check ferm_maj_ver
    if (gp_ferm_default_table.ferm_maj_ver != FERM_MAJOR_VER)
    {
        RING_LOG("%s: ferm_maj_ver=0x%08X failed, FERM_MAJOR_VER=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.ferm_maj_ver, FERM_MAJOR_VER);
//        SW_RING_ASSERT_OR_RETURN(-1);
    }

    RING_LOG("%s: ferm_min_ver=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.ferm_min_ver);
    // check ferm_min_ver
    if (gp_ferm_default_table.ferm_min_ver != FERM_MINOR_VER)
    {
        RING_LOG("%s: ferm_min_ver=0x%08X failed, FERM_MAJOR_VER=0x%08X.\r\n", __FUNCTION__, gp_ferm_default_table.ferm_min_ver, FERM_MINOR_VER);
        //SW_RING_ASSERT_OR_RETURN(-1);
    }

    // Init address.
    RING_LOG("%s: ----------- A2F ----------- .\r\n", __FUNCTION__);
    A2F_Ring0Status[A2F_CONFIG_RING_ID].ring_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring0_base;
    RING_LOG("%s: p_a2f_ring0_base=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_CONFIG_RING_ID].ring_addr);
    A2F_Ring0Status[A2F_CONFIG_RING_ID].index_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring0_write_idx;
    RING_LOG("%s: p_a2f_ring0_write_idx=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_CONFIG_RING_ID].index_addr);
    A2F_Ring0Status[A2F_CONFIG_RING_ID].outdex_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring0_read_idx;
    RING_LOG("%s: p_a2f_ring0_read_idx=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_CONFIG_RING_ID].outdex_addr);
    A2F_Ring0Status[A2F_CONFIG_RING_ID].elemsize = (uint32_t)gp_ferm_default_table.a2f_ring0_elem_size;
    RING_LOG("%s: a2f_ring0_elem_size=%d.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_CONFIG_RING_ID].elemsize);
    A2F_Ring0Status[A2F_CONFIG_RING_ID].numelems = (uint32_t)gp_ferm_default_table.a2f_ring0_num_elems;
    RING_LOG("%s: a2f_ring0_num_elems=%d.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_CONFIG_RING_ID].numelems);

    ringStatus = &A2F_Ring0Status[A2F_CONFIG_RING_ID];
    if ((ringStatus->index >=  ringStatus->numelems) ||
        (ringStatus->outdex >=  ringStatus->numelems) ||
        (ringStatus->numelems == 0))
    {
        RING_LOG("%s: corrupted indexes.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-1);
    }

    RING_LOG("%s: ----------- F2A ----------- .\r\n", __FUNCTION__);
    F2A_Ring0Status[F2A_CONFIG_RING_ID].ring_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring0_base;
    RING_LOG("%s: p_f2a_ring0_base=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[F2A_CONFIG_RING_ID].ring_addr);
    F2A_Ring0Status[F2A_CONFIG_RING_ID].index_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring0_write_idx;
    RING_LOG("%s: p_f2a_ring0_write_idx=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[F2A_CONFIG_RING_ID].index_addr);
    F2A_Ring0Status[F2A_CONFIG_RING_ID].outdex_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring0_read_idx;
    RING_LOG("%s: p_f2a_ring0_read_idx=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[F2A_CONFIG_RING_ID].outdex_addr);
    F2A_Ring0Status[F2A_CONFIG_RING_ID].elemsize = (uint32_t)gp_ferm_default_table.f2a_ring0_elem_size;
    RING_LOG("%s: f2a_ring0_elem_size=%d.\r\n", __FUNCTION__, F2A_Ring0Status[F2A_CONFIG_RING_ID].elemsize);
    F2A_Ring0Status[F2A_CONFIG_RING_ID].numelems = (uint32_t)gp_ferm_default_table.f2a_ring0_num_elems;
    RING_LOG("%s: f2a_ring0_num_elems=%d.\r\n", __FUNCTION__, F2A_Ring0Status[F2A_CONFIG_RING_ID].numelems);

#ifdef NEW_RING1
    // Init address.
    	RING_LOG("%s: -----------RING1 A2F ----------- .\r\n", __FUNCTION__);
        A2F_Ring0Status[A2F_DATA_RING_ID].ring_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring1_base;
        RING_LOG("%s: p_a2f_ring1_base=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_DATA_RING_ID].ring_addr);
        A2F_Ring0Status[A2F_DATA_RING_ID].index_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring1_write_idx;
        RING_LOG("%s: p_a2f_ring1_write_idx=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_DATA_RING_ID].index_addr);
        A2F_Ring0Status[A2F_DATA_RING_ID].outdex_addr = (uint32_t)gp_ferm_default_table.p_a2f_ring1_read_idx;
        RING_LOG("%s: p_a2f_ring1_read_idx=0x%08X.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_DATA_RING_ID].outdex_addr);
        A2F_Ring0Status[A2F_DATA_RING_ID].elemsize = (uint32_t)gp_ferm_default_table.a2f_ring1_elem_size;
        RING_LOG("%s: a2f_ring1_elem_size=%d.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_DATA_RING_ID].elemsize);
        A2F_Ring0Status[A2F_DATA_RING_ID].numelems = (uint32_t)gp_ferm_default_table.a2f_ring1_num_elems;
        RING_LOG("%s: a2f_ring1_num_elems=%d.\r\n", __FUNCTION__, A2F_Ring0Status[A2F_DATA_RING_ID].numelems);

        ringStatus = &A2F_Ring0Status[A2F_DATA_RING_ID];
        if ((ringStatus->index >=  ringStatus->numelems) ||
            (ringStatus->outdex >=  ringStatus->numelems) ||
            (ringStatus->numelems == 0))
        {
            RING_LOG("%s: DATA ring corrupted indexes.\r\n", __FUNCTION__);
    //        SW_RING_ASSERT_OR_RETURN(-1);
        }

        RING_LOG("%s: -----------RING1 F2A ----------- .\r\n", __FUNCTION__);
        F2A_Ring0Status[A2F_DATA_RING_ID].ring_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring1_base;
        RING_LOG("%s: p_f2a_ring1_base=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[A2F_DATA_RING_ID].ring_addr);
        F2A_Ring0Status[A2F_DATA_RING_ID].index_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring1_write_idx;
        RING_LOG("%s: p_f2a_ring1_write_idx=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[A2F_DATA_RING_ID].index_addr);
        F2A_Ring0Status[A2F_DATA_RING_ID].outdex_addr = (uint32_t)gp_ferm_default_table.p_f2a_ring1_read_idx;
        RING_LOG("%s: p_f2a_ring1_read_idx=0x%08X.\r\n", __FUNCTION__, F2A_Ring0Status[A2F_DATA_RING_ID].outdex_addr);
        F2A_Ring0Status[A2F_DATA_RING_ID].elemsize = (uint32_t)gp_ferm_default_table.f2a_ring1_elem_size;
        RING_LOG("%s: f2a_ring1_elem_size=%d.\r\n", __FUNCTION__, F2A_Ring0Status[A2F_DATA_RING_ID].elemsize);
        F2A_Ring0Status[A2F_DATA_RING_ID].numelems = (uint32_t)gp_ferm_default_table.f2a_ring1_num_elems;
        RING_LOG("%s: f2a_ring1_num_elems=%d.\r\n", __FUNCTION__, F2A_Ring0Status[A2F_DATA_RING_ID].numelems);
#endif
    RING_LOG("%s: ----------- --- ----------- .\r\n", __FUNCTION__);

    debug_buffer_addr = (uint32_t)gp_ferm_default_table.p_ferm_log_buf;
    RING_LOG("%s: debug_buffer_addr = %x.\r\n", __FUNCTION__, debug_buffer_addr);
    debug_buffer_pos_addr = (uint32_t)gp_ferm_default_table.p_ferm_log_write_idx;
    RING_LOG("%s: debug_buffer_write_ptr_addr = %x.\r\n", __FUNCTION__, debug_buffer_pos_addr);

    ringStatus = &F2A_Ring0Status[F2A_CONFIG_RING_ID];
    if ((ringStatus->index >=  ringStatus->numelems) ||
        (ringStatus->outdex >=  ringStatus->numelems) ||
        (ringStatus->numelems == 0))
    {
        RING_LOG("%s: corrupted indexes.\r\n", __FUNCTION__);
//        SW_RING_ASSERT_OR_RETURN(-1);
    }

    return 0;
}

int32_t sw_ring_init(uint32_t handle, SW_RING_TRANS_READ trans_read, SW_RING_TRANS_WRITE trans_write, SW_RING_TRANS_A2F_INT trans_a2f_int)
{
    sw_ring_transport_read = trans_read;
    sw_ring_transport_write = trans_write;
    sw_ring_transport_a2f_int = trans_a2f_int;

    return sw_ring_init_table(handle);
}



int32_t ring_init( void )
{
    int ret;
    uint32_t spi_handle = 0;

    ret = sw_ring_init(spi_handle, qcspi_read_addr_align, qcspi_write_addr_align, qcspi_interrupt_m4);
    if (ret != 0)
    {
    	RING_LOG("sw_ring_init failed\r\n");
    	return (-1);
    }

    RING_LOG("%s, ring_init END successfully.\r\n", __FUNCTION__);

    return 0;
}

