/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "../../User/oled/OLED.h"
#include "../../User/sd/SD_Card.h"
#include "../../User/led/led.h"
#include "../../User/beep/beep.h"
#include "../../User/key/key.h"
#include "../../User/ad8232/AD8232.h"
#include "../../User/display/display.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_ShowString(0, 0, "ECG v1.4", OLED_8X16);
  OLED_ShowString(0, 16, "SD Init...", OLED_6X8);
  OLED_Update();
  
  uint8_t sd_res = SD_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, "ECG v1.4", OLED_8X16);
  if (sd_res == 0)
  {
    OLED_ShowString(0, 20, "SD OK!", OLED_8X16);
    if (SD_CardType == SD_TYPE_V2HC)
      OLED_ShowString(0, 40, "Type: SDHC(V2)", OLED_6X8);
    else if (SD_CardType == SD_TYPE_V2)
      OLED_ShowString(0, 40, "Type: SD V2", OLED_6X8);
    else if (SD_CardType == SD_TYPE_V1)
      OLED_ShowString(0, 40, "Type: SD V1", OLED_6X8);
    else if (SD_CardType == SD_TYPE_MMC)
      OLED_ShowString(0, 40, "Type: MMC", OLED_6X8);
    OLED_ShowString(0, 50, "Retry:", OLED_6X8);
    OLED_ShowNum(36, 50, SD_Debug_Retry, 5, OLED_6X8);
  }
  else
  {
    OLED_ShowString(0, 20, "SD FAIL!", OLED_8X16);
    OLED_ShowString(0, 38, "Err:", OLED_6X8);
    OLED_ShowNum(24, 38, sd_res, 1, OLED_6X8);
    OLED_ShowString(0, 48, "C55:", OLED_6X8);
    OLED_ShowHexNum(24, 48, SD_Debug_CMD55_Res, 2, OLED_6X8);
    OLED_ShowString(60, 48, "C41:", OLED_6X8);
    OLED_ShowHexNum(84, 48, SD_Debug_CMD41_Res, 2, OLED_6X8);
    OLED_ShowString(0, 56, "Retry:", OLED_6X8);
    OLED_ShowNum(36, 56, SD_Debug_Retry, 5, OLED_6X8);
  }
  OLED_Update();
  HAL_Delay(2000);
  
  /* Initialize AD8232 + display + timer */
  AD8232_Init();
  Display_Init();
  MX_TIM2_Init();
  TIM2_Start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 1. Key scan + page navigation (v1.3 style) */
    Key_Scan();
    Key_Process();

    /* 2. Display update (page transition + ECG sampling + OLED flush) */
    Display_Update();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  * HSI = 16MHz → PLL → SYSCLK = 100MHz
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;      /* VCO input  = 16MHz / 8  = 2MHz   */
  RCC_OscInitStruct.PLL.PLLN = 100;    /* VCO output = 2MHz * 100 = 200MHz */
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;  /* SYSCLK = 200MHz / 2 = 100MHz */
  RCC_OscInitStruct.PLL.PLLQ = 4;      /* USB/SDIO   = 200MHz / 4 = 50MHz  */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  * SYSCLK=100MHz, AHB=100MHz, APB1=50MHz(max), APB2=100MHz
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* APB1 = 100/2 = 50MHz (max 50) */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   /* APB2 = 100MHz */

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
