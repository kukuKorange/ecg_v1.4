/**
  ******************************************************************************
  * @file    recorder.h
  * @brief   ECG recording — sector/block-based, backend-agnostic
  *
  * @details Block layout:
  *            Block 0       : Master index
  *            Blocks 1-4    : Record index (16 entries per block, max 64)
  *            Blocks 100+   : ECG data (256 uint16 samples per block)
  ******************************************************************************
  */

#ifndef __RECORDER_H
#define __RECORDER_H

#include <stdint.h>

/*============================ Constants ============================*/

#define RECORD_MAX              64
#define RECORD_INDEX_START      1
#define RECORD_DATA_START       256     /* block 256 = sector 6 start on Flash */
#define RECORD_SAMPLES_PER_BLK  256     /* 512B / 2 */

#define MODE_CONTINUOUS         0
#define MODE_TIMED              1

/*============================ Data Types ============================*/

/**
 * ECG 录制记录元数据 (固定 32 字节, 按 1 字节对齐)
 *
 * 每条记录描述一次完整的 ECG 录制会话,
 * 存储在索引区 (Block 1-4), 每块可容纳 16 条 (512 / 32 = 16).
 * 实际 ECG 采样数据存储在数据区 (Block 256+).
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;             /* 魔数: 固定为 0x45434731 (ASCII "ECG1"),
                                   用于校验该记录是否有效/已初始化 */
    uint16_t record_id;         /* 记录编号: 全局自增 ID, 每新建一条录制 +1,
                                   用于唯一标识本次录制 */
    uint8_t  mode;              /* 监测模式: MODE_CONTINUOUS(0)=连续监测,
                                   MODE_TIMED(1)=定时分段监测 */
    uint8_t  user_age;          /* 用户年龄: 录制时关联的用户年龄 (岁) */
    uint32_t start_block;       /* 数据起始块号: 本条记录的 ECG 采样数据
                                   在存储介质中的起始 block 编号 (≥256) */
    uint32_t num_samples;       /* 采样点总数: 本次录制共采集的 uint16 样本数,
                                   占用 ceil(num_samples/256) 个存储块 */
    uint16_t duration_sec;      /* 录制时长: 从开始到结束的秒数 */
    uint8_t  avg_hr;            /* 平均心率: 录制期间所有有效心率的均值 (bpm) */
    uint8_t  max_hr;            /* 最大心率: 录制期间出现的最高心率 (bpm) */
    char     user_name[8];      /* 用户姓名: 录制时关联的用户名,
                                   最多 7 个 ASCII 字符 + '\0' */
    uint8_t  padding[4];        /* 填充字节: 保证结构体总长恰好 32 字节,
                                   便于在 512 字节块中整除排列 */
} ECGRecord_t;                  /* 总大小 32 bytes */
#pragma pack(pop)

/*============================ External Variables ============================*/

extern uint8_t  recorder_recording;     /* 1 while recording */
extern uint16_t recorder_elapsed_sec;

/*============================ API ============================*/

void     Recorder_Init(void);
uint8_t  Recorder_Start(uint8_t mode, uint16_t timed_duration_sec);
void     Recorder_FeedSample(uint16_t sample, uint8_t hr);
void     Recorder_Stop(void);
void     Recorder_Tick1Hz(void);

uint8_t  Recorder_GetCount(void);
uint8_t  Recorder_LoadRecord(uint8_t index, ECGRecord_t *rec);
uint16_t Recorder_LoadSamples(uint8_t rec_index, uint32_t offset,
                              uint16_t *buf, uint16_t count);
void     Recorder_SaveIndex(void);
uint8_t  Recorder_DeleteAll(void);

#endif /* __RECORDER_H */
