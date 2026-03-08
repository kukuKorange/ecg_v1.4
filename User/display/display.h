/**
  ******************************************************************************
  * @file    display.h
  * @brief   OLED display module - page system
  *
  * @details Ported from v1.3 display module.
  *          v1.4 pages (no max30102):
  *          - Page 0 (PAGE_ECG):  ECG waveform + heart rate
  *          - Page 1 (PAGE_INFO): ECG info / debug
  *
  *          Display_Update() is the single entry point called from main loop.
  *          It internally handles ECG sampling, drawing and OLED flushing.
  ******************************************************************************
  */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdint.h>

/*============================ Page Definitions ============================*/

#define PAGE_ECG          0
#define PAGE_INFO         1
#define PAGE_MAX          2

/*============================ External Variables ============================*/

extern uint8_t current_page;

/*============================ Functions ============================*/

void Display_Init(void);
void Display_Update(void);

#endif /* __DISPLAY_H */
