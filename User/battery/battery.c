/**
  ******************************************************************************
  * @file    battery.c
  * @brief   Battery voltage monitoring via PA4 (ADC1_CH4)
  ******************************************************************************
  */

#include "battery.h"
#include "adc.h"

/*============================ Global Variables ============================*/

uint8_t  battery_percent  = 100;   /* 电池电量百分比 0-100%，线性映射 EMPTY~FULL */
uint16_t battery_mv       = 4200;  /* 电池实际电压（毫伏），经分压比还原后的真实值 */
uint8_t  battery_low_flag = 0;     /* 低电量标志：1=电压低于 BATTERY_LOW_MV，触发告警 */

/*============================ API ============================*/

void Battery_Init(void)
{
    Battery_Update();
}

/**
  * @brief  读取PA4 ADC并换算电池电压与电量
  *
  *         采样链路:  Vbat ──┤30K├─── PA4 ──┤10K├─── GND
  *
  *         Vadc = Vbat × 10/(30+10) = Vbat / 4
  *         Vbat = Vadc × 4
  *
  *         ADC 12-bit (0-4095) → Vadc(mV) → Vbat(mV) → 百分比
  */
void Battery_Update(void)
{
    uint16_t adc_raw;       /* PA4 原始 ADC 值 (0-4095) */
    uint32_t vadc_mv;       /* 分压后电压 (mV)，即 PA4 引脚实际电压 */

    adc_raw  = ADC_ReadChannel(ADC_CHANNEL_4);
    vadc_mv  = (uint32_t)adc_raw * BATTERY_ADC_VREF_MV / 4095;
    battery_mv = (uint16_t)(vadc_mv * BATTERY_DIVIDER_RATIO);  /* 还原真实电池电压 */

    if (battery_mv >= BATTERY_FULL_MV)
        battery_percent = 100;
    else if (battery_mv <= BATTERY_EMPTY_MV)
        battery_percent = 0;
    else
        battery_percent = (uint8_t)((uint32_t)(battery_mv - BATTERY_EMPTY_MV) * 100
                          / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));

    battery_low_flag = (battery_mv < BATTERY_LOW_MV) ? 1 : 0;
}
