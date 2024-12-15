/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "ring.h"
#include "qapi_spi.h"
#include "qcspi_driver.h"

#ifdef QCC730_ATCMD_ENABLE
extern void atcmd_set_spi_ready(void);
#endif

int32_t qapi_spi_ring_write(uint8_t *buf, uint16_t write_len, uint8_t ring_id, uint16_t info)
{
    uint32_t spi_handle = 0; //0 for current
    int32_t retval;

//  printf("%s: input_len = %d\r\n", __FUNCTION__, input_len);

    retval = sw_ring_write_buffer(spi_handle, buf, write_len, 0, ring_id, info);
    if (retval)
    {
    	printf("ERROR: sw_ring_write_buffer %d!\r\n", retval);
        return retval;
    }

    return retval;
}


int32_t qapi_spi_ring_read(uint8_t *buf, uint16_t *read_len, uint8_t ring_id, uint16_t *info)
{
	uint32_t spi_handle = 0; //0 for current
	int32_t retval;

	retval = sw_ring_read_buffer(spi_handle, buf, read_len, ring_id, info);
	//retval = sw_ring_read_buffer_new(spi_handle, buf, read_len, ring_id, info);
    if( (retval!= RING_OK) && (retval!=RING_EMPTY) )
    {
    	//printf("ERROR: qapi_spi_ring_read failed ringid=%d, ret %d!\r\n", ring_id,retval);
        return retval;
    }
	return retval;
}

int32_t qapi_spi_ring_ready(uint8_t ring_id, uint8_t *unread, uint16_t *len)
{
	uint32_t spi_handle = 0; //0 for current
	//return sw_ring_ready(spi_handle, ring_id);
	return sw_ring_query_read_data_length(spi_handle,ring_id, unread, len);
}

int32_t qapi_spi_get_slaveid(struct device_id *id)
{
	if(!id)
	{
		printf("Null pointer \r\n");
        return QAPI_ERROR;	    
	}

	qcspi_get_slaveid(id->array);
    if(id->array[1]!=QCSPI_ID1 || id->array[2]!=QCSPI_ID2)
	{
	    return QAPI_ERROR;	        
	}

	return QAPI_OK;
}
void qapi_spi_reset( void )
{
	qcspi_reset();
}

void qapi_spi_init( void )
{
	qcspi_init();
	qcspi_mutex_init();
}

int32_t qapi_spi_ring_init( void )
{
    int32_t ret = 0;
	ret = ring_init();
#ifdef QCC730_ATCMD_ENABLE
    if (ret == 0)
    {
        atcmd_set_spi_ready();
    }
#endif
    return ret;
}
