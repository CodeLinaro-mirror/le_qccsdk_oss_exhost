/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
  */

#ifndef FERMION_CMN_API_H
#define FERMION_CMN_API_H

/*------------------------------------------------------------------------
* Include Files
* ----------------------------------------------------------------------*/
/* None */

/*------------------------------------------------------------------------
* Preprocessor Definitions and Constants
* ----------------------------------------------------------------------*/
#if defined(__GNUC__)
#define PACKSTRUCT __attribute__ ((packed))
#else
#define PACKSTRUCT
#endif

#define Reserve_24_BIT 3

#ifndef FERM_COMPILE_ASSERT
#define FERM_COMPILE_ASSERT(predicate)    \
      extern uint8_t ferm_dummy_array[(predicate) ? 1 : -1];
#endif /* FERM_COMPILE_ASSERT */

#ifndef FERM_STRUCT_4BYTE_ALLIGN_CHECK
#define FERM_STRUCT_4BYTE_ALLIGN_CHECK(struct) FERM_COMPILE_ASSERT(!(sizeof(struct) % 4))
#endif /* FERM_STRUCT_4BYTE_ALLIGN_CHECK */

#ifndef FERM_STRUCT_SIZE_SYNC
#define FERM_STRUCT_SIZE_SYNC(struct, val)  FERM_COMPILE_ASSERT(((sizeof(struct)) == val))
#endif /* FERM_STRUCT_SIZE_SYNC */


/*------------------------------------------------------------------------
* Type Declarations
* ----------------------------------------------------------------------*/
/* None */

#endif /* FERMION_CMN_API_H */
