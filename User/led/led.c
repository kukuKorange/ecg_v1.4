/**
  ******************************************************************************
  * @file    led.c
  * @brief   LED驱动 (PB10, 推挽输出, 低电平点亮)
  ******************************************************************************
  */

#include "led.h"

/**
 * @brief  点亮LED (输出低电平)
 */
void LED_On(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  熄灭LED (输出高电平)
 */
void LED_Off(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  翻转LED状态
 */
void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
}

/**
 * @brief  设置LED状态
 * @param  state: LED_ON(1) 点亮, LED_OFF(0) 熄灭
 */
void LED_SetState(uint8_t state)
{
    if (state)
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  获取LED当前状态
 * @retval 1: 亮, 0: 灭
 */
uint8_t LED_GetState(void)
{
    /* 低电平为亮, 取反 */
    return (HAL_GPIO_ReadPin(LED_GPIO_PORT, LED_GPIO_PIN) == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief  LED闪烁 (阻塞式)
 * @param  times: 闪烁次数
 * @param  interval_ms: 亮灭间隔(毫秒)
 */
void LED_Blink(uint8_t times, uint16_t interval_ms)
{
    for (uint8_t i = 0; i < times; i++)
    {
        LED_On();
        HAL_Delay(interval_ms);
        LED_Off();
        HAL_Delay(interval_ms);
    }
}

