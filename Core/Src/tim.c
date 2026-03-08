/**
  ******************************************************************************
  * @file    tim.c
  * @brief   TIM2 periodic timer (200 Hz) - multi-task scheduler
  *
  * @details Ported from v1.3 Timer2.c business logic.
  *          v1.3 used TIM3 @ 100kHz with large dividers (500, 2000, 20000, etc.)
  *          v1.4 uses TIM2 @ 200Hz directly, with smaller dividers.
  *
  *          APB1 timer clock = 100 MHz (APB1 prescaler ≠ 1 → ×2)
  *          PSC  = 99   → counter clock = 1 MHz
  *          ARR  = 4999 → update event  = 200 Hz
  ******************************************************************************
  */

#include "tim.h"

/*============================ Global Flags ============================*/

volatile uint8_t  ecg_sample_flag      = 0;
volatile uint8_t  oled_update_flag     = 0;
volatile uint8_t  display_refresh_flag = 0;
volatile uint8_t  ecg_upload_flag      = 0;
volatile uint16_t seconds_counter      = 0;

/*============================ Private Counters ============================*/

static volatile uint16_t tick_counter = 0;    /**< 0–199, wraps every second */

/**
  * @brief  Initialize TIM2 as 200 Hz periodic interrupt timer
  */
void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    TIM2->CR1  = 0;
    TIM2->PSC  = 99;
    TIM2->ARR  = 4999;
    TIM2->CNT  = 0;
    TIM2->EGR  = TIM_EGR_UG;
    TIM2->SR   = 0;
    TIM2->DIER = TIM_DIER_UIE;

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_Start(void)
{
    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_Stop(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
}

/**
  * @brief  TIM2 ISR — 200 Hz multi-task flag scheduler
  *
  *         Mirrors v1.3 TIM3_IRQHandler task distribution:
  *         ┌────────────────────────────────────────────┐
  *         │  tick % 1   → ecg_sample_flag   (200Hz)   │
  *         │  tick % 8   → oled_update_flag  ( 25Hz)   │
  *         │  tick % 20  → ecg_upload_flag   ( 10Hz)   │
  *         │  tick % 40  → display_refresh   (  5Hz)   │
  *         │  tick == 0  → seconds_counter++ (  1Hz)   │
  *         └────────────────────────────────────────────┘
  */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR = ~TIM_SR_UIF;

        /* 200Hz: ECG sampling */
        ecg_sample_flag = 1;

        tick_counter++;

        /* 25Hz: OLED framebuffer flush */
        if ((tick_counter % 8) == 0)
        {
            oled_update_flag = 1;
        }

        /* 10Hz: ECG upload batch trigger */
        if ((tick_counter % 20) == 0)
        {
            ecg_upload_flag = 1;
        }

        /* 5Hz: display info refresh */
        if ((tick_counter % 40) == 0)
        {
            display_refresh_flag = 1;
        }

        /* 1Hz: seconds counter */
        if (tick_counter >= 200)
        {
            tick_counter = 0;
            seconds_counter++;
        }
    }
}
