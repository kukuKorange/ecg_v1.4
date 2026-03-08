/**
  ******************************************************************************
  * @file    userinfo.c
  * @brief   User profile (name + age) management
  *
  * @details Stored in block 0 (master index) of the storage layer.
  *          Layout of master index block (512 bytes):
  *            [0-3]   magic  0x45434730 ("ECG0")
  *            [4-5]   record_count
  *            [6-9]   next_data_block
  *            [10-17] user_name (8 chars)
  *            [18]    user_age
  ******************************************************************************
  */

#include "userinfo.h"
#include "../storage/storage.h"
#include "../recorder/recorder.h"
#include <string.h>

/*============================ Constants ============================*/

#define MASTER_MAGIC        0x45434730U   /* "ECG0" */
#define MASTER_NAME_OFFSET  10
#define MASTER_AGE_OFFSET   18

/*============================ Global Variables ============================*/

UserInfo_t user_info = { "USER", 25 };

/*============================ Helpers ============================*/

static uint32_t buf_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*============================ API ============================*/

void UserInfo_Init(void)
{
    UserInfo_Load();
}

void UserInfo_Load(void)
{
    uint8_t buf[STORAGE_BLOCK_SIZE];
    uint32_t magic;

    if (Storage_ReadBlock(0, buf) != 0)
        return;

    magic = buf_read_u32(buf);
    if (magic != MASTER_MAGIC)
        return;

    memcpy(user_info.name, &buf[MASTER_NAME_OFFSET], USER_NAME_MAX);
    user_info.name[USER_NAME_MAX] = '\0';
    user_info.age = buf[MASTER_AGE_OFFSET];
}

void UserInfo_Save(void)
{
    Recorder_SaveIndex();
}
