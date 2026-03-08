/**
  ******************************************************************************
  * @file    AD8232.h
  * @brief   AD8232 ECG module driver (STM32F4xx HAL version)
  *
  * @details Pin assignment:
  *          - PA5  : ECG analog signal (ADC1_CH5)
  *          - PB1  : LO- (Leads Off minus)
  *          - PB2  : LO+ (Leads Off plus)
  ******************************************************************************
  */

#ifndef __AD8232_H
#define __AD8232_H

#include "main.h"

/*============================ Pin Definitions ============================*/

#define AD8232_LO_MINUS_PORT    GPIOB
#define AD8232_LO_MINUS_PIN     GPIO_PIN_1

#define AD8232_LO_PLUS_PORT     GPIOB
#define AD8232_LO_PLUS_PIN      GPIO_PIN_2

/*============================ Constants ============================*/

#define ECG_UPLOAD_BUFFER_SIZE  600
#define ECG_UPLOAD_BATCH_SIZE   1

/*============================ Monitoring State ============================*/

typedef enum {
    MONITOR_IDLE,
    MONITOR_RUNNING,
    MONITOR_DONE
} MonitorState_t;

extern MonitorState_t monitor_state;
extern uint16_t       ecg_last_filtered;    /**< latest filtered sample for recorder */

/*============================ Extern Variables ============================*/

extern uint16_t ecg_data[500];
extern uint16_t ecg_index;
extern uint16_t ecg_upload_read_idx;
extern uint8_t  ecg_upload_active;
extern uint32_t ecg_upload_timestamp;

/*============================ Function Prototypes ============================*/

void     AD8232_Init(void);
uint8_t  AD8232_GetConnect(void);

uint16_t AD8232_ReadADC(void);

void     ECG_SampleAndDraw(void);
void     ECG_ClearAndRedraw(void);
uint8_t  ECG_GetHeartRate(void);

uint8_t  GetHeartRate(uint16_t *array, uint16_t length);
void     DrawChart(uint16_t Chart[], uint8_t Width);
void     ChartOptimize(uint16_t *R, uint16_t *Chart);

uint8_t  ECG_StartUpload(uint32_t timestamp);
void     ECG_StopUpload(void);
uint16_t ECG_GetUploadDataCount(void);
uint8_t  ECG_IsDataReady(void);
uint16_t ECG_GetUploadData(uint16_t index);
uint16_t* ECG_GetUploadBuffer(void);
uint16_t ECG_GetUploadBatch(uint16_t *batch_data, uint16_t batch_size);
uint8_t  ECG_GetUploadProgress(void);
uint8_t  ECG_IsUploadComplete(void);

#endif /* __AD8232_H */
