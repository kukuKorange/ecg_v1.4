/**
  ******************************************************************************
  * @file    battery.h
  * @brief   Battery voltage monitoring via PA4 (ADC1_CH4)
  *
  * @details Voltage divider: 30K (top) + 10K (bottom)
  *          Vbat = Vadc * (30+10)/10 = Vadc * 4
  ******************************************************************************
  */

#ifndef __BATTERY_H
#define __BATTERY_H

#include "main.h"

/*============================ Configuration ============================*/

#define BATTERY_DIVIDER_RATIO   3       /* (20K+10K) / 10K */
#define BATTERY_FULL_MV         5000
#define BATTERY_LOW_MV          4000
#define BATTERY_EMPTY_MV        3500
#define BATTERY_ADC_VREF_MV     3300

/*============================ External Variables ============================*/

extern uint8_t  battery_percent;
extern uint16_t battery_mv;
extern uint8_t  battery_low_flag;

/*============================ API ============================*/

void Battery_Init(void);
void Battery_Update(void);

#endif /* __BATTERY_H */
