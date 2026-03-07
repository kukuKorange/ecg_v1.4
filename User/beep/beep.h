#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f4xx_hal.h"

/* 蜂鸣器 GPIO 定义 (PA8) */
#define BEEP_GPIO_PORT      GPIOA
#define BEEP_GPIO_PIN       GPIO_PIN_8

/* 蜂鸣器状态定义 */
#define BEEP_ON             1
#define BEEP_OFF            0

/**
 * @brief  打开蜂鸣器
 */
void Beep_On(void);

/**
 * @brief  关闭蜂鸣器
 */
void Beep_Off(void);

/**
 * @brief  翻转蜂鸣器状态
 */
void Beep_Toggle(void);

/**
 * @brief  设置蜂鸣器状态
 * @param  state: BEEP_ON(1) 或 BEEP_OFF(0)
 */
void Beep_SetState(uint8_t state);

/**
 * @brief  获取蜂鸣器当前状态
 * @retval 1: 响, 0: 停
 */
uint8_t Beep_GetState(void);

/**
 * @brief  蜂鸣器鸣叫指定时长 (阻塞式)
 * @param  duration_ms: 鸣叫时长(毫秒)
 */
void Beep_Beep(uint16_t duration_ms);

/**
 * @brief  蜂鸣器间歇鸣叫 (阻塞式)
 * @param  times: 鸣叫次数
 * @param  on_ms: 每次鸣叫时长(毫秒)
 * @param  off_ms: 每次间歇时长(毫秒)
 */
void Beep_BeepTimes(uint8_t times, uint16_t on_ms, uint16_t off_ms);

#endif /* __BEEP_H */

