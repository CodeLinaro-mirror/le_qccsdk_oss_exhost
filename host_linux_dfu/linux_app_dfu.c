/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
/*-------------------------------------------------------------------------
 * Include Files
 * ----------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <assert.h>
#include <linux/spi/spidev.h>
#include "fermion_table_api.h"
#include "qcspi_driver.h"
#include "fermion_dfu_api.h"

#define LOG_FILE "log.bin"
#define DBG_PRINTF(...) printf("FILE: %s, LINE: %d, FUNC: %s :: DBG_LOG:", __FILE__, __LINE__, __func__), printf(__VA_ARGS__);

#define SBL_ELF_NAME NULL
#define WLAN_ELF_NAME "FERMION_IOE_QCLI_DEMO.bin"
#define WLAN_BDF_NAME "bdwlan03.bin"
#define WLAN_REG_DB_NAME NULL
#define WLAN_INI_FILE NULL
#define HOST_CMD_RESET    0x7f
#define HOST_CMD_BOOT    0x3f

#define NUM_OF_RESOURCES (3)
#define FALSE	0
#define TRUE	1
#define PLATFORM_FERMION
static const char *device = "/dev/spidev0.0";
static uint32_t mode;
static uint8_t bits = 8;
static char *input_file;
static char *output_file;
static uint32_t speed = 5000000;
static uint16_t delay;
static int verbose=0;
static char *input_file;
static int boot_flag = 0;

typedef enum resource_index
{
    SBL_IMG_INDEX = 0,
    WLAN_IMG_INDEX,
    BDF_IMG_INDEX,
    REG_DB_IMG_INDEX,
    INI_IMG_INDEX,
    IMG_MAX_INDEX,
}resource_index_e;

typedef enum ring_init_status
{
    RING_INIT_FAIL = 0,
    RING_INIT_FAIL_SBL_FAIL,
    RING_INIT_ENTERED_SBL,
    RING_INIT_FAIL_SBL_OK,
}ring_init_status_e;

typedef enum loader_status
{
    LOADER_FAIL = 0,
    LOADER_SUCCESS,
    NO_IMG_UPGRADE,
}loader_status_e;

typedef enum dfu_ring_state
{
    DFU_RING_INACTIVE = 0,
    DFU_RING_ACTIVE,
}dfu_ring_state_e;

typedef enum resource_update_state
{
    RESOURCE_READY = 0,
    RESOURCE_WAITING_FOR_DFU_START_CRM,
    RESOURCE_TRANSFER,
    RESOURCE_WAIT_FOR_REBOOT,
}resource_update_state_e;

typedef enum resource_validity
{
    RESOURCE_NOT_VALID = 0, /* Fermion has a valid resource*/
    RESOURCE_VALID, /* Fermion has an invalid resource*/
}resource_validity_e;

typedef struct ring_element {
    uint32_t* p_buf; /* Pointer to Fermion memory buffer*/
    uint16_t len;  /* Length of the packet */
    uint16_t info; /* Info describing the packet type */

    /* Scratch buff */
    void* p_buf_start; /* Start of the buf used for freeing */
} ring_element_t;

typedef struct dfu_ring_handle {
    dfu_ring_state_e ring_state; // 0-NOT IN USE, 1-VALID
    uint8_t* ring_base;
    uint32_t* read_index_addr;
    uint32_t* write_index_addr;
    uint8_t read_index;
    uint8_t write_index;
    uint8_t num_elements;
    uint8_t element_size;
} dfu_ring_handle_t;

typedef struct dfu_msg_element {
    void* p_message;
    uint8_t* p_data;
} dfu_msg_element_t;

int qcspi_fd;
uint32_t get_spi_handle(void);

uint32_t get_spi_handle(void)
{
	return qcspi_fd;
}
extern uint32_t get_gpio_handle(void);
extern uint32_t get_spi_dev_num(void);
extern uint32_t get_gpio_dev_num(void);
extern uint32_t* get_spi_handle_ref(void);
extern uint32_t* get_gpio_handle_ref(void);

//extern int32_t ft4222_gpio_write(uint32_t handle, uint32_t portIdx, uint32_t value);

static dfu_ring_handle_t a2f_dfu_ring_handle, f2a_dfu_ring_handle;


typedef struct elf_resource_handle
{
    resource_validity_e fermion_validity;
    char* elf_name;
    char* p_file;
    off_t fsize;
}elf_resource_handle_t;

elf_resource_handle_t elf_resource_handle[IMG_MAX_INDEX] = { {.fermion_validity = RESOURCE_NOT_VALID, .elf_name = SBL_ELF_NAME, .p_file = NULL, .fsize = 0},\
                                                              {.fermion_validity = RESOURCE_NOT_VALID, .elf_name = WLAN_ELF_NAME, .p_file = NULL, .fsize = 0 }, \
                                                              {.fermion_validity = RESOURCE_NOT_VALID, .elf_name = WLAN_BDF_NAME, .p_file = NULL, .fsize = 0}, \
                                                              {.fermion_validity = RESOURCE_NOT_VALID, .elf_name = WLAN_REG_DB_NAME, .p_file = NULL, .fsize = 0 }, \
                                                              {.fermion_validity = RESOURCE_NOT_VALID, .elf_name = WLAN_INI_FILE, .p_file = NULL, .fsize = 0 }, \
};

void qcspi_dev_init()
{
	int ret=0;	
	int tmp_speed=0;
#if 0
#define SPI_CPHA                _BITUL(0)       /* clock phase */
#define SPI_CPOL                _BITUL(1)       /* clock polarity */

#define SPI_MODE_0              (0|0)           /* (original MicroWire) */
#define SPI_MODE_1              (0|SPI_CPHA)
#define SPI_MODE_2              (SPI_CPOL|0)
#define SPI_MODE_3              (SPI_CPOL|SPI_CPHA)
#define SPI_MODE_X_MASK         (SPI_CPOL|SPI_CPHA)

#define SPI_CS_HIGH             _BITUL(2)       /* chipselect active high? */
#define SPI_LSB_FIRST           _BITUL(3)       /* per-word bits-on-wire */
#define SPI_3WIRE               _BITUL(4)       /* SI/SO signals shared */
#define SPI_LOOP                _BITUL(5)       /* loopback mode */
#define SPI_NO_CS               _BITUL(6)       /* 1 dev/bus, no chipselect */
#define SPI_READY               _BITUL(7)       /* slave pulls low to pause */
#define SPI_TX_DUAL             _BITUL(8)       /* transmit with 2 wires */
#define SPI_TX_QUAD             _BITUL(9)       /* transmit with 4 wires */
#define SPI_RX_DUAL             _BITUL(10)      /* receive with 2 wires */
#define SPI_RX_QUAD             _BITUL(11)      /* receive with 4 wires */
#define SPI_CS_WORD             _BITUL(12)      /* toggle cs after each word */
#define SPI_TX_OCTAL            _BITUL(13)      /* transmit with 8 wires */
#define SPI_RX_OCTAL            _BITUL(14)      /* receive with 8 wires */
#define SPI_3WIRE_HIZ           _BITUL(15)      /* high impedance turnaround */
#define SPI_RX_CPHA_FLIP        _BITUL(16)      /* flip CPHA on Rx only xfer */	
#endif
	/* Mode settings */
//	mode |= SPI_CPHA;
	
	qcspi_fd = open(device, O_RDWR);
	if (qcspi_fd < 0)
		printf("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(qcspi_fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		printf("can't set spi mode");

	ret = ioctl(qcspi_fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		printf("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(qcspi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't set bits per word");

	ret = ioctl(qcspi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(qcspi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't set max speed hz");

	ret = ioctl(qcspi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &tmp_speed);
	if (ret == -1)
		printf("can't get max speed hz");

	printf("spi mode: 0x%x\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", tmp_speed, tmp_speed/1000);
	
}

/*
* @brief: For reading a single byte from fermion shared memory
* @param address : address of fermion memory
* @param pdata : reference to the data where the red byte needs to be copied.
* @return void
*/
void read_byte_from_fermion(uint32_t address, uint8_t* pdata)
{
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(uint8_t);
    if(0 != qcspi_read_addr_align(spi_handle, bytes_to_read, address, pdata))
    {
        DBG_PRINTF("read byte address 0x%08X, %d bytes failed.\n", address, bytes_to_read);
        assert(0);
    }
}

/*
* @brief: For writing a single byte to fermion shared memory
* @param address : address of fermion memory
* @param pdata : reference to the data from where the data is copied to fermion memory.
* @return void
*/
void write_byte_to_fermion(uint32_t address, uint8_t* pdata)
{
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(uint8_t);
    qcspi_write_addr_align(spi_handle, bytes_to_read, address, pdata);
}

/*
* @brief: saving the ELF as binary for test purpose
* @param pbuffer : pointer to buffer
* @param len : length of the buffer
* @return bool
*/
bool save_as_binary(char* pbuffer, uint32_t len)
{
    FILE *fd_bin = fopen(LOG_FILE, "wb");
    if(NULL == fd_bin)
    {
        return FALSE;
    }
    fseek(fd_bin, 0, SEEK_SET);
    fwrite(pbuffer, len, 1, fd_bin);
    fclose(fd_bin);
    return TRUE;
}

/*
* @brief: reading the address of the table
* @param add_of_table: reference of the variable where the address of the table needs to be copied.
* @return bool: FALSE if some error happened during reading. TRUE if the read successful
*
*/
bool read_table_address(uint32_t* add_of_table)
{
    uint32_t addr_table;
    uint32_t addr_table_holder = FERM_TABLE_ADDR_HOLDER;
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(uint32_t);
    bool status = TRUE;
    if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, addr_table_holder, (uint8_t*)&addr_table))
    {
        DBG_PRINTF("read table holder address 0x%08X, %d bytes failed.\n", addr_table_holder, bytes_to_read);
        assert(0);
    }
    *add_of_table = addr_table;
    DBG_PRINTF("table add %x=%x\n", addr_table_holder,addr_table);

    return status;
}

/*
* @brief: reading the 1st DWORD of the config table.
* @param dword : Reference of the variable where the table header pattren needs to be copied.
* @return FALSE if some error happened during reading. TRUE if the read succeful
*/
static fermion_dfu_defaults_t gp_ferm_default_table;
bool read_config_table_hdr(uint32_t* dword)
{
    uint32_t addr_table;
    uint32_t table_hdr;
    uint32_t addr_table_holder = FERM_TABLE_ADDR_HOLDER;
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(uint32_t);
    bool status = TRUE;

    read_table_address(&addr_table);
    
    addr_table = 0x90000;
    bytes_to_read = sizeof(fermion_dfu_defaults_t);
    memset(&gp_ferm_default_table, 0, bytes_to_read);
    if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, addr_table, (uint8_t*)&gp_ferm_default_table))
    {
        DBG_PRINTF("read table_hdr address 0x%08X, %d bytes failed.\n", table_hdr, bytes_to_read);
        assert(0);
    }
    *dword = &gp_ferm_default_table.table_hdr;
    DBG_PRINTF("gp_ferm_default_table %x read %d bytes, %x-%x\n", addr_table,bytes_to_read, gp_ferm_default_table.table_hdr, *dword);
    return status;
}

void writeToFile(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        exit(1);
    }
    if (write(fd, value, strlen(value)) == -1) {
        close(fd);
        exit(1);
    }
    close(fd);
}
#define GPIO_PIN 16
#define GPIO_PATH "/sys/class/gpio"
void gpio_set(uint8_t pin, uint8_t value)
{
		char path[50];

    writeToFile(GPIO_PATH "/export", GPIO_PIN);

    snprintf(path, sizeof(path), GPIO_PATH "/gpio%d/direction", pin);
    writeToFile(path, "out");
    
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%d/value", pin);
    if(value==1)
    	writeToFile(path, "1");
    else 
    {
    	writeToFile(path, "0");
    }
    DBG_PRINTF("gpio set %d to value %d\n", pin, value);
}
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io

void new_gpio_set(uint8_t pin, uint8_t value)
{
	setup_io();
	INP_GPIO(pin);
	OUT_GPIO(pin);
	DBG_PRINTF("gpio_set pin=%d value=%d\n", pin, value);
	if(value)
		GPIO_SET = 1<<pin;
	else
		GPIO_CLR = 1<<pin;
}
/*
* @brief: to reboot Fermion using the FTDI GPIO
* @param: void
* @return: void
*/
void dfu_reboot_fermion(int flag)
{
#if 0	
	new_gpio_set(GPIO_PIN, 1); //chip_on test
	usleep(300000);
#if 0
	uint32_t val=0;
	uint32_t spi_handle = get_spi_handle();
#define PMU_SMPS1_5	0x0204304C
#define PMU_SMPS2_5	0x02043058
#define PMU_CORE_12 0x0204307c
//#define SMPS1_GDRV_OVR_OFFSET	22
#define SMPS1_GDRV_OVR_LOW	(2<<22)
#define SMPS1_GDRV_OVR_HIGH	(3<<22)
#define PMU_CORE_LDOAO_VIN_MODE_OFFSET (17)
#define PMU_CORE_LDOAO_VIN_MODE_VBATT (0)
	qcspi_read_addr_align(spi_handle, sizeof(val), PMU_CORE_12, &val);
	DBG_PRINTF("PMU_CORE_12=%x\n ",val);
	val &= 0xfff9ffff;
	qcspi_write_addr_align(spi_handle, sizeof(val), PMU_CORE_12, &val);
	qcspi_read_addr_align(spi_handle, sizeof(val), PMU_CORE_12, &val);
	DBG_PRINTF("2 PMU_CORE_12=%x\n ",val);

	qcspi_read_addr_align(spi_handle, sizeof(val), PMU_SMPS1_5, &val);
	DBG_PRINTF("1 PMU_SMPS1_5=%x\n ",val);
	val |= SMPS1_GDRV_OVR_LOW;
	val |= 0x3000000;
	qcspi_write_addr_align(spi_handle, sizeof(val), PMU_SMPS1_5, &val);
	qcspi_read_addr_align(spi_handle, sizeof(val), PMU_SMPS1_5, &val);
	DBG_PRINTF("2 PMU_SMPS1_5=%x\n ",val);
	
#endif
	DBG_PRINTF("KKK done reset flag=%d\n", flag);
	#define DELAY_TIMING	30
	//toggle reset pin
	new_gpio_set(GPIO_PIN, 0); //chip_on test
	//30ms delay?
	sleep(6);
	DBG_PRINTF("After wait 6");
	new_gpio_set(GPIO_PIN, flag);

#else


#endif	
}

/*
* @brief: to read the boot table which contains the info about the tiny loader parameters
* @param p_boot_table : reference to the variable to where the table needs to be copied.
* @return: void
*/
void read_boot_table(fermion_boot_defaults_t* p_boot_table)
{
    uint32_t addr_table;
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(fermion_boot_defaults_t);
    read_table_address(&addr_table);
    if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, addr_table, (uint8_t*)p_boot_table))
    {
        DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", addr_table, bytes_to_read);
        assert(0);
    }
}
/*
* @brief: to read the load status from 1st DWORD
* @param pdata : pointer to the read variable
* @return: void
*/
void dfu_f2a_read_tiny_load_status(uint32_t* pdata)
{
    uint32_t data;
    DBG_PRINTF("reading the tiny load status in shared memory\n");
    read_config_table_hdr(&data);
    *pdata = data;
}

/*
* @brief: to read the config table which contains the info about DFU ring
* @param p_dfu_table : reference to the variable to where the table needs to be copied.
* @return: void
*/
void read_table_for_dfu(fermion_dfu_defaults_t* p_dfu_table)
{
    uint32_t addr_table;
    uint32_t spi_handle = get_spi_handle();
    uint32_t bytes_to_read = sizeof(fermion_dfu_defaults_t);
    
    read_table_address(&addr_table);
    DBG_PRINTF("read_table_for_dfu addr_table=%x\n",addr_table);
    if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, addr_table, (uint8_t*)p_dfu_table))
    {
        DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", addr_table, bytes_to_read);
        assert(0);
    }
}

/*
* @brief: initialising ring handle for DFU
* @param: void
* @return: void
*/
void dfu_ring_handle_init(void)
{
    fermion_dfu_defaults_t fermion_dfu_defaults;
    fermion_dfu_defaults_t *p_fermion_dfu_param;
DBG_PRINTF("in dfu_ring_handle_init\n");
//to check
    //read_table_for_dfu(&fermion_dfu_defaults);
    p_fermion_dfu_param = &(gp_ferm_default_table);

	/* For initiliasing the a2f dfu handle */
    a2f_dfu_ring_handle.ring_base = p_fermion_dfu_param->p_a2f_dfu_ring_base;
    a2f_dfu_ring_handle.read_index_addr = p_fermion_dfu_param->p_a2f_dfu_read_idx;
    a2f_dfu_ring_handle.write_index_addr = p_fermion_dfu_param->p_a2f_dfu_write_idx;
    read_byte_from_fermion((uint32_t)a2f_dfu_ring_handle.write_index_addr, &(a2f_dfu_ring_handle.write_index));
    a2f_dfu_ring_handle.num_elements = p_fermion_dfu_param->a2f_dfu_ring_num_elems;
    a2f_dfu_ring_handle.element_size = p_fermion_dfu_param->a2f_dfu_ring_elem_size;
    a2f_dfu_ring_handle.ring_state = DFU_RING_ACTIVE;

	/* For initiliasing the f2a dfu handle */
    f2a_dfu_ring_handle.ring_base = p_fermion_dfu_param->p_f2a_dfu_ring_base;
    f2a_dfu_ring_handle.read_index_addr = p_fermion_dfu_param->p_f2a_dfu_read_idx;
    f2a_dfu_ring_handle.write_index_addr = p_fermion_dfu_param->p_f2a_dfu_write_idx;
    read_byte_from_fermion((uint32_t)f2a_dfu_ring_handle.read_index_addr, &(f2a_dfu_ring_handle.read_index));
    f2a_dfu_ring_handle.num_elements = p_fermion_dfu_param->f2a_dfu_ring_num_elems;
    f2a_dfu_ring_handle.element_size = p_fermion_dfu_param->f2a_dfu_ring_elem_size;
    f2a_dfu_ring_handle.ring_state = DFU_RING_ACTIVE;
    if (0xFFFFFFFF != p_fermion_dfu_param->ferm_sbl_ver)
        elf_resource_handle[SBL_IMG_INDEX].fermion_validity = RESOURCE_VALID;
    if (0xFFFFFFFF != p_fermion_dfu_param->ferm_wlan_ver)
        elf_resource_handle[WLAN_IMG_INDEX].fermion_validity = RESOURCE_VALID;
    if (0xFFFFFFFF != p_fermion_dfu_param->ferm_bdf_ver)
        elf_resource_handle[BDF_IMG_INDEX].fermion_validity = RESOURCE_VALID;
    if (0xFFFFFFFF != p_fermion_dfu_param->ferm_reg_db_ver)
        elf_resource_handle[REG_DB_IMG_INDEX].fermion_validity = RESOURCE_VALID;
    if (0xFFFFFFFF != p_fermion_dfu_param->ferm_ini_ver)
        elf_resource_handle[INI_IMG_INDEX].fermion_validity = RESOURCE_VALID;
        
    printf("%s: ----------- F2A ----------- .\r\n", __FUNCTION__);
    printf("base %x; read index add %x; write index add %x num_e=%d, ele size=%d\n",
    		gp_ferm_default_table.p_a2f_dfu_ring_base, gp_ferm_default_table.p_a2f_dfu_read_idx,gp_ferm_default_table.p_f2a_dfu_write_idx,
    		f2a_dfu_ring_handle.num_elements, f2a_dfu_ring_handle.element_size);
    printf("%s: ----------- A2F ----------- .\r\n", __FUNCTION__);
    printf("base %x; read index add %x; write index add %x num_e=%d, ele size=%d\n",
    		a2f_dfu_ring_handle.ring_base, a2f_dfu_ring_handle.read_index_addr,a2f_dfu_ring_handle.write_index_addr,
    		a2f_dfu_ring_handle.num_elements, a2f_dfu_ring_handle.element_size);
}

/*
* @brief: To check the occupancy.
* @param: p_dfu_ring_handle reference to the ring handle.
* @return: bool TRUE if there is a occupancy
*/
bool dfu_ring_check_occupancy(dfu_ring_handle_t* p_dfu_ring_handle)
{
    uint8_t write_index=0;
    uint8_t read_index = 0;

	/* reading the write index */
    read_byte_from_fermion((uint32_t)(p_dfu_ring_handle->write_index_addr), &(write_index));
    if (write_index ^ p_dfu_ring_handle->write_index) // checking for integrity of write index
    {
        DBG_PRINTF("There was a write from fermion on the write index.");
        assert(0);
    }
    else
    {
        read_byte_from_fermion((uint32_t)(p_dfu_ring_handle->read_index_addr), &(read_index));
        uint8_t num_elements = p_dfu_ring_handle->num_elements;
        if(((write_index+1) % num_elements) == read_index)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}


/*
* @brief: To check the emptyness for the ring.
* @param: p_dfu_ring_handle reference to the ring handle.
* @return: bool TRUE if the ring is empty.
*/
bool dfu_ring_check_empty(dfu_ring_handle_t* p_dfu_ring_handle)
{
    uint8_t write_index = 0;
    uint8_t read_index = 0;
	
	/* reading the write index */
    read_byte_from_fermion((uint32_t)(p_dfu_ring_handle->read_index_addr), &(read_index));
    if (read_index ^ p_dfu_ring_handle->read_index) // checking for integrity of read index
    {
        DBG_PRINTF("There was a write from fermion on the write index. %x ^ %x", read_index ,p_dfu_ring_handle->read_index);
        assert(0);
    }
    else
    {
        read_byte_from_fermion((uint32_t)(p_dfu_ring_handle->write_index_addr), &(write_index));
        if (read_index == write_index)
            return true;
        else
            return false;
    }
}

/*
* @brief: To initialise the DFU ring
* @param: void
* @return: status of the init.
*/
ring_init_status_e dfu_ring_init(void)
{
    ring_init_status_e status = RING_INIT_FAIL;
    uint32_t pattern;
    if(true == read_config_table_hdr(&pattern))
    {
        //switch(pattern)
        switch(gp_ferm_default_table.table_hdr)
        {
            case FERM_SBL_CORRUPTED_PATTERN:
                DBG_PRINTF("FERM_SBL_CORRUPTED_PATTERN\n");
                status = RING_INIT_FAIL_SBL_FAIL;
            break;

            case FERM_SBL_ENTERED_PATTERN:
            case 0xf2afac61:
                DBG_PRINTF("FERM_SBL_RING_INIT_ENTERED_SBL\n");
                status = RING_INIT_ENTERED_SBL;
                dfu_ring_handle_init();
            break;

            case FERM_TABLE_HDR_PATTERN:
                DBG_PRINTF("FERM_TABLE_HDR_SUCCESS_PATTERN\n");
                status = RING_INIT_FAIL_SBL_OK;
                
            break;
            
            default:
                DBG_PRINTF("PATTERN FAILURE: %x \n", pattern);
            break;
        }

    }
    else
    {
        DBG_PRINTF("FAILURE \n");
    }
    return status;
}

/*
* @brief: to write the SBL file on the 3rd DWORD onwards
* @param pbuffer : pointer to the read variable
* @param size : size of the buffer
*/
void dfu_tiny_loader_write_sbl(char* pbuffer, uint32_t size)
{
    uint32_t address = 0;
    fermion_boot_defaults_t fermion_boot_defaults;

    DBG_PRINTF("Updating the shared memory\n");
    uint32_t spi_handle = get_spi_handle();
    read_boot_table(&fermion_boot_defaults);
    address = (uint32_t)fermion_boot_defaults.p_sbl_elf_push_addr;
    qcspi_write_addr_align(spi_handle, size, address, pbuffer);
    save_as_binary(pbuffer, size);
}

/*
* @brief: to write the load status
* @param data : data to be written.
*
*/
void dfu_a2f_write_tiny_load_status(uint32_t data)
{
    uint32_t address = 0;
    fermion_boot_defaults_t fermion_boot_defaults;

    DBG_PRINTF("writing the tiny load status in shared memory\n");
    uint32_t spi_handle = get_spi_handle();
    read_boot_table(&fermion_boot_defaults);
    address = (uint32_t)fermion_boot_defaults.p_a2f_status;
    qcspi_write_addr_align(spi_handle, sizeof(uint32_t), address, &data);
}

/*
* @brief: for converting the soft_id to index of the elf_resource_handle array.
* @param: soft_id
* @return: resource_index_e, index for the soft_id
*/
resource_index_e softid_to_elf_table_index(soft_id_t soft_id)
{
    resource_index_e index = IMG_MAX_INDEX;
    switch (soft_id)
    {
    case SBL_ID:
        index = SBL_IMG_INDEX;
        break;
    case WLAN_IMG_ID:
        index = WLAN_IMG_INDEX;
        break;
    case BDF_ID:
        index = BDF_IMG_INDEX;
        break;
    case REGDB_ID:
        index = REG_DB_IMG_INDEX;
        break;
    case WLAN_INI:
        index = INI_IMG_INDEX;
        break;
    }
    if (IMG_MAX_INDEX == index)
    {
        DBG_PRINTF("ERROR invalid index\n");
        assert(0);
    }
    return index;
}

/*
* @brief: to read the elf file and write it on to the shared memory
* @return: loader_status_e, status of the load process.
*/
loader_status_e dfu_tiny_loader_program_sbl_elf(void)
{
    loader_status_e status = LOADER_FAIL;
    uint8_t elf_index = softid_to_elf_table_index(SBL_ID);
    char* pfile_name = elf_resource_handle[elf_index].elf_name;
    char* p_elf_file = elf_resource_handle[elf_index].p_file;
    off_t fsize = elf_resource_handle[elf_index].fsize;
    if(p_elf_file)
    {
        dfu_tiny_loader_write_sbl(p_elf_file, fsize);
		/*updating the 2nd DWORD with status */
        dfu_a2f_write_tiny_load_status(A2F_SBL_ELF_UPDATED); 
        uint32_t f2a_status = 0;
        while(!((F2A_SBL_ELF_UPDATE_SUCCESS == f2a_status) || (F2A_SBL_ELF_UPDATE_FAILED == f2a_status)))
        {
            dfu_f2a_read_tiny_load_status(&f2a_status);
            sleep(1);
            DBG_PRINTF("********f2a_status = %x************\n",f2a_status);
        }
        if(F2A_SBL_ELF_UPDATE_SUCCESS == f2a_status)
        {
            DBG_PRINTF("F2A_SBL_ELF_UPDATE_SUCCESS\n");
			DBG_PRINTF("%s Updated successfully\n", pfile_name);
            free(p_elf_file);
            elf_resource_handle[elf_index].p_file = NULL;
            elf_resource_handle[elf_index].fsize = 0;
#if 0
            elf_resource_handle[SBL_IMG_INDEX].fermion_validity = RESOURCE_VALID;
            if (remove(pfile_name))
            {
                DBG_PRINTF("ERROR: Unable to remove SBL file\n")
            }
#endif
            status = LOADER_SUCCESS;
        }
        
    }
    return status;
}

/*
* @brief: to write an element to the DFU ring
* @param pdata : reference for the data
* @param len : length of the data
* @return: status
*/
bool a2f_dfu_ring_write_element(uint8_t* pdata, uint32_t* len)
{
    bool status = false;
	uint32_t data_len = 0;
    msg_hdr_t* p_msg_hdr;
    dfu_msg_element_t* p_dfu_msg_element = (dfu_msg_element_t*)pdata;
    p_msg_hdr = (msg_hdr_t*)p_dfu_msg_element->p_message;

    switch (p_msg_hdr->msg_id)
    {
    case A2F_DFU_DATA_TRANSFER_CFM:
    {
        a2f_dfu_transfer_cfm_t* p_a2f_dfu_transfer_cfm = (a2f_dfu_transfer_cfm_t*)p_dfu_msg_element->p_message;
        data_len = p_a2f_dfu_transfer_cfm->program_len;
        break;
    }

    default:
    	data_len = 0;
        break;

    }
	if(data_len > (*len))
	{
		DBG_PRINTF("data_len is > the whole message length check message framing..\n");
		assert(0);
	}
    uint32_t bytes_to_write = (*len) - data_len;
    if (true == dfu_ring_check_occupancy(&a2f_dfu_ring_handle))
    {
        ring_element_t *p_ring_element = (ring_element_t*)a2f_dfu_ring_handle.ring_base;
        p_ring_element = p_ring_element + a2f_dfu_ring_handle.write_index;
        ring_element_t ferm_ring_element;
        uint32_t spi_handle = get_spi_handle();
        uint32_t bytes_to_read = sizeof(ring_element_t);
        if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, p_ring_element, (uint8_t*)(&ferm_ring_element)))
        {
            DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", p_ring_element, bytes_to_read);
            assert(0);
        }
        if ((*len) <= ferm_ring_element.len)
        {
            uint8_t* p_ferm_data = (uint8_t*)ferm_ring_element.p_buf;
            qcspi_write_addr_align(spi_handle, bytes_to_write, p_ferm_data, p_dfu_msg_element->p_message);
            p_ferm_data = p_ferm_data + bytes_to_write;
            bytes_to_write = data_len;
            if(bytes_to_write>1400)
          	{
          		//bytes_to_write = 1024*4;
          		//DBG_PRINTF("kkk correct write spi len to 1400\n");
          	}
            if((bytes_to_write) && (p_dfu_msg_element->p_data))
            	qcspi_write_addr_align(spi_handle, bytes_to_write, p_ferm_data, p_dfu_msg_element->p_data);
            	
            a2f_dfu_ring_handle.write_index = (a2f_dfu_ring_handle.write_index+1)%a2f_dfu_ring_handle.num_elements;
            write_byte_to_fermion((uint32_t)a2f_dfu_ring_handle.write_index_addr, &(a2f_dfu_ring_handle.write_index));
            status = true;
        }
        else
        {
            status = false;
        }
    }
    else
    {
        status = false;
    }
    return status;
}

/*
* @brief: to read an element from the DFU ring
* @param pdata : reference for the data
* @param len : length of the data
* @return: status
*/
bool f2a_dfu_ring_read_element(uint8_t* pdata, uint32_t* len)
{
    bool status = false;
    uint32_t bytes_to_read;
    dfu_msg_element_t* p_dfu_msg_element = (dfu_msg_element_t*)pdata;
    if (false == dfu_ring_check_empty(&f2a_dfu_ring_handle))
    {
        ring_element_t* p_ring_element = (ring_element_t*)f2a_dfu_ring_handle.ring_base;
        p_ring_element = p_ring_element + f2a_dfu_ring_handle.read_index;
        ring_element_t ferm_ring_element;
        uint32_t spi_handle = get_spi_handle();
        bytes_to_read = sizeof(ring_element_t);
        if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, p_ring_element, (uint8_t*)(&ferm_ring_element)))
        {
            DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", p_ring_element, bytes_to_read);
            assert(0);
        }
        bytes_to_read = sizeof(msg_hdr_t);
        if (bytes_to_read <= ferm_ring_element.len)
        {
            msg_hdr_t msg_hdr;
			void *p_message = NULL;
            if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, ferm_ring_element.p_buf, (uint8_t*)(&msg_hdr)))
            {
                DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", ferm_ring_element.p_buf, bytes_to_read);
                assert(0);
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

            /* reading the message */
            if (bytes_to_read <= ferm_ring_element.len)
            {
                if (bytes_to_read)
                {
                    uint8_t* p_message = (uint8_t*)malloc(bytes_to_read);
                    if (p_message)
                    {
                        uint8_t* p_ferm_data = (uint8_t*)ferm_ring_element.p_buf;
                        if (0 != qcspi_read_addr_align(spi_handle, bytes_to_read, p_ferm_data, p_message))
                        {
                            DBG_PRINTF("table address 0x%08X, %d bytes failed.\n", p_ferm_data, bytes_to_read);
							free(p_message);
                            assert(0);
                        }
                        p_dfu_msg_element->p_message = p_message;
						*len = bytes_to_read;
                        status = true;
                    }
                    else
                        DBG_PRINTF("malloc failed\n");
                }
                else
                {
                    status = false;
                    DBG_PRINTF("no message information.\n");
                }
            }
            else
                DBG_PRINTF("the bytes to read is more than the initialised size.\n");

            /* Since there is no data from F2A, just make the p_data to NULL */
			p_dfu_msg_element->p_data = NULL;

            f2a_dfu_ring_handle.read_index = (f2a_dfu_ring_handle.read_index + 1)% f2a_dfu_ring_handle.num_elements;
            write_byte_to_fermion((uint32_t)f2a_dfu_ring_handle.read_index_addr, &(f2a_dfu_ring_handle.read_index));
        }
        else
            status = false;
    }
    else
        status = false;
    return status;
}

/*
* @brief: For sending the DFU start request to indicate fermion.
* @param soft_id : soft id
* @return: status
*/
bool send_dfu_start_request(soft_id_t soft_id)
{
    bool status;
    a2f_dfu_start_req_t a2f_dfu_start_req;
	  dfu_msg_element_t dfu_msg_element;
    memset(&a2f_dfu_start_req, 0, sizeof(a2f_dfu_start_req_t));
    a2f_dfu_start_req.header.msg_id = A2F_DFU_REQ;
    a2f_dfu_start_req.soft_id_bitmap = soft_id;
		if(boot_flag == 0x1){
			DBG_PRINTF("host send req reset\n");
			a2f_dfu_start_req.soft_id_bitmap = CMD_RESET;
			soft_id = BDF_ID;
		}
		else if(boot_flag == 0x2)
		{
			DBG_PRINTF("host send req boot\n");
			a2f_dfu_start_req.soft_id_bitmap = CMD_BOOT;
			soft_id = BDF_ID; //in case of softid_to_elf_table_index crash
		}
    //image index and soft_id should match
		uint8_t file_index=0;
		file_index = softid_to_elf_table_index(soft_id);
    a2f_dfu_start_req.dfu_imgsize = elf_resource_handle[file_index].fsize;
    DBG_PRINTF("send_dfu_start_request size=%d, id=%d\n", a2f_dfu_start_req.dfu_imgsize, soft_id);
		dfu_msg_element.p_message = &a2f_dfu_start_req;
		dfu_msg_element.p_data = NULL;
    uint32_t length = sizeof(a2f_dfu_start_req_t);
    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element,&length);
    return status;
}

/*
* @brief: For receiving the DFU start confirm response
* @param: void
* @return: status
*/
bool recieve_dfu_start_confirm(void)
{
    uint32_t len;
    bool read_element_status = false, dfu_start_cfm_status = false;
    dfu_msg_element_t dfu_msg_element;
	f2a_dfu_start_cfm_t *p_f2a_dfu_start_cfm;
	
    memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));
    read_element_status = f2a_dfu_ring_read_element((uint8_t*)&dfu_msg_element, &len);
	
    if (true == read_element_status)
    {
        p_f2a_dfu_start_cfm = dfu_msg_element.p_message;
        if (F2A_DFU_CFM == p_f2a_dfu_start_cfm->header.msg_id)
        {
            DBG_PRINTF("F2A_DFU_UPDATE_CFM recieved\n");
            dfu_start_cfm_status = true;
        }
        else
        {
            DBG_PRINTF("ERROR: Recieved frame not equals  F2A_DFU_UPDATE_CFM\n");
            dfu_start_cfm_status = false;
        }
		free(dfu_msg_element.p_message);
		
        if (dfu_msg_element.p_data == NULL)
        {
            DBG_PRINTF("WARNING: there is no data: which is expected \n");
        }
        else
        {
            DBG_PRINTF("WARNING: there is data: which is not expected \n");
            free(dfu_msg_element.p_data);
        }
		
    }
    return dfu_start_cfm_status;
}

/*
* @brief: For sending the dfu data transfer confirm message.
* @param: p_a2f_dfu_transfer_cfm reference to the response header.
* @param: pdata, reference to the data which needs to be attached to the message.
* @return: status
*/
bool send_dfu_transfer_confirm(a2f_dfu_transfer_cfm_t* p_a2f_dfu_transfer_cfm, uint8_t *pdata)
{
    bool status;
    dfu_msg_element_t dfu_msg_element;

	memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));
	dfu_msg_element.p_data = pdata;
    dfu_msg_element.p_message = p_a2f_dfu_transfer_cfm;
    uint32_t length = sizeof(a2f_dfu_transfer_cfm_t) - MAX_DFU_DATA_LEN + p_a2f_dfu_transfer_cfm->program_len;
    DBG_PRINTF("send_dfu_transfer_confirm length=%d, program len=%d\n", length, p_a2f_dfu_transfer_cfm->program_len);
    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element, &length);
    return status;
}

/*
* @brief: For sending the dfu data transfer done confirmation to fermion.
* @param: void
* @return: status
*/
bool send_dfu_transfer_done_cfm(void)
{
    bool status;
    dfu_msg_element_t dfu_msg_element;
	a2f_dfu_transfer_done_rsp_t a2f_dfu_transfer_done_rsp;

    memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));
    a2f_dfu_transfer_done_rsp.header.msg_id = A2F_DFU_DATA_TRANSFER_DONE_RSP;
    a2f_dfu_transfer_done_rsp.more_resource = 0;
	a2f_dfu_transfer_done_rsp.header.status = DFU_MSG_SUCCESS;
	dfu_msg_element.p_data = NULL;
	dfu_msg_element.p_message = &a2f_dfu_transfer_done_rsp;
    uint32_t length = sizeof(a2f_dfu_transfer_done_rsp_t);
    status = a2f_dfu_ring_write_element((uint8_t*)&dfu_msg_element, &length);
    return status;
}

/*
* @brief: For recieving the DFU frmes and processing it.
* @param: p_msg_id, reference to the command id.
* @return: void
*/
void recieve_and_process_dfu_frame(f2a_dfu_msg_id_t* p_msg_id)
{
    uint32_t len;
    bool read_element_status = false;

    dfu_msg_element_t dfu_msg_element;
    memset(&dfu_msg_element, 0, sizeof(dfu_msg_element_t));
    read_element_status = f2a_dfu_ring_read_element((uint8_t*)&dfu_msg_element, &len);

    if (true == read_element_status)
    {
        msg_hdr_t *p_msg_hdr = (msg_hdr_t*)dfu_msg_element.p_message;
        *p_msg_id = p_msg_hdr->msg_id;
        
        DBG_PRINTF("in recieve_and_process_dfu_frame id=%d, p_mes=%p, p_data=%p\n", 
        			p_msg_hdr->msg_id, dfu_msg_element.p_message,dfu_msg_element.p_data)
        switch(p_msg_hdr->msg_id)
        {
            case F2A_DFU_CFM:
                DBG_PRINTF("Warning: this command is a stranger to the state named RESOURCE_TRANSFER \n");
            break;
            case F2A_DFU_DATA_TRANSFER_REQ:
            {
                f2a_dfu_transfer_req_t* p_f2a_dfu_transfer_req;
                uint32_t data_len;
                uint32_t elf_offset;
                soft_id_t soft_id;
                p_f2a_dfu_transfer_req = (f2a_dfu_transfer_req_t*)(dfu_msg_element.p_message);
                elf_offset = p_f2a_dfu_transfer_req->elf_offset;
                soft_id = p_f2a_dfu_transfer_req->soft_id;
                DBG_PRINTF("receive program_len=%x softid=%d\n ",p_f2a_dfu_transfer_req->program_len, p_f2a_dfu_transfer_req->soft_id);
               // if(p_f2a_dfu_transfer_req->program_len=0xaabbccdd)
               // 	data_len = elf_resource_handle[soft_id].fsize;
               // else 
               // {
                	data_len = p_f2a_dfu_transfer_req->program_len;
               // }
                
                a2f_dfu_transfer_cfm_t a2f_dfu_transfer_cfm;
                memset(&a2f_dfu_transfer_cfm, 0, sizeof(a2f_dfu_transfer_cfm_t));
                a2f_dfu_transfer_cfm.program_len = data_len;
                a2f_dfu_transfer_cfm.elf_offset = elf_offset;
                a2f_dfu_transfer_cfm.soft_id = soft_id;
				a2f_dfu_transfer_cfm.header.msg_id = A2F_DFU_DATA_TRANSFER_CFM;
				a2f_dfu_transfer_cfm.header.status = DFU_MSG_SUCCESS;
                uint8_t* pdata;
                uint8_t elf_index = softid_to_elf_table_index(soft_id);
                char* p_elf_file = elf_resource_handle[elf_index].p_file;
                pdata = p_elf_file + a2f_dfu_transfer_cfm.elf_offset;
                //DBG_PRINTF(" dfu tranfer confirm elf soft_id=%d index=%d, offset=%d, pdata=%p\n", 
                //	soft_id, elf_index, a2f_dfu_transfer_cfm.elf_offset, pdata);
                send_dfu_transfer_confirm(&a2f_dfu_transfer_cfm, pdata);
            }    
            break;

			case F2A_DFU_DATA_TRANSFER_DONE_IND:
			{
				DBG_PRINTF("F2A_DFU_DATA_TRANSFER_DONE_IND cmd recieved\n");
				f2a_dfu_transfer_done_ind_t* p_f2a_dfu_transfer_done_ind;
				soft_id_t soft_id;
				p_f2a_dfu_transfer_done_ind = (f2a_dfu_transfer_done_ind_t*)(dfu_msg_element.p_message);
				soft_id = p_f2a_dfu_transfer_done_ind->soft_id;

				uint8_t elf_index = softid_to_elf_table_index(soft_id);
				char* p_elf_file = elf_resource_handle[elf_index].p_file;
				free(p_elf_file);
				elf_resource_handle[elf_index].p_file = NULL;
                elf_resource_handle[elf_index].fsize = 0;
				send_dfu_transfer_done_cfm();
#if 0
                char* pfile_name = elf_resource_handle[elf_index].elf_name;
                elf_resource_handle[elf_index].fermion_validity = RESOURCE_VALID;
                if (remove(pfile_name))
                {
                    DBG_PRINTF("ERROR: Unable to remove %s file\n", pfile_name)
                }
#endif
			}	 
			break;


        }

        if (dfu_msg_element.p_data == NULL)
        {
            DBG_PRINTF("Warning: there is no data: which is expected \n");
        }
        else
        {
            DBG_PRINTF("Warning: there is data: which is not expected \n");
            free(dfu_msg_element.p_data);
        }
        
    }

}

/*
* @brief: For reading the elf resources for DFU
* @param: void
* @return: void
*/
void read_elf_files(void)
{
    for (int i = 0; i < NUM_OF_RESOURCES; i++)
    {
        char* pfile_name = elf_resource_handle[i].elf_name;
        if (pfile_name)
        {
            int32_t fd;
            off_t fsize;
            fd = open(pfile_name, O_RDONLY);
            if (fd < 0) {
                DBG_PRINTF("ERROR %d Unable to open %s\n", fd, pfile_name);
                continue;
            }
            fsize = lseek(fd, 0, SEEK_END);
            DBG_PRINTF("********filesize = %d************\n", fsize);
            char* p_file = (char*)malloc(fsize);
            if (p_file)
            {
                lseek(fd, (off_t)0, SEEK_SET);
                read(fd, (void*)p_file, fsize);
                close(fd);
                elf_resource_handle[i].p_file = p_file;
                elf_resource_handle[i].fsize = fsize;
            }
            else
            {
                close(fd);
                DBG_PRINTF("ERROR %d Unable to allocate memory %s\n", fd, pfile_name);
                assert(0);
            }
        }
    }

}


/*
* @brief: main state machine which communicates to device for the DFU \
*         after a valid SBL image present in the device.
* @param void
* @return: status
*/
loader_status_e dfu_normal_loader_program_elf(void)
{
    loader_status_e status = LOADER_FAIL;
    resource_update_state_e resouce_update_state = RESOURCE_READY;
    uint8_t loader_loop_on=1;
    soft_id_t soft_id = IMG_NIL;
    DBG_PRINTF("enter dfu_normal_loader_program_elf resouce_update_state=%d\n",resouce_update_state);
    while (loader_loop_on)
    {
        switch (resouce_update_state)
        {
            case RESOURCE_READY:
            {
				if(boot_flag == 0x1)
				{
					 if (true == send_dfu_start_request(HOST_CMD_RESET)){
					 	DBG_PRINTF("host send dfu request 0x7f RESET\n");
						return NO_IMG_UPGRADE;
					 }
				}
				else if(boot_flag == 0x2)
				{		 
					if (true == send_dfu_start_request(HOST_CMD_BOOT)){
					 	DBG_PRINTF("host send dfu request 0x3f boot\n");
						return NO_IMG_UPGRADE;
					 }
				}
				
                soft_id = IMG_NIL;
                uint8_t elf_index = 0;
                elf_index = softid_to_elf_table_index(SBL_ID);
                if (elf_resource_handle[elf_index].p_file)
                    soft_id |= SBL_ID;

                elf_index = softid_to_elf_table_index(WLAN_IMG_ID);
                if (elf_resource_handle[elf_index].p_file)
                    soft_id |= WLAN_IMG_ID;

                elf_index = softid_to_elf_table_index(BDF_ID);
                if (elf_resource_handle[elf_index].p_file)
                    soft_id |= BDF_ID;

                elf_index = softid_to_elf_table_index(REGDB_ID);
                if (elf_resource_handle[elf_index].p_file)
                    soft_id |= REGDB_ID;

                elf_index = softid_to_elf_table_index(WLAN_INI);
                if (elf_resource_handle[elf_index].p_file)
                    soft_id |= WLAN_INI;

				if(0==strncmp(elf_resource_handle[WLAN_IMG_INDEX].elf_name, input_file, 
						strlen(elf_resource_handle[WLAN_IMG_INDEX].elf_name)))
				{
					soft_id = WLAN_IMG_ID;
				}
				else if(0==strncmp(elf_resource_handle[BDF_IMG_INDEX].elf_name, input_file, 
						strlen(elf_resource_handle[BDF_IMG_INDEX].elf_name)))
				{
					soft_id = BDF_ID;
				}
                if (IMG_NIL == soft_id)
                {
                    for (int i = 0; i < NUM_OF_RESOURCES; i++)
                    {
                        if (RESOURCE_VALID != elf_resource_handle[i].fermion_validity)
                        {
                            DBG_PRINTF("*****%s NOT UPDATED IN THE PRODUCT LIFE*****\n", elf_resource_handle[i].elf_name);
                            DBG_PRINTF("*****Please add the resource file and again start the application*****\n");
                            assert(0);
                        }
                    }
                }
                DBG_PRINTF("soft_id=%d input_file=%s\n",soft_id, input_file);
                if (true == send_dfu_start_request(soft_id))
                    resouce_update_state = RESOURCE_WAITING_FOR_DFU_START_CRM;
                break;
            }

            case RESOURCE_WAITING_FOR_DFU_START_CRM:
            	DBG_PRINTF("RESOURCE_WAITING_FOR_DFU_START_CRM\n");
                if(true == recieve_dfu_start_confirm())
                {
                    if (IMG_NIL == soft_id)
                    {
                        status = NO_IMG_UPGRADE;
                        loader_loop_on = 0;
                    }
                    else
                        resouce_update_state = RESOURCE_TRANSFER;
                }
                break;

            case RESOURCE_TRANSFER:
            {
                f2a_dfu_msg_id_t msg_id;
                //DBG_PRINTF("start recieve_and_process_dfu_frame\n");
                recieve_and_process_dfu_frame(&msg_id);
								//DBG_PRINTF("after recieve_and_process_dfu_frame id=%d\n", msg_id);
                switch (msg_id)
                {
									case F2A_DFU_DATA_TRANSFER_DONE_IND:
											status = NO_IMG_UPGRADE;
											return status;
	                case F2A_DFU_REBOOT_REQ:
	                    DBG_PRINTF("F2A_DFU_REBOOT_IND cmd recieved, reboot or continue upgrade!\n");
											status = NO_IMG_UPGRADE;
											return status;
	                    resouce_update_state = RESOURCE_WAIT_FOR_REBOOT;
	                    break;
                }
            }
                break;

            case RESOURCE_WAIT_FOR_REBOOT:
                dfu_reboot_fermion(1);
                sleep(1);
                loader_loop_on = 0;
                status = LOADER_SUCCESS;
                break;
        }
    }

    return status;
}

/*
* @brief: entry function for normal loader
* @param: void
* @return: void
*/
loader_status_e dfu_normal_loader(void)
{
    bool status = false;
    //int32_t fd;
    DBG_PRINTF("inside normal dfu\n");
    return (dfu_normal_loader_program_elf());
}


static void parse_opts(int argc, char **argv)
{
	while (1) {
		/*static const struct option lopts[] = {
			{ "file",  1, 0, 'f' },
			{ "reboot",   1, 0, 'r' },

			{ NULL, 0, 0, 0 },
		};*/
		int c;

		//c = getopt_long(argc, argv, "f:r",
		//		lopts, NULL);
		c = getopt(argc, argv, "f:rb::");
		DBG_PRINTF("getopt=%c %x \n", c,c);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			boot_flag = 0;
			input_file = optarg;
			break;
		case 'r':
			boot_flag = 1;//atoi(optarg);
			DBG_PRINTF("flag=%d arg=%s\n", boot_flag, optarg);
			dfu_reboot_fermion(boot_flag);
			break;
		case 'b':
		default:
			boot_flag = 2;//atoi(optarg);
			DBG_PRINTF("flag=%d \n", boot_flag);
			//print_usage(argv[0]);
			break;
		}
	}

}

/*
* @brief: entry function for dfu
* @param: void
* @return: void
*/
void main(int argc, char* argv[])
{
    uint32_t spi_handle;
		qcspi_dev_init();
	
    spi_handle = get_spi_handle();
#if 0    
    /*test*/
    uint8_t read_buf[32]={0};
    int address=0;
    int len = 0;
    int config_status;
    printf("fd=%x\n",qcspi_fd);
    qcspi_RDSR(qcspi_fd, &config_status);
    printf("rdsr config=%x\n", config_status);


    len=16;
    qcspi_read_addr_align(qcspi_fd, len, address, read_buf);
    for(int i=0;i<len;i++)
    {
    printf(" %x ",read_buf[i]);
    }
#endif 
    uint8_t array[3]={0};
    qcspi_get_slaveid(array);
    printf("qcspi slave id %x %x %x\n",array[0],array[1],array[2]);

    printf("\n");
    parse_opts(argc, argv);
#if 0    
		if(boot_flag==1)
		{
			printf("host do HW reset\n");
			return;
		}
#endif
    if (spi_handle)
    {
        read_elf_files();
        while (1)
        {
            // waiting for ring init from Fermion
            ring_init_status_e status = RING_INIT_FAIL;
            while (RING_INIT_FAIL == status)
            {
                status = dfu_ring_init();
                sleep(1);
            }

            switch (status)
            {


            case RING_INIT_ENTERED_SBL:
                if (NO_IMG_UPGRADE == dfu_normal_loader())
                    return;
                break;

            case RING_INIT_FAIL_SBL_OK:
                return;
            }

        }
    }
    else
    {
        DBG_PRINTF("spi interface is not yet up\n")
    }
}

