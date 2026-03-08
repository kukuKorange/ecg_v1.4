/**
  ******************************************************************************
  * @file    tim.h
  * @brief   TIM2 periodic timer - multi-task scheduler
  *
  * @details Ported from v1.3 Timer2.c (TIM3 @ 100kHz with software dividers)
  *          v1.4 uses TIM2 @ 200Hz as the base tick, with software dividers:
  *
  *          Task              Freq    Divider   Purpose
  *          ─────────────────────────────────────────────
  *          ecg_sample_flag   200Hz   /1        ECG ADC + filter + draw
  *          ecg_upload_flag   10Hz    /20       ECG data upload batch
  *          display_refresh   5Hz     /40       Info page refresh
  *          oled_update_flag  25Hz    /8        OLED framebuffer flush
  *          seconds_counter   1Hz     /200      Runtime counter (test)
  *
  *          Clock: APB1 timer clock = 100 MHz
  *          TIM2: PSC=99, ARR=4999 → 200 Hz
  ******************************************************************************
  */

#ifndef __TIM_H
#define __TIM_H

#include "main.h"

/*============================ Timer Flags ============================*/

extern volatile uint8_t  ecg_sample_flag;       /**< 200Hz ECG sampling */
extern volatile uint8_t  oled_update_flag;      /**< 25Hz OLED flush */
extern volatile uint8_t  display_refresh_flag;  /**< 5Hz info page refresh */
extern volatile uint8_t  ecg_upload_flag;       /**< 10Hz ECG upload batch */
extern volatile uint8_t  battery_check_flag;    /**< 1Hz battery reading */
extern volatile uint8_t  second_tick_flag;      /**< 1Hz tick for recorder */
extern volatile uint16_t seconds_counter;       /**< Runtime seconds */

/*============================ Functions ============================*/

void MX_TIM2_Init(void);
void TIM2_Start(void);
void TIM2_Stop(void);

#endif /* __TIM_H */
