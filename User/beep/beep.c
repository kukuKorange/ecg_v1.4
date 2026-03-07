/**
  ******************************************************************************
  * @file    beep.c
  * @brief   蜂鸣器驱动 (PA8, 推挽输出, 高电平驱动)
  ******************************************************************************
  */

#include "beep.h"

/**
 * @brief  打开蜂鸣器 (输出高电平; 若低电平驱动请交换 SET/RESET)
 */
void Beep_On(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  关闭蜂鸣器
 */
void Beep_Off(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  翻转蜂鸣器状态
 */
void Beep_Toggle(void)
{
    HAL_GPIO_TogglePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN);
}

/**
 * @brief  设置蜂鸣器状态
 * @param  state: BEEP_ON(1) 响, BEEP_OFF(0) 停
 */
void Beep_SetState(uint8_t state)
{
    if (state)
        HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  获取蜂鸣器当前状态
 * @retval 1: 响, 0: 停
 */
uint8_t Beep_GetState(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(BEEP_GPIO_PORT, BEEP_GPIO_PIN);
}

/**
 * @brief  蜂鸣器鸣叫指定时长 (阻塞式)
 * @param  duration_ms: 鸣叫时长(毫秒)
 */
void Beep_Beep(uint16_t duration_ms)
{
    Beep_On();
    HAL_Delay(duration_ms);
    Beep_Off();
}

/**
 * @brief  蜂鸣器间歇鸣叫 (阻塞式)
 * @param  times: 鸣叫次数
 * @param  on_ms: 每次鸣叫时长(毫秒)
 * @param  off_ms: 每次间歇时长(毫秒)
 */
void Beep_BeepTimes(uint8_t times, uint16_t on_ms, uint16_t off_ms)
{
    for (uint8_t i = 0; i < times; i++)
    {
        Beep_On();
        HAL_Delay(on_ms);
        Beep_Off();
        if (i < times - 1)  /* 最后一次不等待 */
            HAL_Delay(off_ms);
    }
}

