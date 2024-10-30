/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "qcc730.h"

/* Ring buffer description used in Host Target Communication */
struct htc_ring_desc {
    u32 a2f_base_addr;
    u32 a2f_read_idx_addr;
    u32 a2f_write_idx_addr;
    u32 a2f_elem_size;
    u32 a2f_elem_num;
    u32 f2a_base_addr;
    u32 f2a_read_idx_addr;
    u32 f2a_write_idx_addr;
    u32 f2a_elem_size;
    u32 f2a_elem_num;
};

/*QCC730 device struction*/
struct qcc730_device {
	struct spi_device   *spi;
	struct net_device   *ndev;
	u8 mac_addr[QCC730_MAC_REGION_LEN];
    u8 spi_tx_buff[QCC730_SPI_MAX_TX_BUFF_LEN];
    u8 spi_rx_buff[QCC730_SPI_MAX_RX_BUFF_LEN];
    struct htc_ring_desc rings[QCC730_SPI_MAX_RING_NUM];
    u8 a2f_sring_rd_idx[QCC730_SPI_MAX_RING_NUM];
    u8 a2f_sring_wr_idx[QCC730_SPI_MAX_RING_NUM];
    u8 f2a_sring_rd_idx[QCC730_SPI_MAX_RING_NUM];
    u8 f2a_sring_wr_idx[QCC730_SPI_MAX_RING_NUM];
    struct mutex        spi_lock;
    struct work_struct	txrx_work;
    struct work_struct	ctrl_work;
    struct sk_buff_head	txq;
    int irq;
    int state;
    int f2a_irq_pending;
    unsigned long flags;
};

typedef union {
    u32 reg;                         /*!< QCC730_SPI_SLAVE_REG_CONFIG_TYPE                                            */
    
    struct {
      u32 RSVD       : 5;            /*!< RSVD                                                                      */
      u32 HOST_INT   : 3;            /*!< HOST_INT                                                                  */
      u32 RSVD3      : 2;            /*!< RSVD3                                                                     */
      u32 SPI_ACC_CTRL : 1;          /*!< SPI_ACC_CTRL                                                              */
      u32 EXT_BASE_ADDR_LOCK : 1;    /*!< EXT_BASE_ADDR_LOCK                                                        */
      u32 CORE_DIS   : 1;            /*!< CORE_DIS                                                                  */
      u32 CORE_DIS_STICKY : 1;       /*!< CORE_DIS_STICKY                                                           */
      u32 CPHA       : 1;            /*!< CPHA                                                                      */
      u32 CPOL       : 1;            /*!< CPOL                                                                      */
      u32 SEQMOD     : 1;            /*!< SEQMOD                                                                    */
      u32 RSVD2      : 1;            /*!< RSVD2                                                                     */
      u32 WPDIS      : 1;            /*!< WPDIS                                                                     */
      u32 WRBREN     : 1;            /*!< WRBREN                                                                    */
      u32 RDBREN     : 1;            /*!< RDBREN                                                                    */
      u32 ADDR_BYTE_LEN : 2;         /*!< ADDR_BYTE_LEN                                                             */
      u32 HOST_CTRL  : 1;            /*!< HOST_CTRL                                                                 */
      u32 N_DUMMY    : 4;            /*!< N_DUMMY                                                                   */
      u32 ACC_SIZE   : 2;            /*!< ACC_SIZE                                                                  */
      u32 INIT       : 1;            /*!< INIT                                                                      */
      u32 RSVD1      : 1;            /*!< RSVD1                                                                     */
    } bit;
} QCC730_SPI_SLAVE_REG_CONFIG_TYPE;

typedef union {
    u32 reg;                         /*!< QCC730_SPI_SLAVE_REG_TRNS_LEN_TYPE                                          */

    struct {
        u32 RSVD       : 16;           /*!< RSVD                                                                      */
        u32 LENGTH     : 16;           /*!< LENGTH                                                                    */
    } bit;
} QCC730_SPI_SLAVE_REG_TRNS_LEN_TYPE;

typedef struct fermion_defaults {
	uint32_t table_hdr; /* Table Header pattern for host verification */
	uint16_t table_len; /* Table length for host verification */
	uint16_t reserved0;

	uint16_t  ferm_maj_ver;  /* Fermion Major version for Host verification */
	uint16_t  ferm_min_ver;  /* Fermion Minor version for Host verification */

	u8 mac_addr[QCC730_MAC_REGION_LEN];  /*Mac address of QCC730 device*/
	uint16_t reserved1;
	
        uint8_t num_a2f_rings;  /* Number of rings supported in A2F direction  */
	uint8_t num_f2a_rings;  /* Number of rings supported in F2A direction  */
	uint16_t reserved2;

	uint8_t a2f_ring0_elem_size; /* Size of each element of A2F ring0 */
	uint8_t f2a_ring0_elem_size; /* Size of each element of F2A ring0 */
	uint8_t a2f_ring0_num_elems; /* Maximum Num elements in A2F ring0 */
	uint8_t f2a_ring0_num_elems; /* Maximum Num elements in A2F ring0 */

	/* A2F_Outdex_Ring_0, A2F_Outdex_Ring_1..........A2F_Outdex_Ring_n       */
	uint32_t p_a2f_ring0_read_idx;
	/* F2A_Index_Ring_0, F2A_Index_Ring_1...... F2A_Index_Ring_n             */
	uint32_t p_f2a_ring0_write_idx;
	/* F2A_Outdex_Ring_0,  F2A_Outdex_Ring_1........ F2A_Outdex_Ring_n       */
	uint32_t p_f2a_ring0_read_idx;
	/* A2F_Index_Ring_0, A2F_Index_Ring_1...... A2F_Index_Ring_n             */
	uint32_t p_a2f_ring0_write_idx;

	/* Base address of A2F Control command Ring */
	uint32_t p_a2f_ring0_base;

	/* Base address of F2A Control event Ring */
	uint32_t p_f2a_ring0_base;

	/*Host Fermion Communication Data Ring*/
	uint8_t a2f_ring1_elem_size; /* Size of each element of A2F ring0 */
	uint8_t f2a_ring1_elem_size; /* Size of each element of F2A ring0 */
	uint8_t a2f_ring1_num_elems; /* Maximum Num elements in A2F ring0 */
	uint8_t f2a_ring1_num_elems; /* Maximum Num elements in A2F ring0 */
	/* A2F_Outdex_Ring_0, A2F_Outdex_Ring_1..........A2F_Outdex_Ring_n       */
	uint32_t p_a2f_ring1_read_idx;
	/* F2A_Index_Ring_0, F2A_Index_Ring_1...... F2A_Index_Ring_n             */
	uint32_t p_f2a_ring1_write_idx;
	/* F2A_Outdex_Ring_0,  F2A_Outdex_Ring_1........ F2A_Outdex_Ring_n       */
	uint32_t p_f2a_ring1_read_idx;
	/* A2F_Index_Ring_0, A2F_Index_Ring_1...... A2F_Index_Ring_n             */
	uint32_t p_a2f_ring1_write_idx;

	/* Base address of A2F Control command Ring */
	uint32_t p_a2f_ring1_base;

	/* Base address of F2A Control event Ring */
	uint32_t p_f2a_ring1_base;

	/* INI parameters */
	void      *p_ini_file;
	uint16_t  max_ini_len;
	uint16_t  reserved3;

	/* Core dump parameters */
	uint32_t p_core_dump;
	uint16_t  core_dump_len;
	uint16_t  reserved4;

	/* Fermion logging parameters */
	uint32_t p_ferm_log_buf;
	uint32_t p_ferm_log_read_idx;
	uint32_t p_ferm_log_write_idx;
	uint16_t  ferm_log_entry_size;
	uint16_t reserved5;
} fermion_defaults_t;

//sw rings
typedef struct htc_ring_element{
    uint32_t addr; /* Pointer to Fermion memory buffer*/
    uint16_t len;  /* Length of the packet */
    uint16_t type; /* Info describing the packet type*/
} htc_ring_element_t;

typedef enum {
    HTC_MSG_TYPE_INVALID, /* Not to be used */
    HTC_MSG_TYPE_WIFI,    /* Wi-Fi control */
    HTC_MSG_TYPE_RAW_ETH,
    HTC_MSG_TYPE_IP_CONNECT,
    HTC_MSG_TYPE_IP_SECURITY,
	HTC_MSG_TYPE_COMMON,
    HTC_MSG_TYPE_MAX,
} htc_msg_type;

typedef enum {
    QCC730_DEVICE_NOT_READY = -1,
	QCC730_WIFI_DISCONNECT,
	QCC730_WIFI_CONNECT,
	QCC730_HANDLER_IDLE,
	QCC730_HANDLER_BUSY,
} qcc730_state;

static int qcc730_spi_rd_sr(struct qcc730_device *qcc730, u32 *sr);

/**
 * A variant of spi_write_then_read(), only for error: 
 * dw_axi_dma_platform 1f0018800.dma: invalid buffer alignment. 
 */

#define SPI_BUFSIZ 64 
static int qcc730_spi_write_then_read(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	static DEFINE_MUTEX(lock);

	int			status;
	struct spi_message	message;
	struct spi_transfer	x[2];
	u8	*local_buf;
    u8  *buf = (u8*)txbuf; 
	
    u8 pad_bytes = (n_tx & 0x03) ? (4 - (n_tx & 0x03)) : (0);
        
        /*
	 * Use preallocated DMA-safe buffer if we can. We can't avoid
	 * copying here, (as a pure convenience thing), but we can
	 * keep heap costs out of the hot path unless someone else is
	 * using the pre-allocated buffer or the transfer is too large.
	 */
	if ((n_tx + n_rx) > SPI_BUFSIZ || !mutex_trylock(&lock)) {
		local_buf = kmalloc(max((unsigned)SPI_BUFSIZ, n_tx + n_rx),
				    GFP_KERNEL | GFP_DMA);
		if (!local_buf)
			return -ENOMEM;
	} else {
		local_buf = buf;
	}

    spi_message_init(&message);
	memset(x, 0, sizeof(x));
	if (n_tx) {
		x[0].len = n_tx;
		spi_message_add_tail(&x[0], &message);
	}
	if (n_rx) {
		x[1].len = n_rx;
		spi_message_add_tail(&x[1], &message);
	}

	memcpy(local_buf, txbuf, n_tx);
	x[0].tx_buf = local_buf;
	x[1].rx_buf = local_buf + n_tx + pad_bytes;

	
    /* Do the I/O */
	status = spi_sync(spi, &message);
	if (status == 0)
		memcpy(rxbuf, x[1].rx_buf, n_rx);

	if (x[0].tx_buf == buf)
		mutex_unlock(&lock);
	else
		kfree(local_buf);

	return status;
}


/* Function to convert big endian byte array to u32 value */
static u32 big_endian_to_u32(const uint8_t *buffer) {
    u32 value = 0;
    value |= ((u32)buffer[0] << 24);
    value |= ((u32)buffer[1] << 16);
    value |= ((u32)buffer[2] << 8);
    value |= buffer[3];
    return value;
}

/* Function to convert u32 value to big endian byte array */
static void u32_to_big_endian(u32 value, uint8_t *buffer) {
    buffer[0] = (value >> 24) & 0xFF;   // Most significant byte (MSB)
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;           // Least significant byte (LSB)
}

static int qcc730_spi_rd_id(struct qcc730_device *qcc730, u16 *id)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_RDID;
    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN, id, sizeof(u16));
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    return 0;
}

static int qcc730_spi_reset(struct qcc730_device *qcc730)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE0;
    qcc730->spi_tx_buff[1] = QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE1;
    qcc730->spi_tx_buff[2] = QCC730_SPI_CMDCODE_RESET_REQUEST_BYTE2;
    qcc730->spi_tx_buff[3] = 0x00;

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, 4, NULL, 0);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    msleep(2);

    u32 sr;
    qcc730_spi_rd_sr(qcc730, &sr);

    return 0;
}

static int qcc730_spi_wren(struct qcc730_device *qcc730)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_WREN;

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN, NULL, 0);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    return 0;
}

static int qcc730_spi_nop(struct qcc730_device *qcc730)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_NOP;

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN, NULL, 0);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    return 0;
}

static int qcc730_spi_rd_sr(struct qcc730_device *qcc730, u32 *sr)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_RDSR;

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN, qcc730->spi_rx_buff, sizeof(u32));
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    *sr = big_endian_to_u32(qcc730->spi_rx_buff);

    return 0;
}

static int qcc730_spi_rd_reg(struct qcc730_device *qcc730, u8 addr, u32 *val)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_IRR;
    qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN] = addr;
    qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN+1] = 0x00;//2 dummy bytes
    qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN+2] = 0x00;

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN+QCC730_SPI_REG_ADDR_LEN+QCC730_SPI_CMDCODE_IRR_DUMMY_BYTES, qcc730->spi_rx_buff, sizeof(u32));
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    *val = big_endian_to_u32(qcc730->spi_rx_buff);
    return 0;
}

static int qcc730_spi_wr_reg(struct qcc730_device *qcc730, u8 addr, u32 val)
{
    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_IRW;
    qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN] = addr;
    u32_to_big_endian(val, &qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN+QCC730_SPI_REG_ADDR_LEN]);

    int ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN+QCC730_SPI_REG_ADDR_LEN+sizeof(val), NULL, 0);
    if (ret != 0) {
        /* Handle error */
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    return 0;
}

static void qcc730_spi_a2f_notify(struct qcc730_device *qcc730)
{
    QCC730_SPI_SLAVE_REG_CONFIG_TYPE val;
    qcc730_spi_rd_reg(qcc730, QCC730_SPI_SLAVE_REG_CONFIG, &val.reg);

    val.bit.HOST_INT = 0x01;

    qcc730_spi_wr_reg(qcc730, QCC730_SPI_SLAVE_REG_CONFIG, val.reg);
}

static int qcc730_spi_a2f_rd(struct qcc730_device *qcc730, u32 addr, u8 *buff, u32 size)
{
    int ret = 0;
    u8 pad_bytes = (size & 0x03) ? (4 - (size & 0x03)) : (0);

    QCC730_SPI_SLAVE_REG_TRNS_LEN_TYPE trns_len = {0};
    trns_len.bit.LENGTH = (u16)(size + pad_bytes);
    ret = qcc730_spi_wr_reg(qcc730, QCC730_SPI_SLAVE_REG_TRNS_LEN, trns_len.reg);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %x\n", __func__, __LINE__, ret);
        return -1;
    }

    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_FREAD;
    u32_to_big_endian(addr, &qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN]);

    ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN+QCC730_SPI_ADDR_LEN+QCC730_SPI_CMDCODE_FREAD_DUMMY_BYTES, buff, size+pad_bytes);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %x\n", __func__, __LINE__, ret);
        return -1;
    }

    u32 sr = 0;
    qcc730_spi_rd_sr(qcc730, &sr);
    if (sr & (1<<24)) {
        printk(KERN_ERR "[%s][%d]: error %x\n", __func__, __LINE__, sr);
        return -1;
    }
    
    return 0;
}

static int qcc730_spi_a2f_wr(struct qcc730_device *qcc730, u32 addr, u8 *buff, u32 size)
{
    int ret = 0;
    u8 pad_bytes = (size & 0x03) ? (4 - (size & 0x03)) : (0);
    
    QCC730_SPI_SLAVE_REG_TRNS_LEN_TYPE trns_len = {0};
    trns_len.bit.LENGTH = (u16)(size + pad_bytes);
    ret = qcc730_spi_wr_reg(qcc730, QCC730_SPI_SLAVE_REG_TRNS_LEN, trns_len.reg);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %x\n", __func__, __LINE__, ret);
        return -1;
    }

    qcc730->spi_tx_buff[0] = QCC730_SPI_CMDCODE_WRITE;
    u32_to_big_endian(addr, &qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN]);
    memcpy(&qcc730->spi_tx_buff[QCC730_SPI_CMDCODE_LEN+QCC730_SPI_ADDR_LEN], buff, size);

    ret = spi_write_then_read(qcc730->spi, qcc730->spi_tx_buff, QCC730_SPI_CMDCODE_LEN+QCC730_SPI_ADDR_LEN+size+pad_bytes, NULL, 0);
    if (ret != 0) {
        printk(KERN_ERR "[%s][%d]: error %d", __func__, __LINE__, ret);
        return -1;
    }

    return 0;
}

static void qcc730_spi_rd_wlan_state(struct qcc730_device *qcc730)
{    
    qcc730_spi_a2f_rd(qcc730, QCC730_HTC_F2A_ASSERT_REASON_CODE, (u8 *)&(qcc730->state), sizeof(u32));
}

static void qcc730_ring_index_update(struct qcc730_device *qcc730)
{
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[0].a2f_read_idx_addr, qcc730->a2f_sring_rd_idx, sizeof(qcc730->a2f_sring_rd_idx));
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[0].a2f_write_idx_addr, qcc730->a2f_sring_wr_idx, sizeof(qcc730->a2f_sring_wr_idx));
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[0].f2a_read_idx_addr, qcc730->f2a_sring_rd_idx, sizeof(qcc730->f2a_sring_rd_idx));
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[0].f2a_write_idx_addr, qcc730->f2a_sring_wr_idx, sizeof(qcc730->f2a_sring_wr_idx));
}

static int qcc730_f2a_ring_is_empty(struct qcc730_device *qcc730, u8 ring)
{
    return qcc730->f2a_sring_rd_idx[ring] == qcc730->f2a_sring_wr_idx[ring];
}

static int qcc730_a2f_ring_is_full(struct qcc730_device *qcc730, u8 ring)
{
    return ((qcc730->a2f_sring_wr_idx[ring] + 1)%qcc730->rings[ring].a2f_elem_num) == qcc730->a2f_sring_rd_idx[ring];
}

static int qcc730_spi_ring_wr(struct qcc730_device *qcc730, u8 *data, u32 len, u8 ring, htc_msg_type type)
{
    if (qcc730_a2f_ring_is_full(qcc730, ring)) {
        printk(KERN_ERR "[%s][%d]: error ring %d full\n", __func__, __LINE__, ring);
        return -1;
    }

    /* read ring element */
    htc_ring_element_t elem;
    memset(&elem, 0, sizeof(elem));
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[ring].a2f_base_addr + qcc730->rings[ring].a2f_elem_size*qcc730->a2f_sring_wr_idx[ring], (u8 *)&elem, sizeof(elem));


    /* write data */
    qcc730_spi_a2f_wr(qcc730, elem.addr, data, len);

    /* update element info */
    elem.len = len;
    elem.type = type;
    qcc730_spi_a2f_wr(qcc730, qcc730->rings[ring].a2f_base_addr + qcc730->rings[ring].a2f_elem_size*qcc730->a2f_sring_wr_idx[ring], (u8 *)&elem, sizeof(elem));
    //printk(KERN_INFO "ring[%hhx] elem[%hhx]: addr %x, len %hx, type %hx\n", ring, qcc730->a2f_sring_wr_idx[ring], elem.addr, elem.len, elem.type);

    /* update a2f ring write index*/
    qcc730->a2f_sring_wr_idx[ring] += 1;
    if (qcc730->a2f_sring_wr_idx[ring] >= qcc730->rings[ring].a2f_elem_num)
        qcc730->a2f_sring_wr_idx[ring] = 0;

    qcc730_spi_a2f_wr(qcc730, qcc730->rings[0].a2f_write_idx_addr, qcc730->a2f_sring_wr_idx, sizeof(qcc730->a2f_sring_wr_idx));

    qcc730_spi_a2f_notify(qcc730);
    return 0;
}

static int qcc730_spi_ring_rd(struct qcc730_device *qcc730, u8 *data, u32 *len, u8 ring, htc_msg_type *type)
{
    if (qcc730_f2a_ring_is_empty(qcc730, ring)) {
        printk(KERN_ERR "[%s][%d]: error ring %d empty\n", __func__, __LINE__, ring);
        return -1;
    }

    /* read element */
    htc_ring_element_t elem;
    memset(&elem, 0, sizeof(elem));
    qcc730_spi_a2f_rd(qcc730, qcc730->rings[ring].f2a_base_addr + qcc730->rings[ring].f2a_elem_size*qcc730->f2a_sring_rd_idx[ring], (u8 *)&elem, sizeof(elem));

    /* printk(KERN_INFO "ring[%hhx] elem[%hhx]: addr %x, len %hx, type %hx\n", ring, qcc730->f2a_sring_rd_idx[ring], elem.addr, elem.len, elem.type); */
    
    /* read data */
    *len = elem.len;
    *type = elem.type;
    qcc730_spi_a2f_rd(qcc730, elem.addr, data, elem.len);

    qcc730->f2a_sring_rd_idx[ring] += 1;
    if (qcc730->f2a_sring_rd_idx[ring] >= qcc730->rings[ring].f2a_elem_num)
        qcc730->f2a_sring_rd_idx[ring] = 0;

    qcc730_spi_a2f_wr(qcc730, qcc730->rings[0].f2a_read_idx_addr, qcc730->f2a_sring_rd_idx, sizeof(qcc730->f2a_sring_rd_idx));
    
    qcc730_spi_a2f_notify(qcc730);

    return 0;
}

static int qcc730_tx_wlan_data(struct qcc730_device *qcc730, u8 *data, int len)
{
    return qcc730_spi_ring_wr(qcc730, data, len, QCC730_SPI_DATA_RING_IDX, HTC_MSG_TYPE_COMMON);
}

static int qcc730_htc_init(struct qcc730_device *qcc730)
{
    u32 sr;
    u16 id;
    u32 ferm_table_addr = 0;

    qcc730_spi_nop(qcc730);
    qcc730_spi_wren(qcc730);

    qcc730_spi_rd_sr(qcc730, &sr);
    printk(KERN_INFO "test sr: %x\n", sr);

    qcc730_spi_rd_id(qcc730, &id);
    printk(KERN_INFO "device id: %hx\n", id);

	if (id != 0x907f) {
		printk(KERN_INFO "invalid device id\n");
		return -1;
	}

    qcc730_spi_a2f_rd(qcc730, QCC730_HTC_TABLE_ADDR_HOLDER, (u8 *)&ferm_table_addr, sizeof(ferm_table_addr));
    
    printk(KERN_INFO "ferm_table_addr mem: %x\n", ferm_table_addr);
    fermion_defaults_t ferm_default_table; 
    qcc730_spi_a2f_rd(qcc730, ferm_table_addr, (u8 *)&ferm_default_table, sizeof(fermion_defaults_t));

    if (ferm_default_table.table_hdr != 0xfe0defa) {
        printk(KERN_ERR "qcc730 init failed\n");
        qcc730_spi_reset(qcc730);
        return -1;
    }

	qcc730_spi_rd_wlan_state(qcc730);

    printk(KERN_INFO "ferm_default_table: size %lu, hdr %x %hx, ferm_ver v%hx.%hx, rings %hhx %hhx\n", sizeof(fermion_defaults_t),
        ferm_default_table.table_hdr, ferm_default_table.table_len, ferm_default_table.ferm_maj_ver, ferm_default_table.ferm_min_ver, 
        ferm_default_table.num_a2f_rings, ferm_default_table.num_f2a_rings);

    /* ring 0 ctrl */ 
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].a2f_base_addr = ferm_default_table.p_a2f_ring0_base;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].a2f_read_idx_addr = ferm_default_table.p_a2f_ring0_read_idx;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].a2f_write_idx_addr = ferm_default_table.p_a2f_ring0_write_idx;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].a2f_elem_size = ferm_default_table.a2f_ring0_elem_size;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].a2f_elem_num = ferm_default_table.a2f_ring0_num_elems;
    
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].f2a_base_addr = ferm_default_table.p_f2a_ring0_base;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].f2a_read_idx_addr = ferm_default_table.p_f2a_ring0_read_idx;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].f2a_write_idx_addr = ferm_default_table.p_f2a_ring0_write_idx;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].f2a_elem_size = ferm_default_table.f2a_ring0_elem_size;
    qcc730->rings[QCC730_SPI_CTRL_RING_IDX].f2a_elem_num = ferm_default_table.f2a_ring0_num_elems;
    
    /* ring 1 data */
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].a2f_base_addr = ferm_default_table.p_a2f_ring1_base;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].a2f_read_idx_addr = ferm_default_table.p_a2f_ring1_read_idx;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].a2f_write_idx_addr = ferm_default_table.p_a2f_ring1_write_idx;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].a2f_elem_size = ferm_default_table.a2f_ring1_elem_size;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].a2f_elem_num = ferm_default_table.a2f_ring1_num_elems;

    qcc730->rings[QCC730_SPI_DATA_RING_IDX].f2a_base_addr = ferm_default_table.p_f2a_ring1_base;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].f2a_read_idx_addr = ferm_default_table.p_f2a_ring1_read_idx;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].f2a_write_idx_addr = ferm_default_table.p_f2a_ring1_write_idx;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].f2a_elem_size = ferm_default_table.f2a_ring1_elem_size;
    qcc730->rings[QCC730_SPI_DATA_RING_IDX].f2a_elem_num = ferm_default_table.f2a_ring1_num_elems;
   
    /*mac address*/ 
    memcpy(qcc730->mac_addr, ferm_default_table.mac_addr, QCC730_MAC_REGION_LEN); 
     int i;
    for (i=0; i<QCC730_MAC_REGION_LEN; i++)
    {    
        printk(KERN_INFO "%02x", qcc730->mac_addr[i]);
    } 
   for (int i=0; i<QCC730_SPI_MAX_RING_NUM; i++)
        printk(KERN_INFO "ring[%d]: a2f: %x %x %x %x %x, f2a: %x %x %x %x %x\n", i, 
                qcc730->rings[i].a2f_base_addr, qcc730->rings[i].a2f_read_idx_addr, qcc730->rings[i].a2f_write_idx_addr, qcc730->rings[i].a2f_elem_size, qcc730->rings[i].a2f_elem_num,
                qcc730->rings[i].f2a_base_addr, qcc730->rings[i].f2a_read_idx_addr, qcc730->rings[i].f2a_write_idx_addr, qcc730->rings[i].f2a_elem_size, qcc730->rings[i].f2a_elem_num);

    printk(KERN_INFO "p_ferm_log_buf %x %x %x %hx\n", ferm_default_table.p_ferm_log_buf, ferm_default_table.p_ferm_log_write_idx, ferm_default_table.p_ferm_log_read_idx, ferm_default_table.ferm_log_entry_size);

    return 0;
}

static void qcc730_ctrl_work(struct work_struct *work) 
{
    struct qcc730_device *qcc730 = container_of(work, struct qcc730_device, ctrl_work);
    
    if (qcc730->state == QCC730_DEVICE_NOT_READY) {
        if (qcc730_htc_init(qcc730) == 0)
            eth_hw_addr_set(qcc730->ndev, qcc730->mac_addr);
    } else {
		qcc730_spi_rd_wlan_state(qcc730);
	}

	if (qcc730->state == QCC730_WIFI_CONNECT) {
            netif_wake_queue(qcc730->ndev);
	}
}
static void qcc730_tx(struct qcc730_device *qcc730)
{
    while (skb_queue_len(&qcc730->txq) && (!qcc730_a2f_ring_is_full(qcc730, QCC730_SPI_DATA_RING_IDX))) {
        struct sk_buff *skb = skb_dequeue(&qcc730->txq);
        qcc730_tx_wlan_data(qcc730, skb->data, skb->len);
        dev_kfree_skb(skb);
        
        if (netif_queue_stopped(qcc730->ndev) && (skb_queue_len(&qcc730->txq) < TX_QUEUE_LOW_WATER))
            netif_wake_queue(qcc730->ndev);
        else
            qcc730_spi_a2f_rd(qcc730, qcc730->rings[0].a2f_read_idx_addr, qcc730->a2f_sring_rd_idx, sizeof(qcc730->a2f_sring_rd_idx));
    }    
}

static void qcc730_rx(struct qcc730_device *qcc730)
{
    while (!qcc730_f2a_ring_is_empty(qcc730, QCC730_SPI_DATA_RING_IDX)) {
        u32 skb_len = 0;
        u32 skb_type = 0;
        struct sk_buff *skb = netdev_alloc_skb(qcc730->ndev, QCC730_MAX_MTU_LEN);
        skb_reserve(skb, NET_IP_ALIGN);

        qcc730_spi_ring_rd(qcc730, &skb->data[0], &skb_len, QCC730_SPI_DATA_RING_IDX, &skb_type);
        skb_put(skb, skb_len);

        skb->dev = qcc730->ndev;
        skb->protocol = eth_type_trans(skb, qcc730->ndev);
        netif_rx(skb);
    }   
}

static void qcc730_txrx_work(struct work_struct *work)
{
    struct qcc730_device *qcc730 = container_of(work, struct qcc730_device, txrx_work);
	
    qcc730->state = QCC730_HANDLER_BUSY;
    mutex_lock(&qcc730->spi_lock);
HANDLER_START:
    qcc730_ring_index_update(qcc730);
    qcc730_tx(qcc730);
	qcc730_rx(qcc730);

    if (qcc730->f2a_irq_pending) {
        qcc730->f2a_irq_pending = 0;
        goto HANDLER_START; 
    } 
    
    mutex_unlock(&qcc730->spi_lock);
    qcc730->state = QCC730_HANDLER_IDLE;
}

static irqreturn_t qcc730_irq_handler(int irq, void *ndev)
{
    struct qcc730_device *qcc730 = (struct qcc730_device*)netdev_priv(ndev);

    if (qcc730->state < QCC730_WIFI_CONNECT) {
		schedule_work(&qcc730->ctrl_work); 
		return IRQ_HANDLED;
	} else if (qcc730->state == QCC730_HANDLER_BUSY) {
        qcc730->f2a_irq_pending = 1; 
        return IRQ_HANDLED; 
    }

    schedule_work(&qcc730->txrx_work);
    
    return IRQ_HANDLED;
}

static int qcc730_open(struct net_device *ndev)
{
    int ret = 0;
 
    printk(KERN_DEBUG "[%s][%d]\n", __func__, __LINE__);
    ret = request_irq(ndev->irq, qcc730_irq_handler, IRQF_TRIGGER_FALLING, ndev->name, ndev);
    if (ret < 0) {
	    printk(KERN_INFO "request qcc730 irq failed %d %d\n", ndev->irq, IRQF_TRIGGER_FALLING);
        return ret; 
    }
    
    netif_start_queue(ndev);
    netif_carrier_on(ndev);
	
    return 0;
}

static int qcc730_stop(struct net_device *ndev)
{
    struct qcc730_device *qcc730 = (struct qcc730_device*)netdev_priv(ndev);

    printk(KERN_DEBUG "[%s][%d]\n", __func__, __LINE__);

    netif_carrier_off(ndev);	
    if (qcc730->state < QCC730_WIFI_CONNECT) {
        netif_stop_queue(ndev);
    }
    __skb_queue_purge(&qcc730->txq);
    cancel_work_sync(&qcc730->txrx_work);

    free_irq(ndev->irq, ndev);
    return 0;
}

static netdev_tx_t qcc730_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct qcc730_device *qcc730 = (struct qcc730_device*)netdev_priv(ndev);
    netdev_tx_t ret = NETDEV_TX_OK;

    if (qcc730->state < QCC730_WIFI_CONNECT) {
        netif_stop_queue(ndev);
	    return NETDEV_TX_BUSY;
    }
	
    if (skb_queue_len(&qcc730->txq) > TX_QUEUE_HIGH_WATER)
    {		
        netif_stop_queue(ndev);
    } 
    skb_queue_tail(&qcc730->txq, skb);
    schedule_work(&qcc730->txrx_work);

    return ret;
}

static const struct net_device_ops qcc730_netdev_ops = {
    .ndo_open       = qcc730_open,
    .ndo_stop       = qcc730_stop,
    .ndo_start_xmit = qcc730_start_xmit
};

static int qcc730_probe(struct spi_device *spi)
{
    int ret = 0;
	struct qcc730_device *qcc730;
    struct net_device *ndev;
	u8 addr[ETH_ALEN];

    printk(KERN_INFO "qcc730_probe\n");
    ndev = devm_alloc_etherdev(&spi->dev, sizeof(*qcc730));
    if (!ndev) {
        printk(KERN_INFO "failed to alloc ethernet device\n");
        return -ENOMEM;
    }		
    SET_NETDEV_DEV(ndev, &spi->dev);
    qcc730 = (struct qcc730_device*)netdev_priv(ndev);
	dev_set_drvdata(&spi->dev, qcc730);
	qcc730->spi = spi;
	qcc730->ndev = ndev;
		
	spi->mode = SPI_MODE_0;
    if (spi_setup(spi)) {
        printk(KERN_INFO "spi_setup fail\n");
        ret = -ENOMEM;
        goto ERR_NDEV;
    }

	mutex_init(&qcc730->spi_lock);
	INIT_WORK(&qcc730->txrx_work, qcc730_txrx_work);
	INIT_WORK(&qcc730->ctrl_work, qcc730_ctrl_work);
	skb_queue_head_init(&qcc730->txq);
	qcc730->state = QCC730_DEVICE_NOT_READY;
	qcc730->f2a_irq_pending = 0;

    /* qcc730 probe */
	if (qcc730_htc_init(qcc730) == 0) {
#if 0		
		addr[0]=0xc4; 
		addr[1]=0xcc; 
		addr[2]=0x37; 
		addr[3]=0xa0; 
		addr[4]=0x08; 
		addr[5]=0xe7; 
		eth_hw_addr_set(ndev, addr);
#endif
	    eth_hw_addr_set(ndev, qcc730->mac_addr);
	} else {
         	memset(addr, 0x08, sizeof(addr));
	        eth_hw_addr_set(ndev, qcc730->mac_addr);
        }	
    ndev->if_port     = IF_PORT_100BASET;
    ndev->netdev_ops  = &qcc730_netdev_ops;

	ret = devm_register_netdev(&spi->dev, ndev);
    if (ret) {
        printk(KERN_INFO "failed to register network device\n");
        goto ERR_NDEV;
    }
	ndev->irq = spi->irq;
    printk(KERN_INFO "spi irq %d!\n", spi->irq);
    printk(KERN_INFO "QCC730 Probe Suc!\n");

        return 0;

ERR_NDEV:
    printk(KERN_INFO "ERR_NDEV!\n");
    free_netdev(ndev);

    return ret;
}

static void qcc730_remove(struct spi_device *spi)
{
    //struct qcc730_device *qcc730 = (struct qcc730_device*)dev_get_drvdata(&spi->dev);
   ;
}


static struct of_device_id qcc730_dt_ids[] = {
    {
        .compatible = "qualcomm,qcc730",
    },
    {}
};

static const struct spi_device_id qcc730_id_table[] = {
    {"qcc730", 0},
    {}
}; 

MODULE_DEVICE_TABLE(spi, qcc730_dt_ids);

static struct spi_driver qcc730_driver = {
    .driver = {
        .name  = DRVNAME_730,
        .owner = THIS_MODULE,
        .of_match_table = qcc730_dt_ids,
        .bus = &spi_bus_type,
        },
        .probe = qcc730_probe,
        .remove = qcc730_remove,
        .id_table = qcc730_id_table, 
};

static int __init qcc730_init(void)
{
    printk(KERN_INFO "qcc730 init!\n");
    return spi_register_driver(&qcc730_driver);
}

static void __exit qcc730_exit(void)
{
    spi_unregister_driver(&qcc730_driver);
}

module_init(qcc730_init);
module_exit(qcc730_exit);

MODULE_DESCRIPTION("Qualcomm qcc730 network driver");
MODULE_AUTHOR("Hit <callme5@163.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("spi:qcc730");

