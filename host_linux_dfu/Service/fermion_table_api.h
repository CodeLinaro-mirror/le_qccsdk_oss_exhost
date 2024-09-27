/*========================================================================
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear

* @file fermion_table_api.h
* @brief Fermion initialization table APIs/Defs to be shared with Apps
*========================================================================*/
#ifndef FERMION_TABLE_API_H
#define FERMION_TABLE_API_H

/*------------------------------------------------------------------------
* Include Files
* ----------------------------------------------------------------------*/
#include "fermion_cmn_api.h"

/*------------------------------------------------------------------------
* Preprocessor Definitions and Constants
* ----------------------------------------------------------------------*/
#ifdef PLATFORM_FERMION
/* TBD: Will be updated after Fermion init and list files are ready */
#define FERM_TABLE_ADDR_HOLDER   (0x000001f4)
#else /* PLATFORM_FERMION */
/* Updated based on Ferm-NT file frn_wlan_sys_image.ld */
#define FERM_TABLE_ADDR_HOLDER   (0x000001f4)
#endif /* PLATFORM_FERMION */

#define FERM_SBL_CORRUPTED_PATTERN (0xF2A0DEAD)
#define A2F_SBL_ELF_UPDATED        (0xA2F00001)
#define F2A_SBL_ELF_UPDATE_SUCCESS (0xF2A00001)
#define F2A_SBL_ELF_UPDATE_FAILED  (0xF2A00002)
#define FERM_SBL_ENTERED_PATTERN   (0xF2AFACE1)

#define FERM_TABLE_HDR_PATTERN  (0xFE0DEFA)
#define FERM_MAJOR_VER          (0)
#define FERM_MINOR_VER          (2)

/* Macro for early drop version 04/18 */
//#define NEW_RING1
/*------------------------------------------------------------------------
* Type Declarations
* ----------------------------------------------------------------------*/
/* The table shared to host when there is an invalid SBL image */
typedef struct fermion_boot_defaults {

uint32_t table_hdr; /* Table Header pattern for host verification */
uint16_t table_len; /* Table length for host verification */
uint16_t reserved;

/* DFU parameters for sbl download */
uint32_t *p_a2f_status; /* the address to which the dfu status for Fermion sbl gets updated */
void *p_sbl_elf_push_addr; /* the address to which ermion sbl gets updated */
uint32_t max_sbl_elf_size; /* The max size allocated for SBL elf in Fermion memory*/
} fermion_boot_defaults_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(fermion_boot_defaults_t);
FERM_STRUCT_SIZE_SYNC(fermion_boot_defaults_t, (20));

/* The table shared to host when SBL image boots up */
typedef struct fermion_dfu_defaults {
uint32_t table_hdr; /* Table Header pattern for host verification */
uint16_t table_len; /* Table length for host verification */
uint16_t reserved1;

/* DFU parameters for WLAN, REG_DB, BDF, INI download */
uint32_t ferm_pbl_ver; /* Fermion pbl version for Host verification */
uint32_t ferm_wlan_ver; /* Fermion wlan version for Host verification */
uint32_t ferm_sbl_ver; /* Fermion sbl version for Host verification */
uint32_t ferm_bdf_ver; /* Fermion bdf version for Host verification */
uint32_t ferm_reg_db_ver; /* Fermion reg_db version for Host verification */
uint32_t ferm_ini_ver; /* Fermion ini version for Host verification */

uint8_t a2f_dfu_ring_elem_size; /* size of each elememnt of A2F DFU ring */
uint8_t f2a_dfu_ring_elem_size; /* size of each element of F2A DFU ring */
uint8_t a2f_dfu_ring_num_elems; /* Maximum Num elements in A2F DFU ring */
uint8_t f2a_dfu_ring_num_elems; /* Maximum Num elements in A2F DFU ring */

uint32_t *p_a2f_dfu_read_idx; /* a2f DFU read index */
uint32_t *p_f2a_dfu_write_idx; /* f2a DFU write index */

uint32_t *p_f2a_dfu_read_idx; /* f2a DFU read idx */
uint32_t *p_a2f_dfu_write_idx; /* a2f DFU write idx */

/* Base address of A2F DFU Ring */
void *p_a2f_dfu_ring_base;

/* Base address of F2A DFU Ring */
void *p_f2a_dfu_ring_base;
} fermion_dfu_defaults_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(fermion_dfu_defaults_t);
FERM_STRUCT_SIZE_SYNC(fermion_dfu_defaults_t, (60));

/* The table shared to host when wlan image boots up */
typedef struct fermion_defaults {
	uint32_t table_hdr; /* Table Header pattern for host verification */
	uint16_t table_len; /* Table length for host verification */
	uint16_t reserved1;

	uint16_t  ferm_maj_ver;  /* Fermion Major version for Host verification */
	uint16_t  ferm_min_ver;  /* Fermion Minor version for Host verification */

	uint8_t num_a2f_rings;  /* Number of rings supported in A2F direction  */
	uint8_t num_f2a_rings;  /* Number of rings supported in F2A direction  */
	uint16_t reserved2;

	uint8_t a2f_ring0_elem_size; /* Size of each element of A2F ring0 */
	uint8_t f2a_ring0_elem_size; /* Size of each element of F2A ring0 */
	uint8_t a2f_ring0_num_elems; /* Maximum Num elements in A2F ring0 */
	uint8_t f2a_ring0_num_elems; /* Maximum Num elements in A2F ring0 */

	/* A2F_Outdex_Ring_0, A2F_Outdex_Ring_1..........A2F_Outdex_Ring_n       */
	uint8_t *p_a2f_ring0_read_idx;
	/* F2A_Index_Ring_0, F2A_Index_Ring_1...... F2A_Index_Ring_n             */
	uint8_t *p_f2a_ring0_write_idx;
	/* F2A_Outdex_Ring_0,  F2A_Outdex_Ring_1........ F2A_Outdex_Ring_n       */
	uint8_t *p_f2a_ring0_read_idx;
	/* A2F_Index_Ring_0, A2F_Index_Ring_1...... A2F_Index_Ring_n             */
	uint8_t *p_a2f_ring0_write_idx;

	/* Base address of A2F Control command Ring */
	void *p_a2f_ring0_base;

	/* Base address of F2A Control event Ring */
	void *p_f2a_ring0_base;

	/* INI parameters */
	void      *p_ini_file;
	uint16_t  max_ini_len;
	uint16_t  reserved3;

	/* Core dump parameters */
	void      *p_core_dump;
	uint16_t  core_dump_len;
	uint16_t  reserved4;

	/* Fermion logging parameters */
	void     *p_ferm_log_buf;
	uint32_t *p_ferm_log_read_idx;
	uint32_t *p_ferm_log_write_idx;
	uint16_t  ferm_log_entry_size;
	uint16_t reserved5;
} fermion_defaults_t;
FERM_STRUCT_4BYTE_ALLIGN_CHECK(fermion_defaults_t)
FERM_STRUCT_SIZE_SYNC(fermion_defaults_t, (76))

#endif /* FERMION_TABLE_API_H */
