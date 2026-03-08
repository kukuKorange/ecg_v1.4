/**
  ******************************************************************************
  * @file    recorder.c
  * @brief   ECG recording — block-based, backend-agnostic
  *
  * @details Index area (blocks 0-4) is kept in RAM cache to avoid
  *          Flash read-modify-write issues. On Flash backend, sector 5
  *          is erased and rewritten atomically via Recorder_SaveIndex().
  ******************************************************************************
  */

#include "recorder.h"
#include "../storage/storage.h"
#include "../userinfo/userinfo.h"
#include <string.h>

/*============================ Constants ============================*/

#define MASTER_MAGIC    0x45434730U
#define RECORD_MAGIC    0x45434731U

/*============================ Global Variables ============================*/

uint8_t  recorder_recording   = 0;
uint16_t recorder_elapsed_sec = 0;

/*============================ Private Variables ============================*/

static ECGRecord_t records[RECORD_MAX];
static uint16_t record_count    = 0;
static uint32_t next_data_block = RECORD_DATA_START;

static uint8_t  cur_mode       = MODE_CONTINUOUS;
static uint16_t cur_timed_dur  = 0;

static uint16_t wr_buf[RECORD_SAMPLES_PER_BLK];
static uint16_t wr_buf_idx     = 0;
static uint32_t wr_block_ptr   = 0;
static uint32_t wr_total_samples = 0;

static uint32_t hr_sum   = 0;
static uint16_t hr_count = 0;
static uint8_t  hr_max   = 0;

/*============================ Helpers ============================*/

static void buf_write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void buf_write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint32_t buf_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void flush_wr_buf(void)
{
    if (wr_buf_idx == 0)
        return;

    while (wr_buf_idx < RECORD_SAMPLES_PER_BLK)
        wr_buf[wr_buf_idx++] = 0xFFFF;

    Storage_WriteBlock(wr_block_ptr, (const uint8_t *)wr_buf);
    wr_block_ptr++;
    wr_buf_idx = 0;
}

/*============================ API ============================*/

void Recorder_Init(void)
{
    uint8_t buf[STORAGE_BLOCK_SIZE];
    uint32_t magic;
    uint16_t i;

    record_count    = 0;
    next_data_block = RECORD_DATA_START;
    memset(records, 0, sizeof(records));

    if (Storage_ReadBlock(0, buf) != 0)
        return;

    magic = buf_read_u32(buf);
    if (magic != MASTER_MAGIC)
        return;

    record_count    = (uint16_t)(buf[4] | ((uint16_t)buf[5] << 8));
    next_data_block = buf_read_u32(&buf[6]);

    if (record_count > RECORD_MAX)
        record_count = RECORD_MAX;
    if (next_data_block < RECORD_DATA_START)
        next_data_block = RECORD_DATA_START;

    /* Load all record entries into RAM cache */
    for (i = 0; i < record_count; i++)
    {
        uint32_t blk = RECORD_INDEX_START + (i / 16);
        uint32_t pos = (i % 16) * sizeof(ECGRecord_t);

        if ((i % 16) == 0)
        {
            if (Storage_ReadBlock(blk, buf) != 0)
                break;
        }

        memcpy(&records[i], &buf[pos], sizeof(ECGRecord_t));
    }
}

uint8_t Recorder_Start(uint8_t mode, uint16_t timed_duration_sec)
{
    if (recorder_recording)
        return 1;
    if (record_count >= RECORD_MAX)
        return 2;
    if (next_data_block >= Storage_GetCapacityBlocks())
        return 3;

    cur_mode      = mode;
    cur_timed_dur = timed_duration_sec;

    wr_buf_idx       = 0;
    wr_block_ptr     = next_data_block;
    wr_total_samples = 0;

    hr_sum   = 0;
    hr_count = 0;
    hr_max   = 0;

    recorder_elapsed_sec = 0;
    recorder_recording   = 1;

    return 0;
}

void Recorder_FeedSample(uint16_t sample, uint8_t hr)
{
    if (!recorder_recording)
        return;

    wr_buf[wr_buf_idx++] = sample;
    wr_total_samples++;

    if (wr_buf_idx >= RECORD_SAMPLES_PER_BLK)
    {
        Storage_WriteBlock(wr_block_ptr, (const uint8_t *)wr_buf);
        wr_block_ptr++;
        wr_buf_idx = 0;

        if (wr_block_ptr >= Storage_GetCapacityBlocks())
        {
            Recorder_Stop();
            return;
        }
    }

    if (hr > 0)
    {
        hr_sum += hr;
        hr_count++;
        if (hr > hr_max)
            hr_max = hr;
    }
}

void Recorder_Stop(void)
{
    ECGRecord_t *rec;

    if (!recorder_recording)
        return;

    recorder_recording = 0;
    flush_wr_buf();

    /* Build record entry in RAM cache */
    rec = &records[record_count];
    memset(rec, 0xFF, sizeof(ECGRecord_t));
    rec->magic        = RECORD_MAGIC;
    rec->record_id    = record_count;
    rec->mode         = cur_mode;
    rec->user_age     = user_info.age;
    rec->start_block  = next_data_block;
    rec->num_samples  = wr_total_samples;
    rec->duration_sec = recorder_elapsed_sec;
    rec->avg_hr       = (hr_count > 0) ? (uint8_t)(hr_sum / hr_count) : 0;
    rec->max_hr       = hr_max;
    memcpy(rec->user_name, user_info.name, 8);

    record_count++;
    next_data_block = wr_block_ptr;

    Recorder_SaveIndex();
}

/**
  * @brief  Write master index + all record entries to storage.
  *         On Flash: erases sector 5 first, then rewrites from RAM cache.
  *         On SD: overwrites blocks directly.
  */
void Recorder_SaveIndex(void)
{
    uint8_t buf[STORAGE_BLOCK_SIZE];
    uint16_t i;
    uint8_t num_idx_blocks;

#ifdef STORAGE_USE_FLASH
    Storage_EraseIndex();
#endif

    /* Write master block (block 0) */
    memset(buf, 0xFF, STORAGE_BLOCK_SIZE);
    buf_write_u32(&buf[0], MASTER_MAGIC);
    buf_write_u16(&buf[4], (uint16_t)record_count);
    buf_write_u32(&buf[6], next_data_block);
    memcpy(&buf[10], user_info.name, 8);
    buf[18] = user_info.age;
    Storage_WriteBlock(0, buf);

    /* Write record index blocks from RAM cache */
    num_idx_blocks = (record_count > 0)
                     ? (uint8_t)((record_count - 1) / 16) + 1
                     : 0;
    if (num_idx_blocks > 4) num_idx_blocks = 4;

    for (i = 0; i < num_idx_blocks; i++)
    {
        uint16_t base = i * 16;
        uint16_t cnt  = record_count - base;
        if (cnt > 16) cnt = 16;

        memset(buf, 0xFF, STORAGE_BLOCK_SIZE);
        memcpy(buf, &records[base], cnt * sizeof(ECGRecord_t));
        Storage_WriteBlock(RECORD_INDEX_START + i, buf);
    }
}

void Recorder_Tick1Hz(void)
{
    if (!recorder_recording)
        return;

    recorder_elapsed_sec++;

    if (cur_mode == MODE_TIMED && cur_timed_dur > 0)
    {
        if (recorder_elapsed_sec >= cur_timed_dur)
            Recorder_Stop();
    }
}

uint8_t Recorder_GetCount(void)
{
    return (uint8_t)record_count;
}

uint8_t Recorder_LoadRecord(uint8_t index, ECGRecord_t *rec)
{
    if (index >= record_count || rec == NULL)
        return 1;

    *rec = records[index];

    if (rec->magic != RECORD_MAGIC)
        return 3;

    return 0;
}

uint16_t Recorder_LoadSamples(uint8_t rec_index, uint32_t offset,
                              uint16_t *buf, uint16_t count)
{
    ECGRecord_t *rec;
    uint8_t blk_buf[STORAGE_BLOCK_SIZE];
    uint32_t blk_id, blk_off;
    uint16_t loaded = 0;

    if (rec_index >= record_count)
        return 0;
    rec = &records[rec_index];
    if (offset >= rec->num_samples)
        return 0;

    while (loaded < count && (offset + loaded) < rec->num_samples)
    {
        uint32_t abs_sample = offset + loaded;
        blk_id  = rec->start_block + (abs_sample / RECORD_SAMPLES_PER_BLK);
        blk_off = abs_sample % RECORD_SAMPLES_PER_BLK;

        if (Storage_ReadBlock(blk_id, blk_buf) != 0)
            break;

        {
            uint16_t *samples = (uint16_t *)blk_buf;
            while (blk_off < RECORD_SAMPLES_PER_BLK &&
                   loaded < count &&
                   (offset + loaded) < rec->num_samples)
            {
                buf[loaded++] = samples[blk_off++];
            }
        }
    }

    return loaded;
}

uint8_t Recorder_DeleteAll(void)
{
    uint8_t ret = Storage_EraseAll();
    if (ret == 0)
    {
        record_count    = 0;
        next_data_block = RECORD_DATA_START;
        memset(records, 0, sizeof(records));

        Recorder_SaveIndex();
    }
    return ret;
}
