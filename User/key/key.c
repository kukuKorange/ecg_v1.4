/**
  ******************************************************************************
  * @file    key.c
  * @brief   三按键驱动 (PA11 / PA12 / PA15, 输入模式, 支持消抖 + 短按/长按)
  ******************************************************************************
  */

#include "key.h"

/* ---------- 内部数据结构 ---------- */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       debounce_cnt;   /* 消抖计数器 */
    uint8_t       stable;         /* 稳定后的状态 1=按下 0=松开 */
    uint16_t      hold_cnt;       /* 按住计数器 */
    uint8_t       long_triggered; /* 长按已触发标志 */
    uint8_t       event;          /* 待读取事件 */
} KeyState_t;

static KeyState_t keys[KEY_COUNT] = {
    { KEY1_GPIO_PORT, KEY1_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
    { KEY2_GPIO_PORT, KEY2_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
    { KEY3_GPIO_PORT, KEY3_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
};

/* ---------- 读原始电平 ---------- */
/**
 * @brief  读取指定按键的原始电平 (无消抖)
 * @param  key_id: KEY1 / KEY2 / KEY3
 * @retval 1: 按下, 0: 松开
 */
uint8_t Key_ReadRaw(uint8_t key_id)
{
    if (key_id >= KEY_COUNT) return 0;
    return (HAL_GPIO_ReadPin(keys[key_id].port, keys[key_id].pin) == KEY_PRESSED_LEVEL) ? 1 : 0;
}

/* ---------- 周期扫描 (非阻塞, 含消抖 + 长按检测) ---------- */
/**
 * @brief  按键扫描, 需周期性调用 (建议 10~20ms)
 */
void Key_Scan(void)
{
    for (uint8_t i = 0; i < KEY_COUNT; i++)
    {
        uint8_t raw = Key_ReadRaw(i);

        /* ---- 消抖 ---- */
        if (raw != keys[i].stable)
        {
            keys[i].debounce_cnt++;
            if (keys[i].debounce_cnt >= KEY_DEBOUNCE_COUNT)
            {
                keys[i].debounce_cnt = 0;
                keys[i].stable = raw;

                if (raw)
                {
                    /* 按下: 重置计数 */
                    keys[i].hold_cnt = 0;
                    keys[i].long_triggered = 0;
                }
                else
                {
                    /* 松开: 若未触发长按则判定为短按 */
                    if (!keys[i].long_triggered)
                    {
                        keys[i].event = KEY_EVENT_SHORT_PRESS;
                    }
                }
            }
        }
        else
        {
            keys[i].debounce_cnt = 0;
        }

        /* ---- 长按检测 ---- */
        if (keys[i].stable && !keys[i].long_triggered)
        {
            keys[i].hold_cnt++;
            if (keys[i].hold_cnt >= KEY_LONG_PRESS_COUNT)
            {
                keys[i].long_triggered = 1;
                keys[i].event = KEY_EVENT_LONG_PRESS;
            }
        }
    }
}

/* ---------- 获取事件 (读后清除) ---------- */
/**
 * @brief  获取按键事件
 * @param  key_id: KEY1 / KEY2 / KEY3
 * @retval KEY_EVENT_NONE / KEY_EVENT_SHORT_PRESS / KEY_EVENT_LONG_PRESS
 */
uint8_t Key_GetEvent(uint8_t key_id)
{
    if (key_id >= KEY_COUNT) return KEY_EVENT_NONE;
    uint8_t evt = keys[key_id].event;
    keys[key_id].event = KEY_EVENT_NONE;
    return evt;
}

/* ---------- 阻塞式等待按键 ---------- */
/**
 * @brief  阻塞等待任意按键按下 (含消抖)
 * @retval KEY1 / KEY2 / KEY3
 */
uint8_t Key_WaitPress(void)
{
    /* 等待所有按键松开 */
    while (Key_ReadRaw(KEY1) || Key_ReadRaw(KEY2) || Key_ReadRaw(KEY3))
    {
        HAL_Delay(10);
    }
    HAL_Delay(20);

    /* 等待按下 */
    while (1)
    {
        for (uint8_t i = 0; i < KEY_COUNT; i++)
        {
            if (Key_ReadRaw(i))
            {
                HAL_Delay(20);  /* 消抖 */
                if (Key_ReadRaw(i))
                    return i;
            }
        }
        HAL_Delay(10);
    }
}

