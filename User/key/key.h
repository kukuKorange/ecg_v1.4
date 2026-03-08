/**
  ******************************************************************************
  * @file    key.h
  * @brief   Key driver (PA11/PA12/PA15, non-blocking scan + page navigation)
  *
  * @details Ported from v1.3 Key.h:
  *          - KEY1: previous page
  *          - KEY2: function key
  *          - KEY3: next page
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx_hal.h"

/*============================ GPIO Definitions ============================*/

#define KEY1_GPIO_PORT      GPIOA
#define KEY1_GPIO_PIN       GPIO_PIN_11

#define KEY2_GPIO_PORT      GPIOA
#define KEY2_GPIO_PIN       GPIO_PIN_12

#define KEY3_GPIO_PORT      GPIOA
#define KEY3_GPIO_PIN       GPIO_PIN_15

/*============================ Key IDs ============================*/

#define KEY1                0
#define KEY2                1
#define KEY3                2
#define KEY_COUNT           3

#define KEY_PRESSED_LEVEL   GPIO_PIN_RESET

/*============================ Events ============================*/

#define KEY_EVENT_NONE          0
#define KEY_EVENT_SHORT_PRESS   1
#define KEY_EVENT_LONG_PRESS    2

/*============================ Timing (unit: Key_Scan calls) ============================*/

#define KEY_DEBOUNCE_COUNT      3       /* 3 calls × ~5ms = 15ms debounce */
#define KEY_LONG_PRESS_COUNT    200     /* 200 calls × ~5ms = 1s long press */

/*============================ Functions ============================*/

uint8_t Key_ReadRaw(uint8_t key_id);
void    Key_Scan(void);
uint8_t Key_GetEvent(uint8_t key_id);
uint8_t Key_WaitPress(void);

/**
  * @brief  Key event handler — page navigation + function keys
  * @note   Ported from v1.3 Key_Process():
  *         KEY1 short: previous page
  *         KEY2 short: function key (LED toggle on ECG page)
  *         KEY3 short: next page
  */
void Key_Process(void);

#endif /* __KEY_H */
