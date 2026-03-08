/**
  ******************************************************************************
  * @file    display.h
  * @brief   OLED display module — 4-page system
  *
  * @details Pages:
  *          0  PAGE_ECG      — ECG waveform + HR + time + battery + lead status
  *          1  PAGE_HISTORY  — Recording history list / playback
  *          2  PAGE_USER     — User info (name + age) editing
  *          3  PAGE_SETTINGS — Monitoring mode + timed duration
  ******************************************************************************
  */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdint.h>

/*============================ Page Definitions ============================*/

#define PAGE_ECG          0
#define PAGE_HISTORY      1
#define PAGE_USER         2
#define PAGE_SETTINGS     3
#define PAGE_MAX          4

/*============================ History Sub-states ============================*/

#define HIST_VIEW_LIST      0
#define HIST_VIEW_PLAYBACK  1

/*============================ User-edit Sub-states ============================*/

#define UEDIT_VIEW          0       /* browse mode, K1/K3 switch pages */
#define UEDIT_FIELD_NAME    1       /* editing name character */
#define UEDIT_FIELD_AGE     2       /* editing age value */

/*============================ Settings Items ============================*/

#define SETTING_MODE        0
#define SETTING_DURATION    1
#define SETTING_DELETE      2
#define SETTING_ITEM_MAX    3

/*============================ External Variables ============================*/

extern uint8_t current_page;

/* History page state (accessed by key.c) */
extern uint8_t hist_view;
extern uint8_t hist_cursor;
extern uint32_t hist_play_offset;

/* User-edit page state */
extern uint8_t uedit_field;
extern uint8_t uedit_name_pos;
extern char    uedit_name_buf[9];
extern uint8_t uedit_age_val;
extern uint8_t uedit_save_tip;      /* >0: show "Save Succeed" countdown */

/* Settings page state */
extern uint8_t setting_cursor;
extern uint8_t setting_mode;           /* MODE_CONTINUOUS / MODE_TIMED */
extern uint16_t setting_timed_dur;     /* seconds: 30, 60, 300 */

/*============================ Functions ============================*/

void Display_Init(void);
void Display_Update(void);

#endif /* __DISPLAY_H */
