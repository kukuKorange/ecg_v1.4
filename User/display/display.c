/**
  ******************************************************************************
  * @file    display.c
  * @brief   OLED display module - page system
  *
  * @details Display_Update() is the single entry point.
  *          All display-related logic is encapsulated here:
  *          - Page transition (clear + state reset)
  *          - ECG sampling & waveform drawing (200Hz flag)
  *          - Static overlay rendering
  *          - OLED framebuffer flush (25Hz flag)
  *          - Info page rendering (5Hz flag)
  ******************************************************************************
  */

#include "display.h"
#include "tim.h"
#include "../oled/OLED.h"
#include "../ad8232/AD8232.h"

/*============================ Global Variables ============================*/

uint8_t current_page = PAGE_ECG;

/*============================ Private Variables ============================*/

static uint8_t last_page = 0xFF;

/*============================ Private Functions ============================*/

static void Display_PageECG_Overlay(void);
static void Display_PageInfo(void);

/*============================ Display Entry ============================*/

void Display_Init(void)
{
    current_page = PAGE_ECG;
    last_page = 0xFF;
}

/**
  * @brief  Main display entry — sole call from main loop
  *
  *         PAGE_ECG flow per iteration:
  *           1. Page transition → OLED_Clear + AD8232_Init + axes
  *           2. Static overlay  → title, HR, time, page indicator
  *           3. ECG sample flag → ECG_SampleAndDraw (200Hz)
  *           4. OLED flush flag → OLED_Update (25Hz)
  *
  *         PAGE_INFO flow:
  *           1. Page transition → OLED_Clear
  *           2. display_refresh_flag → full redraw + OLED_Update (5Hz)
  */
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

        last_page = current_page;
    }

    /* ---- Per-page processing ---- */
    switch (current_page)
    {
        case PAGE_ECG:
            /* 200Hz: ECG sampling + waveform drawing (fast, RAM only) */
            if (ecg_sample_flag)
            {
                ecg_sample_flag = 0;
                if (AD8232_GetConnect())
                {
                    ECG_SampleAndDraw();
                }
            }

            /* 25Hz: redraw overlay + flush framebuffer to OLED */
            if (oled_update_flag)
            {
                oled_update_flag = 0;
                Display_PageECG_Overlay();
                OLED_Update();
            }
            break;

        case PAGE_INFO:
            if (display_refresh_flag)
            {
                display_refresh_flag = 0;
                Display_PageInfo();
            }
            break;

        default:
            current_page = PAGE_ECG;
            break;
    }
}

/*============================ Page 0: ECG Waveform ============================*/

/**
  * @brief  ECG page static overlay — title, axes, HR, time, page indicator
  *         Called every main-loop iteration; lightweight (RAM buffer writes only)
  */
static void Display_PageECG_Overlay(void)
{
    uint8_t ecg_hr;

    OLED_ShowString(0, 0, "ECG Monitor", OLED_6X8);

    OLED_DrawLine(1, 54, 120, 54);
    OLED_DrawLine(1, 10, 1, 54);
    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);

    OLED_ShowNum(70, 0, seconds_counter, 4, OLED_6X8);
    OLED_ShowString(94, 0, "s", OLED_6X8);

    ecg_hr = ECG_GetHeartRate();
    OLED_ShowNum(104, 0, ecg_hr, 3, OLED_6X8);

    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "1/2", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
}

/*============================ Page 1: Info ============================*/

/**
  * @brief  Info page — full redraw at 5Hz, self-contained OLED_Update
  */
static void Display_PageInfo(void)
{
    uint16_t adc_raw;
    uint8_t  connected;
    uint8_t  hr;

    OLED_Clear();

    OLED_ShowString(0, 0, "[ECG Info] 5Hz", OLED_6X8);
    OLED_DrawLine(0, 10, 127, 10);

    adc_raw = AD8232_ReadADC();
    OLED_ShowString(0, 14, "ADC:", OLED_6X8);
    OLED_ShowNum(30, 14, adc_raw, 4, OLED_6X8);

    hr = ECG_GetHeartRate();
    OLED_ShowString(0, 24, "HR:", OLED_6X8);
    OLED_ShowNum(24, 24, hr, 3, OLED_6X8);
    OLED_ShowString(48, 24, "bpm", OLED_6X8);

    connected = AD8232_GetConnect();
    OLED_ShowString(0, 34, "Lead:", OLED_6X8);
    OLED_ShowString(36, 34, connected ? "OK" : "OFF", OLED_6X8);

    OLED_ShowString(0, 44, "Time:", OLED_6X8);
    OLED_ShowNum(36, 44, seconds_counter, 5, OLED_6X8);
    OLED_ShowString(72, 44, "s", OLED_6X8);

    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "2/2", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);

    OLED_Update();
}
