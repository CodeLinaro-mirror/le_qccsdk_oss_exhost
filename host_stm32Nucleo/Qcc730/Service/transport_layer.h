/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <stdint.h>
#include <string.h>
#include "qcspi_hal.h"

#ifndef SERVICE_TRANSPORT_LAYER_H_
#define SERVICE_TRANSPORT_LAYER_H_

#define MAX_SPI_BUFFER_SIZE               1400

typedef struct {
	uint8_t if_type;
	uint8_t if_num;
	uint8_t *payload;
	uint8_t flag;
	uint16_t payload_len;
} interface_handle_t;

struct target_payload_header {
	uint8_t          if_type;
	uint8_t          if_num;
	uint8_t          flags;
	uint16_t         len;
	uint16_t         offset;
	uint16_t         checksum;
	uint16_t		 seq_num;
	uint8_t          reserved2;

} __attribute__((packed));


struct pbuf {
	struct target_payload_header header;
	/* Data buffer */
	uint8_t *payload;

};

void process_rx_task(void * pvParameters);
void transaction_task(void * pvParameters);
STM_RET tx_to_slave(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen);
void handle_rx_data(uint8_t unread, uint16_t size);

#endif /* SERVICE_TRANSPORT_LAYER_H_ */
