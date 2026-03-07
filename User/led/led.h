#ifndef __LED_H
#define __LED_H

#include "stm32f4xx_hal.h"

/* LED GPIO 定义 (PB10) */
#define LED_GPIO_PORT       GPIOB
#define LED_GPIO_PIN        GPIO_PIN_10

/* LED 状态定义 */
#define LED_ON              1
#define LED_OFF             0

/**
 * @brief  点亮LED
 */
void LED_On(void);

/**
 * @brief  熄灭LED
 */
void LED_Off(void);

/**
 * @brief  翻转LED状态
 */
void LED_Toggle(void);

/**
 * @brief  设置LED状态
 * @param  state: LED_ON(1) 或 LED_OFF(0)
 */
void LED_SetState(uint8_t state);

/**
 * @brief  获取LED当前状态
 * @retval 1: 亮, 0: 灭
 */
uint8_t LED_GetState(void);

/**
 * @brief  LED闪烁
 * @param  times: 闪烁次数
 * @param  interval_ms: 每次亮灭的间隔(毫秒)
 */
void LED_Blink(uint8_t times, uint16_t interval_ms);

#endif /* __LED_H */

