/**
  ******************************************************************************
  * @file    display.c
  * @brief   OLED display module — 4-page system
  ******************************************************************************
  */

#include "display.h"
#include "tim.h"
#include "../oled/OLED.h"
#include "../ad8232/AD8232.h"
#include "../battery/battery.h"
#include "../recorder/recorder.h"
#include "../userinfo/userinfo.h"
#include "../key/key.h"
#include "../led/led.h"
#include "../beep/beep.h"
#include "adc.h"
#include <string.h>

/*============================ Global Variables ============================*/

uint8_t current_page = PAGE_ECG;

/* History page */
uint8_t  hist_view        = HIST_VIEW_LIST;
uint8_t  hist_cursor      = 0;
uint32_t hist_play_offset = 0;

/* User-edit page */
uint8_t  uedit_field    = UEDIT_FIELD_NAME;
uint8_t  uedit_name_pos = 0;
char     uedit_name_buf[9] = {0};
uint8_t  uedit_age_val  = 25;
uint8_t  uedit_save_tip = 0;

/* Settings page */
uint8_t  setting_cursor    = 0;
uint8_t  setting_mode      = MODE_CONTINUOUS;
uint16_t setting_timed_dur = 30;

/*============================ Private Variables ============================*/

static uint8_t last_page = 0xFF;
static uint8_t lead_off_blink = 0;

/*============================ Forward Declarations ============================*/

static void Display_StatusBar(void);
static void Display_PageECG(void);
static void Display_PageHistory(void);
static void Display_PageUser(void);
static void Display_PageSettings(void);
#ifdef FEATURE_DEBUG_PAGE
static void Display_PageDebug(void);
#endif

/*============================ Helpers ============================*/

static char * mode_str(uint8_t mode, uint16_t dur)
{
    if (mode == MODE_CONTINUOUS) return (char *)"CONT";
    switch (dur) {
        case 30:  return (char *)" 30s";
        case 60:  return (char *)"  1m";
        case 300: return (char *)"  5m";
        default:  return (char *)"  T?";
    }
}

static void show_time_mmss(uint8_t x, uint8_t y, uint16_t total_sec)
{
    uint8_t m = total_sec / 60;
    uint8_t s = total_sec % 60;
    OLED_ShowNum(x, y, m, 2, OLED_6X8);
    OLED_ShowChar(x + 12, y, ':', OLED_6X8);
    OLED_ShowNum(x + 18, y, s, 2, OLED_6X8);
}

/*============================ Display Entry ============================*/

void Display_Init(void)
{
    current_page = PAGE_ECG;
    last_page    = 0xFF;
    hist_view    = HIST_VIEW_LIST;
    hist_cursor  = 0;

    memcpy(uedit_name_buf, user_info.name, USER_NAME_MAX);
    uedit_name_buf[USER_NAME_MAX] = '\0';
    uedit_age_val = user_info.age;
}

void Display_Update(void)
{
    /* ---- Page transition ---- */
    if (current_page != last_page)
    {
        OLED_Clear();

        if (current_page == PAGE_ECG)
        {
            AD8232_Init();
            ECG_ClearAndRedraw();
        }
        else if (current_page == PAGE_HISTORY)
        {
            hist_view = HIST_VIEW_LIST;
            hist_cursor = 0;
        }
        else if (current_page == PAGE_USER)
        {
            uedit_field = UEDIT_VIEW;
            uedit_name_pos = 0;
            uedit_save_tip = 0;
            memcpy(uedit_name_buf, user_info.name, USER_NAME_MAX);
            uedit_name_buf[USER_NAME_MAX] = '\0';
            uedit_age_val = user_info.age;
        }

        last_page = current_page;
    }

    /* ---- Per-page processing ---- */
    switch (current_page)
    {
        case PAGE_ECG:      Display_PageECG();       break;
        case PAGE_HISTORY:  Display_PageHistory();   break;
        case PAGE_USER:     Display_PageUser();      break;
        case PAGE_SETTINGS: Display_PageSettings();  break;
#ifdef FEATURE_DEBUG_PAGE
        case PAGE_DEBUG:    Display_PageDebug();     break;
#endif
        default:            current_page = PAGE_ECG; break;
    }
}

/*============================ Status Bar ============================*/

static void Display_StatusBar(void)
{
    /* Battery percentage (top-right) */
    if (battery_low_flag && (seconds_counter & 1))
    {
        OLED_ShowString(104, 0, "LOW", OLED_6X8);
    }
    else
    {
        OLED_ShowNum(104, 0, battery_percent, 3, OLED_6X8);
        OLED_ShowChar(122, 0, '%', OLED_6X8);
    }
}

/*============================ PAGE 0: ECG ============================*/

static void Display_PageECG(void)
{
    /* 200Hz: ECG sampling + waveform drawing */
    if (ecg_sample_flag)
    {
        ecg_sample_flag = 0;

        if (AD8232_GetConnect())
        {
            ECG_SampleAndDraw();
            if (recorder_recording)
                Recorder_FeedSample(ecg_last_filtered, ECG_GetHeartRate());
        }
    }

    /* 25Hz: overlay + OLED flush */
    if (oled_update_flag)
    {
        uint8_t ecg_hr;

        oled_update_flag = 0;

        /* Title + mode */
        OLED_ShowString(0, 0, "ECG", OLED_6X8);

        /* Recording indicator */
        if (recorder_recording)
        {
            OLED_ShowString(20, 0, "REC", OLED_6X8);
        }
        else
        {
            OLED_ShowString(20, 0, mode_str(setting_mode, setting_timed_dur), OLED_6X8);
        }

        /* Elapsed time MM:SS */
        show_time_mmss(46, 0, recorder_recording ? recorder_elapsed_sec : seconds_counter);

        /* Heart rate */
        ecg_hr = ECG_GetHeartRate();
        OLED_ShowChar(80, 0, 'H', OLED_6X8);
        OLED_ShowNum(86, 0, ecg_hr, 3, OLED_6X8);

        /* Status bar (battery) */
        Display_StatusBar();

        /* Axes */
        OLED_DrawLine(1, 54, 120, 54);
        OLED_DrawLine(1, 10, 1, 54);
        OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);
        OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);

        /* Lead-off warning overlay */
        if (!AD8232_GetConnect())
        {
            lead_off_blink = !lead_off_blink;
            if (lead_off_blink)
            {
                OLED_ShowString(20, 26, "LEAD OFF!", OLED_8X16);
            }
            else
            {
                OLED_ClearArea(20, 26, 90, 16);
            }
        }

        /* Bottom bar */
        OLED_ShowString(0, 56, "<K1", OLED_6X8);
        if (recorder_recording)
            OLED_ShowString(40, 56, "STOP", OLED_6X8);
        else
            OLED_ShowString(36, 56, "START", OLED_6X8);
        OLED_ShowString(110, 56, "K3>", OLED_6X8);

        OLED_Update();
    }
}

/*============================ PAGE 1: History ============================*/

static void Display_PageHistory(void)
{
    if (!display_refresh_flag)
        return;
    display_refresh_flag = 0;

    OLED_Clear();

    if (hist_view == HIST_VIEW_LIST)
    {
        uint8_t count = Recorder_GetCount();
        uint8_t i;
        ECGRecord_t rec;

        OLED_ShowString(0, 0, "[History]", OLED_6X8);
        OLED_ShowNum(60, 0, count, 2, OLED_6X8);

        if (count == 0)
        {
            OLED_ShowString(16, 28, "No records", OLED_6X8);
        }
        else
        {
            uint8_t start = 0;
            if (hist_cursor > 3) start = hist_cursor - 3;
            if (start + 4 > count) start = (count > 4) ? count - 4 : 0;

            for (i = 0; i < 4 && (start + i) < count; i++)
            {
                uint8_t idx = start + i;
                uint8_t ypos = 12 + i * 10;

                if (Recorder_LoadRecord(idx, &rec) == 0)
                {
                    if (idx == hist_cursor)
                        OLED_ShowChar(0, ypos, '>', OLED_6X8);

                    OLED_ShowChar(8, ypos, '#', OLED_6X8);
                    OLED_ShowNum(14, ypos, idx + 1, 2, OLED_6X8);

                    OLED_ShowString(30, ypos,
                        rec.mode == MODE_CONTINUOUS ? "C" : "T", OLED_6X8);

                    show_time_mmss(40, ypos, rec.duration_sec);

                    OLED_ShowString(78, ypos, "HR", OLED_6X8);
                    OLED_ShowNum(92, ypos, rec.avg_hr, 3, OLED_6X8);
                }
            }
        }

        OLED_ShowString(0, 56, "<K1", OLED_6X8);
        OLED_ShowString(36, 56, "VIEW", OLED_6X8);
        OLED_ShowString(110, 56, "K3>", OLED_6X8);
    }
    else /* HIST_VIEW_PLAYBACK */
    {
        ECGRecord_t rec;
        uint16_t samples[120];
        uint16_t loaded;
        uint8_t i;

        if (Recorder_LoadRecord(hist_cursor, &rec) != 0)
        {
            hist_view = HIST_VIEW_LIST;
            OLED_Update();
            return;
        }

        /* Header */
        OLED_ShowChar(0, 0, '#', OLED_6X8);
        OLED_ShowNum(6, 0, hist_cursor + 1, 2, OLED_6X8);
        OLED_ShowString(22, 0,
            rec.mode == MODE_CONTINUOUS ? "CONT" : "TIMED", OLED_6X8);
        OLED_ShowString(52, 0, "HR", OLED_6X8);
        OLED_ShowNum(64, 0, rec.avg_hr, 3, OLED_6X8);

        /* Progress */
        OLED_ShowNum(92, 0, (uint32_t)(hist_play_offset / 200), 3, OLED_6X8);
        OLED_ShowChar(110, 0, '/', OLED_6X8);
        OLED_ShowNum(116, 0, rec.duration_sec, 3, OLED_6X8);

        /* Load and draw waveform */
        loaded = Recorder_LoadSamples(hist_cursor, hist_play_offset,
                                      samples, 120);
        if (loaded > 1)
        {
            OLED_ClearArea(3, 10, 125, 45);
            OLED_DrawLine(1, 54, 120, 54);
            OLED_DrawLine(1, 10, 1, 54);

            for (i = 0; i < loaded - 1 && i < 119; i++)
            {
                int16_t y0 = 90 - (int16_t)(samples[i] / 45);
                int16_t y1 = 90 - (int16_t)(samples[i + 1] / 45);
                if (y0 < 11) y0 = 11; if (y0 > 53) y0 = 53;
                if (y1 < 11) y1 = 11; if (y1 > 53) y1 = 53;
                OLED_DrawLine(i + 3, (uint8_t)y0, i + 4, (uint8_t)y1);
            }
        }

        OLED_ShowString(0, 56, "<Prev", OLED_6X8);
        OLED_ShowString(36, 56, "BACK", OLED_6X8);
        OLED_ShowString(98, 56, "Next>", OLED_6X8);
    }

    OLED_Update();
}

/*============================ PAGE 2: User Info ============================*/

static void Display_PageUser(void)
{
    if (!display_refresh_flag)
        return;
    display_refresh_flag = 0;

    /* Decrement save-tip timer */
    if (uedit_save_tip > 0)
        uedit_save_tip--;

    OLED_Clear();
    OLED_ShowString(0, 0, "[User Info]", OLED_6X8);

    if (uedit_field != UEDIT_VIEW)
        OLED_ShowString(76, 0, "Edit", OLED_6X8);

    OLED_DrawLine(0, 10, 127, 10);

    /* Name field */
    OLED_ShowString(0, 14, "Name:", OLED_6X8);
    {
        uint8_t i;
        for (i = 0; i < USER_NAME_MAX; i++)
        {
            char ch = uedit_name_buf[i];
            if (ch == 0) ch = '_';
            OLED_ShowChar(36 + i * 8, 14, ch, OLED_8X16);
        }
    }

    /* Cursor underline for name (only in name-edit mode) */
    if (uedit_field == UEDIT_FIELD_NAME)
    {
        uint8_t cx = 36 + uedit_name_pos * 8;
        OLED_DrawLine(cx, 30, cx + 7, 30);
    }

    /* Age field */
    OLED_ShowString(0, 36, "Age:", OLED_6X8);
    OLED_ShowNum(30, 36, uedit_age_val, 3, OLED_8X16);

    if (uedit_field == UEDIT_FIELD_AGE)
    {
        OLED_DrawLine(30, 52, 53, 52);
    }

    /* Bottom bar — context-dependent */
    if (uedit_save_tip > 0)
    {
        OLED_ShowString(16, 56, "Save Succeed!", OLED_6X8);
    }
    else
    {
        OLED_ShowString(0, 56, "<K1", OLED_6X8);
        OLED_ShowString(110, 56, "K3>", OLED_6X8);

        switch (uedit_field)
        {
            case UEDIT_VIEW:
                OLED_ShowString(30, 56, "EDIT", OLED_6X8);
                break;
            case UEDIT_FIELD_NAME:
                OLED_ShowString(30, 56, "NEXT", OLED_6X8);
                break;
            case UEDIT_FIELD_AGE:
                OLED_ShowString(24, 56, "L-SAVE", OLED_6X8);
                break;
        }
    }

    OLED_Update();
}

/*============================ PAGE 3: Settings ============================*/

static const char *duration_strs[] = { "30s", " 1m", " 5m" };
static const uint16_t duration_vals[] = { 30, 60, 300 };
#define DURATION_OPT_COUNT  3

static uint8_t dur_index_from_val(uint16_t v)
{
    uint8_t i;
    for (i = 0; i < DURATION_OPT_COUNT; i++)
        if (duration_vals[i] == v) return i;
    return 0;
}

static void Display_PageSettings(void)
{
    uint8_t y;

    if (!display_refresh_flag)
        return;
    display_refresh_flag = 0;

    OLED_Clear();
    OLED_ShowString(0, 0, "[Settings]", OLED_6X8);
    OLED_DrawLine(0, 10, 127, 10);

    /* Mode */
    y = 14;
    if (setting_cursor == SETTING_MODE)
        OLED_ShowChar(0, y, '>', OLED_6X8);
    OLED_ShowString(8, y, "Mode:", OLED_6X8);
    OLED_ShowString(46, y,
        setting_mode == MODE_CONTINUOUS ? "Continuous" : "Timed", OLED_6X8);

    /* Duration (only meaningful for timed mode) */
    y = 26;
    if (setting_cursor == SETTING_DURATION)
        OLED_ShowChar(0, y, '>', OLED_6X8);
    OLED_ShowString(8, y, "Dur:", OLED_6X8);
    if (setting_mode == MODE_TIMED)
    {
        uint8_t di = dur_index_from_val(setting_timed_dur);
        OLED_ShowString(46, y, (char *)duration_strs[di], OLED_6X8);
    }
    else
    {
        OLED_ShowString(46, y, "---", OLED_6X8);
    }

    /* Delete all records */
    y = 38;
    if (setting_cursor == SETTING_DELETE)
        OLED_ShowChar(0, y, '>', OLED_6X8);
    OLED_ShowString(8, y, "Del All Rec", OLED_6X8);
    OLED_ShowNum(80, y, Recorder_GetCount(), 2, OLED_6X8);

    /* Bottom bar */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(30, 56, "ENTER", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);

    OLED_Update();
}

/*============================ PAGE 4: Debug ============================*/
#ifdef FEATURE_DEBUG_PAGE

static void Display_PageDebug(void)
{
    uint16_t bat_adc, ecg_adc;

    if (!display_refresh_flag)
        return;
    display_refresh_flag = 0;

    OLED_Clear();
    OLED_ShowString(0, 0, "[Debug]", OLED_6X8);
    OLED_ShowNum(92, 0, seconds_counter, 5, OLED_6X8);

    /* 电池: ADC原始值 + 还原电压 + 百分比 */
    bat_adc = ADC_ReadChannel(ADC_CHANNEL_4);
    OLED_ShowString(0, 10, "BAT:", OLED_6X8);
    OLED_ShowNum(24, 10, bat_adc, 4, OLED_6X8);
    OLED_ShowNum(56, 10, battery_mv, 4, OLED_6X8);
    OLED_ShowString(80, 10, "mV", OLED_6X8);
    OLED_ShowNum(98, 10, battery_percent, 3, OLED_6X8);
    OLED_ShowChar(116, 10, '%', OLED_6X8);

    /* ECG ADC原始值 + 导联状态 */
    ecg_adc = ADC_ReadChannel(ADC_CHANNEL_5);
    OLED_ShowString(0, 20, "ECG:", OLED_6X8);
    OLED_ShowNum(24, 20, ecg_adc, 4, OLED_6X8);
    OLED_ShowString(56, 20, "Lead:", OLED_6X8);
    OLED_ShowString(86, 20, (char *)(AD8232_GetConnect() ? "OK " : "OFF"), OLED_6X8);

    /* 按键实时状态 (1=按下 0=松开) */
    OLED_ShowString(0, 30, "K1:", OLED_6X8);
    OLED_ShowNum(18, 30, Key_ReadRaw(KEY1), 1, OLED_6X8);
    OLED_ShowString(32, 30, "K2:", OLED_6X8);
    OLED_ShowNum(50, 30, Key_ReadRaw(KEY2), 1, OLED_6X8);
    OLED_ShowString(64, 30, "K3:", OLED_6X8);
    OLED_ShowNum(82, 30, Key_ReadRaw(KEY3), 1, OLED_6X8);

    /* LED + Beep 状态 */
    OLED_ShowString(0, 40, "LED:", OLED_6X8);
    OLED_ShowString(24, 40, (char *)(LED_GetState() ? "ON " : "OFF"), OLED_6X8);
    OLED_ShowString(56, 40, "Beep:", OLED_6X8);
    OLED_ShowString(86, 40, (char *)(Beep_GetState() ? "ON " : "OFF"), OLED_6X8);

    /* 录制状态 + 记录数 */
    OLED_ShowString(0, 50, "REC:", OLED_6X8);
    OLED_ShowString(24, 50, (char *)(recorder_recording ? "RUN" : "---"), OLED_6X8);
    OLED_ShowString(56, 50, "N:", OLED_6X8);
    OLED_ShowNum(68, 50, Recorder_GetCount(), 2, OLED_6X8);

    OLED_Update();
}
#endif /* FEATURE_DEBUG_PAGE */
