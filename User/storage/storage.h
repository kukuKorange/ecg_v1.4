/**
  ******************************************************************************
  * @file    storage.h
  * @brief   Abstract storage layer — dual backend (Flash / SD)
  *
  * @details Select backend at compile time:
  *            #define STORAGE_USE_FLASH  1   (Internal Flash, Sectors 5-7, 384KB)
  *            #define STORAGE_USE_SD     1   (SD card via SPI)
  *
  *          All higher-level modules (recorder, userinfo) use only this API.
  *          One block = 512 bytes.
  ******************************************************************************
  */

#ifndef __STORAGE_H
#define __STORAGE_H

#include "main.h"

/*============================ Backend Selection ============================*/

#define STORAGE_USE_FLASH       1
/* #define STORAGE_USE_SD       1 */

#if defined(STORAGE_USE_FLASH) && defined(STORAGE_USE_SD)
  #error "Choose only one storage backend"
#endif
#if !defined(STORAGE_USE_FLASH) && !defined(STORAGE_USE_SD)
  #error "No storage backend selected"
#endif

/*============================ Constants ============================*/

#define STORAGE_BLOCK_SIZE          512

#ifdef STORAGE_USE_FLASH
  #define FLASH_DATA_BASE_ADDR      0x08020000U   /* Sector 5 start */
  #define FLASH_DATA_END_ADDR       0x08080000U   /* End of Sector 7 */
  #define FLASH_DATA_TOTAL_BYTES    (FLASH_DATA_END_ADDR - FLASH_DATA_BASE_ADDR)
  #define STORAGE_TOTAL_BLOCKS      (FLASH_DATA_TOTAL_BYTES / STORAGE_BLOCK_SIZE)

  #define FLASH_DATA_SECTOR_5       5
  #define FLASH_DATA_SECTOR_6       6
  #define FLASH_DATA_SECTOR_7       7
#endif

#ifdef STORAGE_USE_SD
  #define STORAGE_SD_BASE_SECTOR    0
  #define STORAGE_TOTAL_BLOCKS      4096   /* 2MB logical limit (expandable) */
#endif

/*============================ API ============================*/

uint8_t  Storage_Init(void);
uint8_t  Storage_ReadBlock(uint32_t block_id, uint8_t *buf);
uint8_t  Storage_WriteBlock(uint32_t block_id, const uint8_t *buf);
uint8_t  Storage_EraseAll(void);
uint8_t  Storage_EraseIndex(void);      /**< erase index area only (sector 5) */
uint32_t Storage_GetCapacityBlocks(void);

#endif /* __STORAGE_H */
