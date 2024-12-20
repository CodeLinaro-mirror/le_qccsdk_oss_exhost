/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <qc_plat_al.h>
#include "transport_layer.h"
#include "qcspi_driver.h"
#include "spi.h"

//QC_HAL_SPI_HandleTypeDef hspi1;
/* Buffer used for transmission */
uint8_t aTxBuffer[1500]={0};

/* Buffer used for reception */
uint8_t aRxBuffer[1500];

#define DEBUG_ON
#ifdef DEBUG_ON
#define dbg_print printf
#endif

uint32_t SPI_Read(uint8_t* rcv_buf, uint16_t size, uint32_t address)
{
	uint8_t *op_buf = aTxBuffer;
	uint32_t op_offset = 0;
	uint32_t i;
	uint32_t retry=0;
	uint32_t status=0;

	if(!rcv_buf)
		return QC_PLAT_ERROR_NULLPTR;
;
	/* update the TRNS_LEN */
	if(QC_HAL_OK!=qcspi_IRW(0, SPI_SLAVE_TRNS_LEN, size<<16))
		dbg_print("TRNS_LEN update error\r\n");

	memset(op_buf, 0, size);

	op_buf[op_offset] = SPI_CMDCODE_FREAD;
	op_offset += 1;
	qcspi_word_to_byte_big_endian(&op_buf[op_offset], address);
	op_offset += sizeof(uint32_t);
	for (i = 0; i < 10; i++)
	{
		op_buf[op_offset + i] = 0;//QCSPI_CMDCODE_DUMMY;
	}
	op_offset += 10;
#if 0
	for(i=0;i<5;i++)
		dbg_print("@[%d]=%02x ",i,op_buf[i]);
#endif
	/*todo: should size be +size if setting TRNS_LEN */
	if(QC_HAL_OK == qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)aRxBuffer, 15+size))
	{
		//wait until READ bulk is completed
		while((retry++)<QCSPI_MAX_INIT_TRY_TIMES)
		{
			if(QC_PLAT_OK == qcspi_RDSR(0,&status))
			{
				if(BIT_CHECK(status, 28))
				{
					;//dbg_print("TXUERR error\r\n");
				}
				if(BIT_CHECK(status, 24) == 0)
				{
					break;
				}
			}
			//dbg_print("read retry %d %x tick=%x\r\n",retry,status,HAL_GetTick());
//			HAL_Delay(1);
			qc_hal_delay(1);
		}
	}
#if 0
	for(i=op_offset;i<op_offset+size;i++)
		dbg_print(" %02x ",aRxBuffer[i]);

	dbg_print("END SPI_Read\r\n");
#endif
	memcpy(rcv_buf, aRxBuffer+op_offset, size);

	return QC_PLAT_OK;
}

uint32_t SPI_Write(uint8_t* snd_buf, uint16_t size, uint32_t address)
{
	uint8_t *op_buf = aTxBuffer;
	uint32_t op_offset = 0;
	uint32_t retry=0;
	uint32_t status=0;

	if(!snd_buf)
		return QC_PLAT_ERROR_NULLPTR;

	/* update the TRNS_LEN */
	if(QC_HAL_OK!=qcspi_IRW(0, SPI_SLAVE_TRNS_LEN, size<<16))
		dbg_print("TRNS_LEN update error\r\n");

	memset(op_buf, 0, size);

	op_buf[op_offset] = SPI_CMDCODE_WRITE;
	op_offset += 1;
	qcspi_word_to_byte_big_endian(&op_buf[op_offset], address);
	op_offset += sizeof(uint32_t);
#if 0
	for(i=0;i<5;i++)
		dbg_print("@[%d]=%02x ",i,op_buf[i]);
#endif
	/* TODO: how to avoid this copy to save time */
	memcpy(op_buf+op_offset, snd_buf, size);

	/*todo: should size be +size if setting TRNS_LEN */
	if(QC_HAL_OK == qcspi_transfer(&hspi1, (uint8_t *)op_buf, (uint8_t *)aRxBuffer, 5+size))
	{
		//wait until write is completed -- to check!
		while((retry++)<QCSPI_MAX_INIT_TRY_TIMES)
		{
			if(QC_PLAT_OK == qcspi_RDSR(0,&status))
			{
				if(retry>1)
					dbg_print("$");
				if(BIT_CHECK(status, 28))
				{
					;//dbg_print("TXUERR error\r\n");
				}
				if(BIT_CHECK(status, 20) == 0)
				{
					break;
				}
			}
			//dbg_print("write retry %d %x tick=%x\r\n",retry,status,HAL_GetTick());
			qc_hal_delay(1);
		}
	}
#if 0
	for(i=op_offset;i<op_offset+size;i++)
			dbg_print(" %02x ",op_buf[i]);
	dbg_print("END SPI_Write\r\n");
#endif
	return QC_PLAT_OK;
}

