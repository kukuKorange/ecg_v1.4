/**
  ******************************************************************************
  * @file    storage.c
  * @brief   Abstract storage layer — dual backend (Flash / SD)
  ******************************************************************************
  */

#include "storage.h"

#ifdef STORAGE_USE_FLASH
  #include "stm32f4xx_hal_flash.h"
  #include "stm32f4xx_hal_flash_ex.h"
#endif

#ifdef STORAGE_USE_SD
  #include "../sd/SD_Card.h"
#endif

/*============================================================================*/
/*                        Flash Backend                                       */
/*============================================================================*/
#ifdef STORAGE_USE_FLASH

static uint32_t block_to_addr(uint32_t block_id)
{
    return FLASH_DATA_BASE_ADDR + (block_id * STORAGE_BLOCK_SIZE);
}

uint8_t Storage_Init(void)
{
    return 0;
}

uint8_t Storage_ReadBlock(uint32_t block_id, uint8_t *buf)
{
    uint32_t addr;
    uint32_t i;

    if (block_id >= STORAGE_TOTAL_BLOCKS || buf == NULL)
        return 1;

    addr = block_to_addr(block_id);

    for (i = 0; i < STORAGE_BLOCK_SIZE; i++)
    {
        buf[i] = *(__IO uint8_t *)(addr + i);
    }
    return 0;
}

uint8_t Storage_WriteBlock(uint32_t block_id, const uint8_t *buf)
{
    uint32_t addr;
    uint32_t i;
    HAL_StatusTypeDef status;

    if (block_id >= STORAGE_TOTAL_BLOCKS || buf == NULL)
        return 1;

    addr = block_to_addr(block_id);

    HAL_FLASH_Unlock();

    for (i = 0; i < STORAGE_BLOCK_SIZE; i += 2)
    {
        uint16_t half = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, half);
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 2;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

uint8_t Storage_EraseAll(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;
    HAL_StatusTypeDef status;

    HAL_FLASH_Unlock();

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = FLASH_DATA_SECTOR_5;
    erase.NbSectors    = 3;   /* Sectors 5, 6, 7 */

    status = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0 : 1;
}

uint8_t Storage_EraseIndex(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;
    HAL_StatusTypeDef status;

    HAL_FLASH_Unlock();

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = FLASH_DATA_SECTOR_5;
    erase.NbSectors    = 1;   /* Sector 5 only (index area) */

    status = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0 : 1;
}

uint32_t Storage_GetCapacityBlocks(void)
{
    return STORAGE_TOTAL_BLOCKS;
}

#endif /* STORAGE_USE_FLASH */

/*============================================================================*/
/*                        SD Card Backend                                     */
/*============================================================================*/
#ifdef STORAGE_USE_SD

uint8_t Storage_Init(void)
{
    return 0;
}

uint8_t Storage_ReadBlock(uint32_t block_id, uint8_t *buf)
{
    if (buf == NULL)
        return 1;
    return SD_ReadBlock(STORAGE_SD_BASE_SECTOR + block_id, buf);
}

uint8_t Storage_WriteBlock(uint32_t block_id, const uint8_t *buf)
{
    if (buf == NULL)
        return 1;
    return SD_WriteBlock(STORAGE_SD_BASE_SECTOR + block_id, buf);
}

uint8_t Storage_EraseAll(void)
{
    uint8_t blank[STORAGE_BLOCK_SIZE];
    uint32_t i;

    for (i = 0; i < STORAGE_BLOCK_SIZE; i++)
        blank[i] = 0xFF;

    /* Only erase master index + record index blocks (0-4) */
    for (i = 0; i < 5; i++)
    {
        if (SD_WriteBlock(STORAGE_SD_BASE_SECTOR + i, blank) != 0)
            return 1;
    }
    return 0;
}

uint8_t Storage_EraseIndex(void)
{
    return Storage_EraseAll();   /* SD: same as EraseAll (index blocks only) */
}

uint32_t Storage_GetCapacityBlocks(void)
{
    return STORAGE_TOTAL_BLOCKS;
}

#endif /* STORAGE_USE_SD */
