/**
  ******************************************************************************
  * @file    key.c
  * @brief   Key driver (PA11/PA12/PA15) + page-specific actions
  *
  * @details Non-blocking scan with debounce + short/long press detection.
  *          Key_Process() dispatches to page-specific handlers.
  ******************************************************************************
  */

#include "key.h"
#include "../display/display.h"
#include "../led/led.h"
#include "../recorder/recorder.h"
#include "../userinfo/userinfo.h"
#include "../ad8232/AD8232.h"
#include "../beep/beep.h"

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
                        keys[i].event = KEY_EVENT_SHORT_PRESS;
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
        HAL_Delay(10);
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

/*============================ Page: ECG ============================*/

static void Key_Page_ECG(void)
{
    uint8_t e1 = Key_GetEvent(KEY1);
    uint8_t e2 = Key_GetEvent(KEY2);
    uint8_t e3 = Key_GetEvent(KEY3);

    /* K2: Start / Stop monitoring */
    if (e2 == KEY_EVENT_SHORT_PRESS || e2 == KEY_EVENT_LONG_PRESS)
    {
        if (recorder_recording)
        {
            Recorder_Stop();
            monitor_state = MONITOR_DONE;
        }
        else
        {
            Recorder_Start(setting_mode, setting_timed_dur);
            monitor_state = MONITOR_RUNNING;
        }
    }

    /* K1 / K3: page navigation (only when not recording) */
    if (!recorder_recording)
    {
        if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
        {
            if (current_page > 0)
                current_page--;
            else
                current_page = PAGE_MAX - 1;
        }
        if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
        {
            if (current_page < PAGE_MAX - 1)
                current_page++;
            else
                current_page = 0;
        }
    }
}

/*============================ Page: History ============================*/

static void Key_Page_History(void)
{
    uint8_t e1 = Key_GetEvent(KEY1);
    uint8_t e2 = Key_GetEvent(KEY2);
    uint8_t e3 = Key_GetEvent(KEY3);

    if (hist_view == HIST_VIEW_LIST)
    {
        uint8_t cnt = Recorder_GetCount();

        /* K1: scroll up / prev page */
        if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
        {
            if (cnt == 0)
            {
                current_page = PAGE_ECG;
            }
            else if (hist_cursor > 0)
            {
                hist_cursor--;
            }
            else
            {
                current_page = PAGE_ECG;
            }
        }

        /* K3: scroll down / next page */
        if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
        {
            if (cnt == 0)
            {
                current_page = PAGE_USER;
            }
            else if (hist_cursor < cnt - 1)
            {
                hist_cursor++;
            }
            else
            {
                current_page = PAGE_USER;
            }
        }

        /* K2: enter playback */
        if (e2 == KEY_EVENT_SHORT_PRESS && cnt > 0)
        {
            hist_view = HIST_VIEW_PLAYBACK;
            hist_play_offset = 0;
        }
    }
    else /* HIST_VIEW_PLAYBACK */
    {
        ECGRecord_t rec;
        uint32_t step = 120;

        if (Recorder_LoadRecord(hist_cursor, &rec) != 0)
        {
            hist_view = HIST_VIEW_LIST;
            return;
        }

        /* K1: scroll backward */
        if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
        {
            if (hist_play_offset >= step)
                hist_play_offset -= step;
            else
                hist_play_offset = 0;
        }

        /* K3: scroll forward */
        if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
        {
            if (hist_play_offset + step < rec.num_samples)
                hist_play_offset += step;
        }

        /* K2: back to list */
        if (e2 == KEY_EVENT_SHORT_PRESS)
        {
            hist_view = HIST_VIEW_LIST;
        }
    }
}

/*============================ Page: User Info ============================*/

static const char char_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
#define CHAR_TABLE_LEN  (sizeof(char_table) - 1)

static uint8_t char_to_idx(char c)
{
    uint8_t i;
    for (i = 0; i < CHAR_TABLE_LEN; i++)
        if (char_table[i] == c) return i;
    return CHAR_TABLE_LEN - 1;  /* default to space */
}

static void Key_Page_User(void)
{
    uint8_t e1 = Key_GetEvent(KEY1);
    uint8_t e2 = Key_GetEvent(KEY2);
    uint8_t e3 = Key_GetEvent(KEY3);

    switch (uedit_field)
    {
        case UEDIT_VIEW:
            /* K1/K3: page navigation */
            if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
            {
                current_page = PAGE_HISTORY;
            }
            if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
            {
                current_page = PAGE_SETTINGS;
            }
            /* K2: enter edit mode (start with name) */
            if (e2 == KEY_EVENT_SHORT_PRESS || e2 == KEY_EVENT_LONG_PRESS)
            {
                uedit_field = UEDIT_FIELD_NAME;
                uedit_name_pos = 0;
                uedit_save_tip = 0;
            }
            break;

        case UEDIT_FIELD_NAME:
        {
            uint8_t ci;

            /* K1: prev character */
            if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
            {
                ci = char_to_idx(uedit_name_buf[uedit_name_pos]);
                if (ci > 0) ci--; else ci = CHAR_TABLE_LEN - 1;
                uedit_name_buf[uedit_name_pos] = char_table[ci];
            }

            /* K3: next character */
            if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
            {
                ci = char_to_idx(uedit_name_buf[uedit_name_pos]);
                ci = (ci + 1) % CHAR_TABLE_LEN;
                uedit_name_buf[uedit_name_pos] = char_table[ci];
            }

            /* K2 short: confirm char, advance position */
            if (e2 == KEY_EVENT_SHORT_PRESS)
            {
                uedit_name_pos++;
                if (uedit_name_pos >= USER_NAME_MAX)
                    uedit_field = UEDIT_FIELD_AGE;
            }

            /* K2 long: jump to age field */
            if (e2 == KEY_EVENT_LONG_PRESS)
            {
                uedit_field = UEDIT_FIELD_AGE;
            }
            break;
        }

        case UEDIT_FIELD_AGE:
            /* K1: decrease age */
            if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
            {
                if (uedit_age_val > 1) uedit_age_val--;
            }

            /* K3: increase age */
            if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
            {
                if (uedit_age_val < 150) uedit_age_val++;
            }

            /* K2 short: back to name edit */
            if (e2 == KEY_EVENT_SHORT_PRESS)
            {
                uedit_field = UEDIT_FIELD_NAME;
                uedit_name_pos = 0;
            }

            /* K2 long: save and return to view mode */
            if (e2 == KEY_EVENT_LONG_PRESS)
            {
                memcpy(user_info.name, uedit_name_buf, USER_NAME_MAX);
                user_info.name[USER_NAME_MAX] = '\0';
                user_info.age = uedit_age_val;
                UserInfo_Save();
                uedit_save_tip = 10;   /* show tip for ~2 seconds (10 x 5Hz) */
                uedit_field = UEDIT_VIEW;
            }
            break;
    }
}

/*============================ Page: Settings ============================*/

static void Key_Page_Settings(void)
{
    uint8_t e1 = Key_GetEvent(KEY1);
    uint8_t e2 = Key_GetEvent(KEY2);
    uint8_t e3 = Key_GetEvent(KEY3);

    /* K1: cursor up / prev page */
    if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
    {
        if (setting_cursor > 0)
            setting_cursor--;
        else
            current_page = PAGE_USER;
    }

    /* K3: cursor down / next page */
    if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
    {
        if (setting_cursor < SETTING_ITEM_MAX - 1)
            setting_cursor++;
        else
        {
#ifdef FEATURE_DEBUG_PAGE
            current_page = PAGE_DEBUG;
#else
            current_page = PAGE_ECG;
#endif
        }
    }

    /* K2: toggle / select */
    if (e2 == KEY_EVENT_SHORT_PRESS || e2 == KEY_EVENT_LONG_PRESS)
    {
        switch (setting_cursor)
        {
            case SETTING_MODE:
                setting_mode = (setting_mode == MODE_CONTINUOUS)
                               ? MODE_TIMED : MODE_CONTINUOUS;
                break;

            case SETTING_DURATION:
                if (setting_mode == MODE_TIMED)
                {
                    if (setting_timed_dur == 30)
                        setting_timed_dur = 60;
                    else if (setting_timed_dur == 60)
                        setting_timed_dur = 300;
                    else
                        setting_timed_dur = 30;
                }
                break;

            case SETTING_DELETE:
                Recorder_DeleteAll();
                break;
        }
    }
}

#ifdef FEATURE_DEBUG_PAGE
/*============================ Page: Debug ============================*/

static void Key_Page_Debug(void)
{
    uint8_t e1 = Key_GetEvent(KEY1);
    uint8_t e2 = Key_GetEvent(KEY2);
    uint8_t e3 = Key_GetEvent(KEY3);

    /* K1: prev page (Settings) */
    if (e1 == KEY_EVENT_SHORT_PRESS || e1 == KEY_EVENT_LONG_PRESS)
        current_page = PAGE_SETTINGS;

    /* K3: next page (ECG, wraps around) */
    if (e3 == KEY_EVENT_SHORT_PRESS || e3 == KEY_EVENT_LONG_PRESS)
        current_page = PAGE_ECG;

    /* K2: toggle LED (useful for testing) */
    if (e2 == KEY_EVENT_SHORT_PRESS)
        LED_Toggle();
}
#endif /* FEATURE_DEBUG_PAGE */

/*============================ Main Dispatcher ============================*/

void Key_Process(void)
{
    switch (current_page)
    {
        case PAGE_ECG:      Key_Page_ECG();      break;
        case PAGE_HISTORY:  Key_Page_History();   break;
        case PAGE_USER:     Key_Page_User();      break;
        case PAGE_SETTINGS: Key_Page_Settings();  break;
#ifdef FEATURE_DEBUG_PAGE
        case PAGE_DEBUG:    Key_Page_Debug();     break;
#endif
        default: break;
    }
}
