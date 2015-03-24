#ifndef _SBCHK_BASE_H
#define _SBCHK_BASE_H

/**************************************************************************
 *  HASH CONFIGURATION
 **************************************************************************/
#define HASH_OUTPUT_LEN                     (20)

/**************************************************************************
 *  SBCHK ENGINE PATH
 **************************************************************************/
#define SBCHK_ENGINE_PATH                   "/system/bin/sbchk"

/**************************************************************************
 *  SBCHK ENGINE CHECK
 **************************************************************************/
#define SBCHK_ENGINE_HASH_CHECK             (0)

/**************************************************************************
 *  SBCHK ENGINE HASH VALUE
 **************************************************************************/
#if SBCHK_ENGINE_HASH_CHECK

#error must fill the hash value of '/system/bin/sbchk'

/*
   Kernel will compare these two hash values to check if
   the current '/system/bin/sbchk' is the one you expect.

   To ensure the hash value you fill is right.
   The steps to obtain hash value are listed below;

   (1) Turn off SBCHK_ENGINE_HASH_CHECK

   (2) Download images and record kernel log

   (3) Find the string pattern '[SBCHK_BASE]' in kernel log

   (4) The hash value of current '/system/bin/sbchk' is calculated

       [SBCHK_BASE] Calculate the hash value of '/system/bin/sbchk' =
       xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

   (5) Fill hash value in SBCHK_ENGINE_HASH

   However, the object of '/system/bin/sbchk' may be changed
   if compile tool chain is updated or customer modify this user space program

   Before delivering boot image,
   please double check if the hash value should be updated or not.
*/

/* #define SBCHK_ENGINE_HASH "3a816d2e275818cb12b839a10e838a1e10d729f7" */
#define SBCHK_ENGINE_HASH ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?  ?

#endif

/**************************************************************************
 *  ERROR CODE
 **************************************************************************/
#define SEC_OK                              (0x0000)
#define SBCHK_BASE_ENGINE_OPEN_FAIL         (0x1000)
#define SBCHK_BASE_ENGINE_READ_FAIL         (0x1001)
#define SBCHK_BASE_HASH_INIT_FAIL           (0x1002)
#define SBCHK_BASE_HASH_DATA_FAIL           (0x1003)
#define SBCHK_BASE_HASH_CHECK_FAIL          (0x1004)
#define SBCHK_BASE_INDEX_OUT_OF_RANGE       (0xFFFFFFFF)


/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern void sbchk_base(void);


/******************************************************************************
 * GLOBAL DATA
 ******************************************************************************/
#define DEVINFO_DATA_SIZE    44
u32 g_devinfo_data[DEVINFO_DATA_SIZE];
u32 g_devinfo_data_size = DEVINFO_DATA_SIZE;
u32 get_devinfo_with_index(u32 index);

EXPORT_SYMBOL(g_devinfo_data);
EXPORT_SYMBOL(g_devinfo_data_size);
EXPORT_SYMBOL(get_devinfo_with_index);

#endif	 /*_SBCHK_BASE*/
