/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <qc_os_al.h>
#include <qc_plat_al.h>
#include "transport_layer.h"
#include "stdio.h"
#include "qcspi_driver.h"

static qc_osal_osMutexId_t mutex_spi_drv=NULL;
/* Buffer used for transmission */
uint8_t qcspiTxBuffer[1460]={0};

/* Buffer used for reception */
uint8_t qcspiRxBuffer[1460]={0};

//#define QCSPI_DBG
#ifdef QCSPI_DBG
#define QCSPI_LOG printf
#else
#define QCSPI_LOG(x, ...)
#endif

int32_t qcspi_read_addr_align(uint32_t handle, uint16_t bytes_to_read, uint32_t addr, uint8_t* read_data_buf)
{
    int32_t ret_val = -1;
    uint32_t addr_align;
    uint32_t offset;
    uint32_t size;
    uint32_t total_size = 0;
    uint8_t data_buf[4];

    if (((addr & 0x3) == 0) && ((bytes_to_read % 4) == 0))
    {
        QCSPI_LOG("%s: addr=0x%08X, bytes_to_read=%d are 4 bytes aligned.\r\n", __FUNCTION__, addr, bytes_to_read);
        return qcspi_read(handle, bytes_to_read, addr, read_data_buf);
    }

    QCSPI_LOG("%s: addr=0x%08X, bytes_to_read=%d.\r\n", __FUNCTION__, addr, bytes_to_read);
    // 1st DWORD.
    addr_align = addr & 0xFFFFFFFC;
    ret_val = qcspi_read(handle, 4, addr_align, data_buf);

    offset = addr & 0x3;
    size = ((4 - offset) > bytes_to_read) ? bytes_to_read : (4 - offset);
    memcpy((void*)read_data_buf, (void*)&data_buf[offset], size);
    total_size += size;
    QCSPI_LOG("%s: 1st DWORD addr_align=0x%08X, offset=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, offset, size);
    if (total_size >= bytes_to_read)
    {
        return ret_val;
    }
    addr_align += 4;

    // middle DWORD.
    if (bytes_to_read >= (total_size + 4))
    {
        size = ((uint32_t)bytes_to_read - total_size) & 0xFFFFFFFC; // 4 bytes alignment
        ret_val = qcspi_read(handle, size, addr_align, &read_data_buf[total_size]);
        total_size += size;
        QCSPI_LOG("%s: middle DWORD addr_align=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, size);
        if (total_size >= bytes_to_read)
        {
            return ret_val;
        }
        addr_align += size;
    }

    // last DWORD.
    size = (bytes_to_read - total_size);
    ret_val = qcspi_read(handle, 4, addr_align, data_buf);
    memcpy((void*)&read_data_buf[total_size], (void*)&data_buf[0], size);
    QCSPI_LOG("%s: last DWORD addr_align=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, size);

    return ret_val;
}

int32_t qcspi_write_addr_align(uint32_t handle, uint16_t bytes_to_write, uint32_t addr, uint8_t* write_data_buf)
{
    int32_t ret_val = -1;
    uint32_t addr_align;
    uint32_t offset;
    uint32_t size;
    uint32_t total_size = 0;
    uint8_t data_buf[4]={0};
    uint8_t nop_wbuf = 0;
    uint8_t nop_rbuf = 0;

    if (((addr & 0x3) == 0) && ((bytes_to_write % 4) == 0))
    {
        QCSPI_LOG("%s: addr=0x%08X, bytes_to_write=%d are 4 bytes aligned.\r\n", __FUNCTION__, addr, bytes_to_write);

        return qcspi_write(handle, bytes_to_write, addr, write_data_buf);
    }

    QCSPI_LOG("%s: addr=0x%08X, bytes_to_write=%d.\r\n", __FUNCTION__, addr, bytes_to_write);
    // 1st DWORD.
    addr_align = addr & 0xFFFFFFFC;

    ret_val = qcspi_read(handle, 4, addr_align, data_buf);

    offset = addr & 0x3;
    size = ((4 - offset) > bytes_to_write) ? bytes_to_write : (4 - offset);

    memcpy((void*)&data_buf[offset], (void*)write_data_buf, size);
#if 1
    //needed for STM32 platform to avoid timing issue.
    if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)nop_wbuf, (uint8_t *)nop_rbuf, 1))
	{
		QCSPI_LOG("NOP fail");
	}

    //qc_hal_delay(1);
#endif
    ret_val = qcspi_write(handle, 4, addr_align, data_buf);
    total_size += size;
    QCSPI_LOG("%s: 1st DWORD addr_align=0x%08X, offset=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, offset, size);
    if (total_size >= bytes_to_write)
    {
        return ret_val;
    }
    addr_align += 4;

    // middle DWORD.
    if (bytes_to_write >= (total_size+4))
    {
        size = ((uint32_t)bytes_to_write - total_size) & 0xFFFFFFFC; // 4 bytes alignment
        ret_val = qcspi_write(handle, size, addr_align, &write_data_buf[total_size]);
        total_size += size;
        QCSPI_LOG("%s: middle DWORD addr_align=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, size);
        if (total_size >= bytes_to_write)
        {
            return ret_val;
        }
        addr_align += size;
    }

    // last DWORD.
    size = (bytes_to_write - total_size);
    qcspi_read(handle, 4, addr_align, data_buf);
    memcpy((void*)&data_buf[0], (void*)&write_data_buf[total_size], size);
    ret_val = qcspi_write(handle, 4, addr_align, data_buf);
    QCSPI_LOG("%s: last DWORD addr_align=0x%08X, size=%d.\r\n", __FUNCTION__, addr_align, size);

    return ret_val;
}

int32_t qcspi_interrupt_m4(uint32_t handle)
{
    uint32_t config_value = 0;
#define QCSPI_CONFIG_HOST_IRQ_INT0_EN(s) ((s) << 5)

    QCSPI_LOG("-----------------------------\r\n");
    QCSPI_LOG("%s\n", __FUNCTION__);


    // For A2F interrupt, the master device has to send an IRW command, writing to SPI_SLAVE_CONFIG register enabling bit 5 (HOST_INT0).
    if (qcspi_IRR(handle, SPI_SLAVE_CONFIG, &config_value) != 0)
    {
        QCSPI_LOG("%s: read QCSPI_CONFIG Setting failed !\r\n", __FUNCTION__);
//        QCSPI_ASSERT_OR_RETURN(-1);
    }
    if(config_value & QCSPI_CONFIG_HOST_IRQ_INT0_EN(1))
    {
    	qc_hal_delay(10);
    	if (qcspi_IRR(handle, SPI_SLAVE_CONFIG, &config_value) != 0)
    		printf("qcspi interrupt duplicate, config_value=%x\r\n",config_value);

    	if(config_value & QCSPI_CONFIG_HOST_IRQ_INT0_EN(1)){
    		printf("qcspi interrupt return, config_value=%x\r\n",config_value);
    		return 0;
    	}
    }
    config_value |= QCSPI_CONFIG_HOST_IRQ_INT0_EN(ENABLE);
    if (qcspi_IRW(handle, SPI_SLAVE_CONFIG, config_value) != 0)
    {
        QCSPI_LOG("%s: write QCSPI_CONFIG Setting failed !\r\n", __FUNCTION__);
//        QCSPI_ASSERT_OR_RETURN(-1);
    }

    return 0;
}

int32_t qcspi_read(uint32_t handle, uint16_t size, uint32_t address, uint8_t* rcv_buf)
{
	uint8_t read_buf[32]={0};
	uint8_t *rx_buf;
	uint8_t *op_buf = qcspiTxBuffer;
	uint32_t op_offset = 0;
	uint32_t i;
	uint32_t retry=0;
	uint32_t status=0;
	int32_t ret=0;

	if(!rcv_buf)
		return QC_PLAT_ERROR_NULLPTR;
;
	/* update the TRNS_LEN */
	if(QC_HAL_OK!=qcspi_IRW(0, SPI_SLAVE_TRNS_LEN, size<<16))
		QCSPI_LOG("TRNS_LEN update error\r\n");

    if(size==4)
    	rx_buf = read_buf;
    else
    	rx_buf = qcspiRxBuffer;

	memset(op_buf, 0, size+1);
	memset(qcspiRxBuffer, 0, size);

	qc_osal_mutex_take(mutex_spi_drv, QC_OSAL_portMAX_DELAY);
	/*
	if(size==4)
		op_buf[op_offset] = SPI_CMDCODE_READ_SINGLE;
	else
	*/
	op_buf[op_offset] = SPI_CMDCODE_FREAD;
	op_offset += 1;
	qcspi_word_to_byte_big_endian(&op_buf[op_offset], address);
	op_offset += sizeof(uint32_t);
	#define QCSPI_READ_DUMMY	10
	for (i = 0; i < QCSPI_READ_DUMMY; i++)
	{
		op_buf[op_offset + i] = 0xa5;//QCSPI_CMDCODE_DUMMY;
	}
	op_offset += QCSPI_READ_DUMMY;

	/*todo: should size be +size if setting TRNS_LEN */
	ret = qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)rx_buf, 15+size);

	qc_osal_mutex_give(mutex_spi_drv);
	//if(ret!=QC_HAL_OK)
	//	assert(0);

	//wait until READ bulk is completed
	while((retry++)<QCSPI_MAX_INIT_TRY_TIMES)
	{
		if(QC_PLAT_OK == qcspi_RDSR(0,&status))
		{
			if(BIT_CHECK(status, 28))
			{
				//printf("TXUERR r error %x\r\n",status);
			}
			if(BIT_CHECK(status, 24) == 0)
			{
				break;
			}
		}

		if(retry == QCSPI_MAX_INIT_TRY_TIMES-1){
			//printf("r Retry error, size=%d %x\r\n", size,status);
			return QC_PLAT_FAIL_NOT_FINISHED;
		}
		qc_hal_delay(10);
	}

	memcpy(rcv_buf, rx_buf+op_offset, size);
	//qcspi_word_to_byte_big_endian_array(rcv_buf, &rx_buf[op_offset], size);

	return QC_PLAT_OK;
}

int32_t qcspi_write(uint32_t handle, uint16_t size, uint32_t address, uint8_t* snd_buf)
{
	uint8_t *op_buf = qcspiTxBuffer;
	uint32_t op_offset = 0;
	uint32_t retry=0;
	uint32_t status=0;
	int32_t ret=0;

	if(!snd_buf)
		return QC_PLAT_ERROR_NULLPTR;

	/* update the TRNS_LEN */
	if(QC_HAL_OK!=qcspi_IRW(0, SPI_SLAVE_TRNS_LEN, size<<16))
		QCSPI_LOG("TRNS_LEN update error\r\n");

	memset(op_buf, 0, size);

	qc_osal_mutex_take(mutex_spi_drv, QC_OSAL_portMAX_DELAY);
	op_buf[op_offset] = SPI_CMDCODE_WRITE;
	op_offset += 1;
	qcspi_word_to_byte_big_endian(&op_buf[op_offset], address);
	op_offset += sizeof(uint32_t);

	/* TODO: avoid this copy to save time */
	memcpy(op_buf+op_offset, snd_buf, size);

	ret = qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)qcspiRxBuffer, 5+size);
	if(QC_HAL_OK != ret)
	{
		qc_hal_delay(30);
		//printf("SPI driver write failure ret=%d size=%d address=%x\r\n", ret, size, address);
		ret = qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)qcspiRxBuffer, 5+size);
		if(ret!=QC_HAL_OK)
			printf("SPI write failure again ret=%d\r\n", ret);
	}
	qc_osal_mutex_give(mutex_spi_drv);
		//wait until write is completed -- to check!

	while((retry++)<QCSPI_MAX_INIT_TRY_TIMES)
	{
		if(QC_PLAT_OK == qcspi_RDSR(0,&status))
		{
			if(retry>1)
				QCSPI_LOG("$");
			if(BIT_CHECK(status, 28))
			{
				//printf("TXUERR w error %x\r\n",status);
			}
			if(BIT_CHECK(status, 24) == 0)
			{
				break;
			}
		}

		if(retry == QCSPI_MAX_INIT_TRY_TIMES-1){
			printf("w Retry error, size=%d %x\r\n", size,status);
			return QC_PLAT_FAIL_NOT_FINISHED;
		}
		qc_hal_delay(10);
	}
#if 0	
	uint32_t test_value;
    if (qcspi_IRR(handle, SPI_SLAVE_TRNS_LEN, &test_value) != 0)
    {
        QCSPI_LOG("%s: read QCSPI_CONFIG Setting failed !\r\n", __FUNCTION__);
//        QCSPI_ASSERT_OR_RETURN(-1);
    }
    QCSPI_LOG("test_value=%x\r\n", test_value);

	for(i=op_offset;i<op_offset+size;i++)
			dbg_print(" %02x ",op_buf[i]);
	QCSPI_LOG("END SPI_Write\r\n");
#endif
	return QC_PLAT_OK;
}
void qcspi_reset()
{
	uint8_t write_buf[4]={0};
	uint8_t read_buf[4]={0};
	uint32_t status = 0;
	/* This will reset SPI core and Read status will be 0xffffffff */
	write_buf[0] = SPI_CMDCODE_RESET_REQUEST_BYTE0;
	write_buf[1] = SPI_CMDCODE_RESET_REQUEST_BYTE1;
	write_buf[2] = SPI_CMDCODE_REQUEST_BYTE2;
	write_buf[3] = 0x00;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 4))
	{
		QCSPI_LOG("write enable fail");
	}
	qc_hal_delay(1);

	qcspi_RDSR(0,&status);
	printf("status = %x\r\n", status);
	qc_hal_delay(1);
}

void qcspi_init(void)
{
	uint8_t write_buf[5]={0};
	uint8_t read_buf[5]={0};

	write_buf[0] = SPI_CMDCODE_NOP;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 1))
	{
		QCSPI_LOG("NOP fail");
	}

	write_buf[0] = SPI_CMDCODE_WREN;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 1))
	{
		QCSPI_LOG("write enable fail");
	}

	write_buf[0] = SPI_CMDCODE_RDSR;
	write_buf[1] = 0; // dummy
	write_buf[2] = 0; // dummy
	write_buf[3] = 0; // dummy ;
	write_buf[4] = 0; // dummy ;
	if(QC_HAL_OK == qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 3))
	{
		QCSPI_LOG("RDSR CMD %02x %02x %02x ok\r\n", write_buf[0],write_buf[1],write_buf[2]);
		QCSPI_LOG("RDSR is %02x %02x %02x\r\n",read_buf[1],read_buf[2],read_buf[3], read_buf[4]);
	}
	else
		QCSPI_LOG("HAL_SPI_TransmitReceive error \r\n");
}

void qcspi_get_slaveid(uint8_t *array)
{
	uint8_t write_buf[6]={0};
	uint8_t read_buf[4]={0};

#if 1
	write_buf[0] = SPI_CMDCODE_NOP;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 1))
	{
		QCSPI_LOG("NOP fail");
	}
	qc_hal_delay(1);

	write_buf[0] = SPI_CMDCODE_WREN;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 1))
	{
		QCSPI_LOG("write enable fail");
	}
	qc_hal_delay(1);
#endif
#if 0
		uint32_t status = 0;
	/* This will reset SPI core and Read status will be 0xffffffff */
	write_buf[0] = SPI_CMDCODE_RESET_REQUEST_BYTE0;
	write_buf[1] = SPI_CMDCODE_RESET_REQUEST_BYTE1;
	write_buf[2] = SPI_CMDCODE_REQUEST_BYTE2;
	write_buf[3] = 0x00;
	if(QC_HAL_OK != qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 4))
	{
		QCSPI_LOG("write enable fail");
	}
	qc_hal_delay(1);

	qcspi_RDSR(0,&status);
	printf("status = %x\r\n", status);
	qc_hal_delay(1);
#endif

#if 1
	//	read id
	write_buf[0] = SPI_CMDCODE_RDID;
	write_buf[1] = 0;
	write_buf[2] = 0;
	write_buf[3] = 0;
	write_buf[4] = 0;
	write_buf[5] = 0;
	write_buf[6] = 0;
	read_buf[0] = 0x0;
	read_buf[1] = 0x0;
	read_buf[2] = 0x0;

	if(QC_HAL_OK == qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, 7))
	{
//		printf("Read id is %02x %02x %02x\r\n",read_buf[0],read_buf[1],read_buf[2]);
		array[0]=read_buf[0];
		array[1]=read_buf[1];
		array[2]=read_buf[2];
	}
	else
		QCSPI_LOG("HAL_SPI_TransmitReceive error \r\n");
#endif
}

uint32_t qcspi_RDSR(uint32_t handle, uint32_t *status)
{
	(void)handle;
	uint32_t ret_val=QC_PLAT_OK;
	uint8_t write_buf[5]={0};
    uint8_t read_buf[5]={0};
	if(!status)
		ret_val = QC_PLAT_ERROR_RDSR;

	qc_osal_mutex_take(mutex_spi_drv, QC_OSAL_portMAX_DELAY);
	write_buf[0] = SPI_SLAVE_RDSR;
    write_buf[1] = 0; // dummy
    write_buf[2] = 0; // dummy
    write_buf[3] = 0; // dummy ;
    write_buf[4] = 0; // dummy ;

	if(QC_HAL_OK == qcspi_transfer(&hspi1, (uint8_t *)write_buf, (uint8_t *)read_buf, sizeof(read_buf)))
	{
		QCSPI_LOG("RDSR CMD %02x %02x %02x ok\r\n", write_buf[0],write_buf[1],write_buf[2]);
		QCSPI_LOG("RDSR is %02x %02x %02x\r\n",read_buf[1],read_buf[2],read_buf[3], read_buf[4]);
	}
	else
		QCSPI_LOG("HAL_SPI_TransmitReceive error \r\n");

	*status = (read_buf[1]<<24)|(read_buf[2]<<16)|(read_buf[3]<<8)|(read_buf[4]<<0);
	qc_osal_mutex_give(mutex_spi_drv);
	return ret_val;
}
uint32_t qcspi_IRR(uint32_t handle, uint8_t regAddr, uint32_t *regVal)
{
	(void)handle;
	uint32_t ret_val;
	uint8_t op_buf[8];
	uint8_t read_buf[8];

	memset(op_buf, 0, sizeof(op_buf));
	memset(read_buf, 0, sizeof(read_buf));
	qc_osal_mutex_take(mutex_spi_drv, QC_OSAL_portMAX_DELAY);
	//	read id
	op_buf[0] = SPI_CMDCODE_IRR; //0x81
	op_buf[1] = regAddr;

	ret_val = qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)read_buf, IRR_CMD_LENGTH);
	if(QC_HAL_OK == ret_val)
	{
		QCSPI_LOG("IRR CMD %02x %x ok\r\n", read_buf[0],read_buf[1],regVal);

	}
	else{
		QCSPI_LOG("qcspi_IRR error %d\r\n", ret_val);
	}

	*regVal = (read_buf[4]<<24)|(read_buf[5]<<16)|(read_buf[6]<<8)|(read_buf[7]<<0);
	qc_osal_mutex_give(mutex_spi_drv);
	QCSPI_LOG("IRR read value=%x\r\n", *regVal);
	return ret_val;
}

uint32_t qcspi_IRW(uint32_t handle, uint8_t regAddr, uint32_t regVal)
{
	(void)handle;
	uint32_t ret_val;
	uint8_t op_buf[6]={0};
	uint8_t read_buf[8];

	op_buf[0] = SPI_CMDCODE_IRW; //0x82
	op_buf[1] = regAddr;
	qc_osal_mutex_take(mutex_spi_drv, QC_OSAL_portMAX_DELAY);
	/* memcpy will set with little endian, but qcspi spec needs transfer with big endian CMD+address+data3+data2+data1+data0*/
	qcspi_word_to_byte_big_endian(&op_buf[2], regVal);
	ret_val = qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)read_buf, IRW_CMD_LENGTH);
	if(QC_HAL_OK == ret_val)
	{
		QCSPI_LOG("Read config is %02x %02x %02x %02x\r\n",read_buf[4],read_buf[5],read_buf[6],read_buf[7]);
	}
	else{
		printf("qcspi_IRW ret=%x add=%x, val=0x%x\r\n", ret_val,regAddr, regVal);
	}
	qc_osal_mutex_give(mutex_spi_drv);
	return ret_val;
}

/*
 * qcspi need MOSI/MISO format data3 data2 data1 data0
 * eg: 0x12345678 [data0 data1 data2 data3], should be send/recive with reversed order like big endian
 */

void qcspi_word_to_byte_big_endian(uint8_t *byte_value_buf, uint32_t word_value)
{
    byte_value_buf[0] = (word_value >> 24) & 0xFF;
    byte_value_buf[1] = (word_value >> 16) & 0xFF;
    byte_value_buf[2] = (word_value >> 8) & 0xFF;
    byte_value_buf[3] = (word_value >> 0 ) & 0xFF;
}

/*For other platform*/
#if 0
static void qcspi_word_to_byte_big_endian_array(uint8_t * dst_buf, uint8_t *src_buf, uint16_t length)
{
	uint32_t i;
	if ((length % 4) > 0 || !dst_buf || !src_buf)
	{
		QCSPI_LOG("aligned with 4 or NULL ptr \r\n");
	}
    for (i=0; i<length/4; i++)
    {
    	dst_buf[i*4+0] = src_buf[i*4+3];
    	dst_buf[i*4+1] = src_buf[i*4+2];
    	dst_buf[i*4+2] = src_buf[i*4+1];
    	dst_buf[i*4+3] = src_buf[i*4+0];
    }
}
#endif

void qcspi_mutex_init(void)
{
	if(mutex_spi_drv == NULL)
	{
		mutex_spi_drv = qc_osal_mutex_create();
		assert(mutex_spi_drv);
	}
}
