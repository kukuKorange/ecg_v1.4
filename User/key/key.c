/**
  ******************************************************************************
  * @file    key.c
  * @brief   Key driver (PA11/PA12/PA15) + page navigation
  *
  * @details Non-blocking scan with debounce + short/long press detection.
  *          Key_Process() handles page switching (ported from v1.3 Key_Process).
  ******************************************************************************
  */

#include "key.h"
#include "../display/display.h"
#include "../led/led.h"

/*============================ Internal State ============================*/

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       debounce_cnt;
    uint8_t       stable;
    uint16_t      hold_cnt;
    uint8_t       long_triggered;
    uint8_t       event;
} KeyState_t;

static KeyState_t keys[KEY_COUNT] = {
    { KEY1_GPIO_PORT, KEY1_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
    { KEY2_GPIO_PORT, KEY2_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
    { KEY3_GPIO_PORT, KEY3_GPIO_PIN, 0, 0, 0, 0, KEY_EVENT_NONE },
};

/*============================ Raw Read ============================*/

uint8_t Key_ReadRaw(uint8_t key_id)
{
    if (key_id >= KEY_COUNT) return 0;
    return (HAL_GPIO_ReadPin(keys[key_id].port, keys[key_id].pin) == KEY_PRESSED_LEVEL) ? 1 : 0;
}

/*============================ Non-blocking Scan ============================*/

/**
  * @brief  Rate-limited key scan (~5ms interval via HAL_GetTick)
  * @note   Safe to call from a fast main loop.
  *         Debounce:   3 × 5ms = 15ms
  *         Long press: 200 × 5ms = 1000ms
  */
void Key_Scan(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_tick < 5) return;
    last_tick = now;

    for (uint8_t i = 0; i < KEY_COUNT; i++)
    {
        uint8_t raw = Key_ReadRaw(i);

        if (raw != keys[i].stable)
        {
            keys[i].debounce_cnt++;
            if (keys[i].debounce_cnt >= KEY_DEBOUNCE_COUNT)
            {
                keys[i].debounce_cnt = 0;
                keys[i].stable = raw;

                if (raw)
                {
                    keys[i].hold_cnt = 0;
                    keys[i].long_triggered = 0;
                }
                else
                {
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

/*============================ Get Event ============================*/

uint8_t Key_GetEvent(uint8_t key_id)
{
    if (key_id >= KEY_COUNT) return KEY_EVENT_NONE;
    uint8_t evt = keys[key_id].event;
    keys[key_id].event = KEY_EVENT_NONE;
    return evt;
}

/*============================ Blocking Wait ============================*/

uint8_t Key_WaitPress(void)
{
    while (Key_ReadRaw(KEY1) || Key_ReadRaw(KEY2) || Key_ReadRaw(KEY3))
    {
        HAL_Delay(10);
    }
    HAL_Delay(20);

    while (1)
    {
        for (uint8_t i = 0; i < KEY_COUNT; i++)
        {
            if (Key_ReadRaw(i))
            {
                HAL_Delay(20);
                if (Key_ReadRaw(i))
                    return i;
            }
        }
        HAL_Delay(10);
    }
}

/*============================ Page Navigation (from v1.3 Key_Process) ============================*/

/**
  * @brief  Key event handler — page navigation + function keys
  *
  *         ┌─────────────────────────────────────┐
  *         │  [KEY1]      [KEY2]      [KEY3]     │
  *         │  上一页      功能键       下一页     │
  *         └─────────────────────────────────────┘
  */
void Key_Process(void)
{
    uint8_t evt;

    /* KEY1: previous page (short or long press) */
    evt = Key_GetEvent(KEY1);
    if (evt == KEY_EVENT_SHORT_PRESS || evt == KEY_EVENT_LONG_PRESS)
    {
        if (current_page > 0)
            current_page--;
        else
            current_page = PAGE_MAX - 1;
    }

    /* KEY2: function key (short press only) */
    evt = Key_GetEvent(KEY2);
    if (evt == KEY_EVENT_SHORT_PRESS)
    {
        if (current_page == PAGE_ECG)
        {
            LED_Toggle();
        }
    }

    /* KEY3: next page (short or long press) */
    evt = Key_GetEvent(KEY3);
    if (evt == KEY_EVENT_SHORT_PRESS || evt == KEY_EVENT_LONG_PRESS)
    {
        if (current_page < PAGE_MAX - 1)
            current_page++;
        else
            current_page = 0;
    }
}
