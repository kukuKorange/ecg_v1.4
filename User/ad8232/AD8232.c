/**
  ******************************************************************************
  * @file    AD8232.c
  * @brief   AD8232 ECG module driver (STM32F4xx HAL version)
  *
  * @details Pin assignment:
  *          - PA5  : ECG analog signal (ADC1_CH5)
  *          - PB1  : LO- (Leads Off minus)
  *          - PB2  : LO+ (Leads Off plus)
  *
  *          GPIO and ADC are initialized by CubeMX (gpio.c / adc.c).
  *          This driver provides:
  *          - Electrode connection detection
  *          - ECG data acquisition with low-pass filter
  *          - Real-time OLED waveform rendering
  *          - Heart rate calculation (R-peak detection)
  *          - Double-buffered data upload mechanism
  ******************************************************************************
  */

#include "AD8232.h"
#include "adc.h"
#include "../../User/oled/OLED.h"

/*============================ Global Variables ============================*/

MonitorState_t monitor_state = MONITOR_IDLE;
uint16_t ecg_last_filtered = 2048;

uint16_t ecg_data[500] = {0};
uint16_t map_upload[130] = {0};
uint16_t ecg_index = 1;
uint16_t test = 0;

/*============================ Upload Double-Buffer ============================*/

static uint16_t ecg_buffer_a[ECG_UPLOAD_BUFFER_SIZE];
static uint16_t ecg_buffer_b[ECG_UPLOAD_BUFFER_SIZE];

static uint16_t *ecg_fill_buffer = ecg_buffer_a;
static uint16_t *ecg_upload_buffer = ecg_buffer_b;

static uint16_t ecg_fill_idx = 0;
static uint8_t  ecg_buffer_ready = 0;

uint16_t ecg_upload_read_idx = 0;
uint8_t  ecg_upload_active = 0;
uint32_t ecg_upload_timestamp = 0;

/*============================ Private Variables ============================*/

static uint16_t draw_x = 0;
static float last_filtered = 2048;

/*============================ Heart Rate Detection ============================*/
/*
 * 改进算法 (简化 Pan-Tompkins):
 *   1. 低通滤波 (已有, alpha=0.4, fc≈12Hz)
 *   2. 高通滤波去基线漂移 (fc≈1.6Hz)
 *   3. 一阶微分 → 平方 → 强调 QRS 陡峭边沿
 *   4. 滑动窗口积分 (16 点 = 80ms, 覆盖完整 QRS 波群)
 *   5. 自适应阈值: threshold = 55% × 近期峰值
 *   6. 上升沿检测 + 不应期(350ms) + 滞回 + T 波幅度抑制
 *   7. 中值滤波 (窗口 5) 平滑心率输出
 */

#define ECG_SAMPLE_RATE      200
#define ECG_HR_MIN           30
#define ECG_HR_MAX           220
#define ECG_REFRACTORY_MS    350       /* 不应期 350ms, 挡住 R 波后的 T 波 (最高心率 ~171bpm) */
#define ECG_HR_FILTER_SIZE   5         /* 中值滤波窗口 */
#define ECG_INTEG_WINDOW     16        /* 积分窗口: 16 samples = 80ms, 覆盖 QRS */
#define ECG_HP_ALPHA         0.95f     /* 高通截止 ≈ 1.6Hz */
#define ECG_THRESH_RATIO     0.55f     /* 阈值 = 55% × 峰值估计 (抑制 T 波) */
#define ECG_PEAK_DECAY       0.997f    /* 峰值估计每采样衰减 */
#define ECG_TWAVE_REJECT     0.5f      /* 积分值 < 上次 R 峰 50% 视为 T 波丢弃 */

/* 高通滤波器状态 (去除基线漂移) */
static float ecg_hp_prev_in  = 2048.0f;
static float ecg_hp_prev_out = 0.0f;

/* 微分 + 积分 */
static float ecg_prev_bp = 0.0f;      /* 上一次带通滤波值 */
static float ecg_integ_buf[ECG_INTEG_WINDOW] = {0};
static uint8_t ecg_integ_idx = 0;
static float ecg_integ_sum = 0.0f;

/* 自适应阈值 */
static float ecg_peak_level = 0.0f;   /* 积分信号峰值估计 */
static uint8_t ecg_above_thresh = 0;  /* 阈值以上标志 (用于上升沿检测) */
static float ecg_last_rpeak_integ = 0.0f; /* 上次确认 R 峰的积分幅度 */

/* 计时 */
static uint32_t ecg_sample_count = 0;
static uint32_t ecg_last_peak_sample = 0;
static uint8_t ecg_heart_rate = 0;

/* 中值滤波心率缓冲 */
static uint8_t ecg_hr_buffer[ECG_HR_FILTER_SIZE] = {0};
static uint8_t ecg_hr_buffer_idx = 0;
static uint8_t ecg_hr_buffer_count = 0;

static uint8_t hr_median(const uint8_t *arr, uint8_t len)
{
    uint8_t sorted[ECG_HR_FILTER_SIZE];
    uint8_t i, j, temp;

    for (i = 0; i < len; i++)
        sorted[i] = arr[i];

    for (i = 1; i < len; i++)
    {
        temp = sorted[i];
        j = i;
        while (j > 0 && sorted[j - 1] > temp)
        {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = temp;
    }
    return sorted[len / 2];
}

/*============================ Functions ============================*/

/**
  * @brief  AD8232 initialization
  * @note   GPIO (PB1/PB2) and ADC (PA5) are already configured by CubeMX.
  *         This function resets internal state for a clean start.
  */
void AD8232_Init(void)
{
    uint8_t i;

    ecg_index = 1;
    draw_x = 0;
    last_filtered = 2048;
    ecg_fill_idx = 0;
    ecg_buffer_ready = 0;
    ecg_upload_active = 0;

    /* Heart rate detection reset */
    ecg_sample_count = 0;
    ecg_last_peak_sample = 0;
    ecg_heart_rate = 0;
    ecg_hp_prev_in  = 2048.0f;
    ecg_hp_prev_out = 0.0f;
    ecg_prev_bp = 0.0f;
    ecg_integ_idx = 0;
    ecg_integ_sum = 0.0f;
    for (i = 0; i < ECG_INTEG_WINDOW; i++)
        ecg_integ_buf[i] = 0.0f;
    ecg_peak_level = 0.0f;
    ecg_above_thresh = 0;
    ecg_last_rpeak_integ = 0.0f;
    ecg_hr_buffer_idx = 0;
    ecg_hr_buffer_count = 0;
}

/**
  * @brief  Read ECG ADC value (PA5 / ADC1_CH5)
  * @retval 12-bit ADC value (0-4095)
  */
uint16_t AD8232_ReadADC(void)
{
    return ADC_ReadChannel(ADC_CHANNEL_5);
}

/**
  * @brief  Get electrode connection status
  * @retval 1: electrodes connected, 0: leads off
  * @note   Both LO+ (PB2) and LO- (PB1) must be LOW for connected state
  */
uint8_t AD8232_GetConnect(void)
{
    GPIO_PinState lo_plus, lo_minus;

    lo_plus  = HAL_GPIO_ReadPin(AD8232_LO_PLUS_PORT,  AD8232_LO_PLUS_PIN);
    lo_minus = HAL_GPIO_ReadPin(AD8232_LO_MINUS_PORT, AD8232_LO_MINUS_PIN);

    if ((lo_plus == GPIO_PIN_RESET) && (lo_minus == GPIO_PIN_RESET))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
  * @brief  Get heart rate from real-time R-peak detection
  * @retval Heart rate in bpm, 0 if not yet detected
  */
uint8_t ECG_GetHeartRate(void)
{
    return ecg_heart_rate;
}

/**
  * @brief  ECG sample acquisition, filtering, heart rate detection and OLED drawing
  * @note   Should be called at 200 Hz (e.g. from a timer interrupt or periodic task)
  */
void ECG_SampleAndDraw(void)
{
    uint16_t adc_raw;
    float filtered;

    /* 1. Read ADC */
    adc_raw = AD8232_ReadADC();

    /* 2. Low-pass filter: y = y_prev + alpha * (x - y_prev) */
    filtered = last_filtered + ((float)adc_raw - last_filtered) * 0.4f;
    last_filtered = filtered;
    ecg_last_filtered = (uint16_t)filtered;

    /* 3. Store filtered data into fill buffer (double-buffer) */
    ecg_fill_buffer[ecg_fill_idx] = (uint16_t)filtered;
    ecg_fill_idx++;
    ecg_sample_count++;

    /* 4. R-peak detection: HP filter → derivative → square → integrate → adaptive threshold */
    {
        float bp_value, deriv, sq, integ, old_sq;
        uint32_t refractory_samples = (ECG_REFRACTORY_MS * ECG_SAMPLE_RATE) / 1000;

        /* High-pass: remove baseline wander (fc ≈ 1.6Hz @ 200Hz) */
        ecg_hp_prev_out = ECG_HP_ALPHA * (ecg_hp_prev_out + filtered - ecg_hp_prev_in);
        ecg_hp_prev_in = filtered;
        bp_value = ecg_hp_prev_out;

        /* First-order derivative */
        deriv = bp_value - ecg_prev_bp;
        ecg_prev_bp = bp_value;

        /* Square to emphasise QRS steep edges */
        sq = deriv * deriv;

        /* Moving-window integration (16 samples = 80ms) */
        old_sq = ecg_integ_buf[ecg_integ_idx];
        ecg_integ_buf[ecg_integ_idx] = sq;
        ecg_integ_idx = (ecg_integ_idx + 1) % ECG_INTEG_WINDOW;
        ecg_integ_sum = ecg_integ_sum - old_sq + sq;
        integ = ecg_integ_sum;

        /* Update running peak estimate (slow exponential decay) */
        if (integ > ecg_peak_level)
            ecg_peak_level = integ;
        else
            ecg_peak_level *= ECG_PEAK_DECAY;

        /* Rising-edge detection with hysteresis + T-wave rejection */
        if (integ > ecg_peak_level * ECG_THRESH_RATIO && !ecg_above_thresh)
        {
            ecg_above_thresh = 1;

            if (ecg_sample_count > refractory_samples + ecg_last_peak_sample)
            {
                /* T-wave rejection: if amplitude < 50% of last R-peak, discard */
                if (ecg_last_rpeak_integ > 0.0f &&
                    integ < ecg_last_rpeak_integ * ECG_TWAVE_REJECT)
                {
                    /* likely T-wave, skip */
                }
                else
                {
                    ecg_last_rpeak_integ = integ;

                    if (ecg_last_peak_sample > 0)
                    {
                        uint32_t rr_samples = ecg_sample_count - ecg_last_peak_sample;
                        uint16_t hr = (60 * ECG_SAMPLE_RATE) / rr_samples;

                        if (hr >= ECG_HR_MIN && hr <= ECG_HR_MAX)
                        {
                            ecg_hr_buffer[ecg_hr_buffer_idx] = (uint8_t)hr;
                            ecg_hr_buffer_idx = (ecg_hr_buffer_idx + 1) % ECG_HR_FILTER_SIZE;
                            if (ecg_hr_buffer_count < ECG_HR_FILTER_SIZE)
                                ecg_hr_buffer_count++;

                            ecg_heart_rate = hr_median(ecg_hr_buffer, ecg_hr_buffer_count);
                        }
                    }
                    ecg_last_peak_sample = ecg_sample_count;
                }
            }
        }
        else if (integ < ecg_peak_level * ECG_THRESH_RATIO * 0.4f)
        {
            ecg_above_thresh = 0; /* must drop to 40% of threshold to re-arm */
        }
    }

    /* 5. Swap buffers when fill buffer is full (600 points) */
    if (ecg_fill_idx >= ECG_UPLOAD_BUFFER_SIZE)
    {
        uint16_t *temp;

        ecg_fill_idx = 0;

        if (!ecg_upload_active)
        {
            temp = ecg_fill_buffer;
            ecg_fill_buffer = ecg_upload_buffer;
            ecg_upload_buffer = temp;
            ecg_buffer_ready = 1;
        }
    }

    /* 6. Draw waveform on OLED */
    if (ecg_index < 120)
    {
        int16_t y_pos;

        y_pos = 90 - (int16_t)(filtered / 45);

        if (y_pos < 11) y_pos = 11;
        if (y_pos > 53) y_pos = 53;

        ecg_data[ecg_index] = (uint16_t)y_pos;
        ecg_data[0] = ecg_data[1];

        OLED_DrawLine(draw_x + 3, ecg_data[ecg_index - 1],
                      draw_x + 4, ecg_data[ecg_index]);

        ecg_index++;
        draw_x += 1;
    }
    else
    {
        ECG_ClearAndRedraw();
        ecg_index = 1;
        draw_x = 0;
    }
}

/**
  * @brief  Clear ECG display area and redraw axes
  */
void ECG_ClearAndRedraw(void)
{
    OLED_ClearArea(3, 10, 125, 45);

    OLED_DrawLine(1, 54, 120, 54);     /* X axis */
    OLED_DrawLine(1, 10, 1, 54);       /* Y axis */

    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);   /* Y arrow */
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED); /* X arrow */
}

/**
  * @brief  Calculate heart rate from peak detection on a data array
  * @param  array: ECG data array
  * @param  length: array length
  * @retval Peak interval (used for heart rate estimation)
  */
uint8_t GetHeartRate(uint16_t *array, uint16_t length)
{
    uint8_t i, count = 0;
    uint16_t peakPositions[10] = {0};

    for (i = 1; i < length - 1; i++)
    {
        if ((array[i] > array[i - 1]) &&
            (array[i] > array[i + 1]) &&
            (array[i] < 20))
        {
            peakPositions[count] = i;
            count++;

            if (count >= 2)
            {
                return (peakPositions[1] - peakPositions[0]);
            }
        }
    }

    return 0;
}

/**
  * @brief  Draw static ECG chart
  * @param  Chart: data array
  * @param  Width: line width
  */
void DrawChart(uint16_t Chart[], uint8_t Width)
{
    int i, j = 0;

    for (i = 0; i < 118; i++)
    {
        OLED_DrawLine(j + 3, Chart[i], j + 4 + Width, Chart[i + 1]);
        j += 2;
    }
}

/**
  * @brief  Chart data optimization (2-point average filter)
  * @param  R: raw data array
  * @param  Chart: output data array
  */
void ChartOptimize(uint16_t *R, uint16_t *Chart)
{
    int i, j = 0;

    for (i = 0; i < 500; i++)
    {
        Chart[i] = (R[j] + R[j + 1]) / 2;

        if (R[j + 1] == 0)
        {
            break;
        }
        j += 2;
    }
}

/*============================================================================*/
/*                          ECG Upload Functions                              */
/*============================================================================*/

uint8_t ECG_StartUpload(uint32_t timestamp)
{
    if (!ecg_buffer_ready)
    {
        return 0;
    }

    ecg_upload_timestamp = timestamp;
    ecg_upload_read_idx = 0;
    ecg_upload_active = 1;
    ecg_buffer_ready = 0;

    return 1;
}

void ECG_StopUpload(void)
{
    ecg_upload_active = 0;
    ecg_upload_read_idx = 0;
}

uint16_t ECG_GetUploadDataCount(void)
{
    if (ecg_upload_active || ecg_buffer_ready)
    {
        return ECG_UPLOAD_BUFFER_SIZE;
    }
    return 0;
}

uint8_t ECG_IsDataReady(void)
{
    return ecg_buffer_ready;
}

uint16_t ECG_GetUploadData(uint16_t index)
{
    if (index < ECG_UPLOAD_BUFFER_SIZE)
    {
        return ecg_upload_buffer[index];
    }
    return 0;
}

uint16_t* ECG_GetUploadBuffer(void)
{
    return ecg_upload_buffer;
}

uint16_t ECG_GetUploadBatch(uint16_t *batch_data, uint16_t batch_size)
{
    uint16_t i;
    uint16_t count = 0;
    uint16_t available;

    if (!ecg_upload_active)
    {
        return 0;
    }

    available = ECG_UPLOAD_BUFFER_SIZE - ecg_upload_read_idx;
    if (available == 0)
    {
        ecg_upload_active = 0;
        return 0;
    }

    if (batch_size > available)
    {
        batch_size = available;
    }
    if (batch_size > ECG_UPLOAD_BATCH_SIZE)
    {
        batch_size = ECG_UPLOAD_BATCH_SIZE;
    }

    for (i = 0; i < batch_size; i++)
    {
        batch_data[i] = ecg_upload_buffer[ecg_upload_read_idx + i];
        count++;
    }

    ecg_upload_read_idx += count;

    return count;
}

uint8_t ECG_GetUploadProgress(void)
{
    if (!ecg_upload_active)
    {
        return 100;
    }
    return (uint8_t)((ecg_upload_read_idx * 100) / ECG_UPLOAD_BUFFER_SIZE);
}

uint8_t ECG_IsUploadComplete(void)
{
    return !ecg_upload_active;
}
