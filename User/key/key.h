#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx_hal.h"

/* 按键 GPIO 定义 */
#define KEY1_GPIO_PORT      GPIOA
#define KEY1_GPIO_PIN       GPIO_PIN_11

#define KEY2_GPIO_PORT      GPIOA
#define KEY2_GPIO_PIN       GPIO_PIN_12

#define KEY3_GPIO_PORT      GPIOA
#define KEY3_GPIO_PIN       GPIO_PIN_15

/* 按键编号 */
#define KEY1                0
#define KEY2                1
#define KEY3                2
#define KEY_COUNT           3

/* 按键按下时的电平 (低电平按下, 配合外部上拉; 若高电平按下请改为 GPIO_PIN_SET) */
#define KEY_PRESSED_LEVEL   GPIO_PIN_RESET

/* 按键事件 */
#define KEY_EVENT_NONE          0   /* 无事件 */
#define KEY_EVENT_SHORT_PRESS   1   /* 短按 (松手后触发) */
#define KEY_EVENT_LONG_PRESS    2   /* 长按 (达到阈值时触发) */

/* 消抖与长按参数 (单位: 调用 Key_Scan 的次数) */
#define KEY_DEBOUNCE_COUNT      3       /* 消抖计数 (若10ms扫描一次 = 30ms) */
#define KEY_LONG_PRESS_COUNT    100     /* 长按阈值 (若10ms扫描一次 = 1s) */

/**
 * @brief  读取指定按键的原始电平 (无消抖)
 * @param  key_id: KEY1 / KEY2 / KEY3
 * @retval 1: 按下, 0: 松开
 */
uint8_t Key_ReadRaw(uint8_t key_id);

/**
 * @brief  按键扫描 (需周期性调用, 建议10~20ms一次, 含消抖)
 * @note   支持同时检测三个按键, 非阻塞
 */
void Key_Scan(void);

/**
 * @brief  获取按键事件 (读取后自动清除)
 * @param  key_id: KEY1 / KEY2 / KEY3
 * @retval KEY_EVENT_NONE / KEY_EVENT_SHORT_PRESS / KEY_EVENT_LONG_PRESS
 */
uint8_t Key_GetEvent(uint8_t key_id);

/**
 * @brief  阻塞式等待按键按下并返回按键编号 (含消抖)
 * @retval KEY1 / KEY2 / KEY3, 0xFF 表示无按键
 */
uint8_t Key_WaitPress(void);

#endif /* __KEY_H */

