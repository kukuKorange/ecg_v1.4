#ifndef __SD_CARD_H
#define __SD_CARD_H

#include "main.h"
#include "spi.h"

/* SD卡类型 */
#define SD_TYPE_ERR     0x00
#define SD_TYPE_MMC     0x01
#define SD_TYPE_V1      0x02
#define SD_TYPE_V2      0x04
#define SD_TYPE_V2HC    0x06   /* SDHC: V2 + HC标志 */

/* SD卡指令定义 */
#define CMD0    0       /* 复位 */
#define CMD1    1       /* MMC初始化 */
#define CMD8    8       /* 检查电压范围 (SD V2.0) */
#define CMD9    9       /* 读取CSD */
#define CMD10   10      /* 读取CID */
#define CMD12   12      /* 停止读取 */
#define CMD16   16      /* 设置块大小 */
#define CMD17   17      /* 读取单块 */
#define CMD18   18      /* 读取多块 */
#define CMD23   23      /* 设置多块擦除数 */
#define CMD24   24      /* 写入单块 */
#define CMD25   25      /* 写入多块 */
#define CMD41   41      /* ACMD41: SD初始化 */
#define CMD55   55      /* 启动ACMD前缀 */
#define CMD58   58      /* 读取OCR */

/* 引脚宏定义 - PB5 为 CS */
#define SD_CS_LOW()     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET)

/* 全局变量: 卡类型 */
extern uint8_t SD_CardType;

/* 调试变量: 记录最后一次CMD55和CMD41的应答 */
extern uint8_t SD_Debug_CMD55_Res;
extern uint8_t SD_Debug_CMD41_Res;
extern uint16_t SD_Debug_Retry;

/* 函数声明 */
uint8_t SD_Init(void);
uint8_t SD_ReadBlock(uint32_t sector, uint8_t *buffer);
uint8_t SD_WriteBlock(uint32_t sector, const uint8_t *buffer);

#endif
