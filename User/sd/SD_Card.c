#include "SD_Card.h"
#include "../../User/oled/OLED.h"

extern SPI_HandleTypeDef hspi1;

/* 记录卡类型 */
uint8_t SD_CardType = SD_TYPE_ERR;

/* 调试变量 */
uint8_t SD_Debug_CMD55_Res = 0xFF;
uint8_t SD_Debug_CMD41_Res = 0xFF;
uint16_t SD_Debug_Retry = 0;

/*========== SPI 底层 ==========*/

static uint8_t SD_SPI_ReadWriteByte(uint8_t byte)
{
    uint8_t d_read = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &byte, &d_read, 1, 200);
    return d_read;
}

static void SD_SPI_SetSpeed(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR_Msk, prescaler);
    __HAL_SPI_ENABLE(&hspi1);
}

/*========== SD 卡协议层 ==========*/

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t res;
    uint8_t retry = 200;

    SD_CS_HIGH();
    SD_SPI_ReadWriteByte(0xFF);
    SD_CS_LOW();
    SD_SPI_ReadWriteByte(0xFF);

    SD_SPI_ReadWriteByte(cmd | 0x40);
    SD_SPI_ReadWriteByte((uint8_t)(arg >> 24));
    SD_SPI_ReadWriteByte((uint8_t)(arg >> 16));
    SD_SPI_ReadWriteByte((uint8_t)(arg >> 8));
    SD_SPI_ReadWriteByte((uint8_t)(arg));
    SD_SPI_ReadWriteByte(crc);

    if (cmd == CMD12) SD_SPI_ReadWriteByte(0xFF);

    do {
        res = SD_SPI_ReadWriteByte(0xFF);
    } while ((res & 0x80) && --retry);

    return res;
}

/* 发送80+空时钟 + CMD0, 返回R1 */
static uint8_t SD_GoIdleState(void)
{
    uint8_t res;
    uint16_t i, retry;

    SD_CS_HIGH();
    for (i = 0; i < 20; i++)
        SD_SPI_ReadWriteByte(0xFF);

    retry = 0;
    do {
        res = SD_SendCmd(CMD0, 0, 0x95);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
        if (++retry > 100) return 0xFF;
    } while (res != 0x01);

    return res;
}

/*========== SD 卡初始化 - 多策略尝试 ==========*/

uint8_t SD_Init(void)
{
    uint8_t res;
    uint16_t retry;
    uint8_t buf[4];
    uint16_t i;
    uint8_t strategy = 0;  /* 记录成功的策略编号 */

    SD_CardType = SD_TYPE_ERR;
    SD_Debug_CMD55_Res = 0xFF;
    SD_Debug_CMD41_Res = 0xFF;
    SD_Debug_Retry = 0;

    HAL_Delay(50);

    /* SPI低速 ~390kHz */
    SD_SPI_SetSpeed(SPI_BAUDRATEPRESCALER_256);

    /*==========================================================
     * 策略1: 标准V2流程  CMD0 → CMD8 → ACMD41(HCS=1)
     *==========================================================*/
    OLED_ClearArea(0, 16, 128, 48);
    OLED_ShowString(0, 16, "Try1: CMD8+ACMD41", OLED_6X8);
    OLED_Update();

    res = SD_GoIdleState();
    if (res != 0x01) return 1;

    res = SD_SendCmd(CMD8, 0x000001AA, 0x87);
    if (res == 0x01)
    {
        for (i = 0; i < 4; i++)
            buf[i] = SD_SPI_ReadWriteByte(0xFF);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);

        if (buf[2] == 0x01 && buf[3] == 0xAA)
        {
            /* ACMD41(HCS=1, VDD=2.7~3.6V) */
            retry = 0;
            do {
                SD_Debug_CMD55_Res = SD_SendCmd(CMD55, 0, 0x65);
                SD_CS_HIGH();
                SD_SPI_ReadWriteByte(0xFF);

                SD_Debug_CMD41_Res = SD_SendCmd(CMD41, 0x40FF8000, 0x01);
                SD_CS_HIGH();
                SD_SPI_ReadWriteByte(0xFF);

                res = SD_Debug_CMD41_Res;
                retry++;
                SD_Debug_Retry = retry;
            } while (res != 0x00 && retry < 300);

            if (res == 0x00) { strategy = 1; goto init_ok_v2; }
        }
    }
    else
    {
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
    }

    /*==========================================================
     * 策略2: 不发CMD8, 直接 CMD0 → ACMD41(无HCS)
     *         用于V1卡或不支持CMD8的卡
     *==========================================================*/
    OLED_ClearArea(0, 16, 128, 48);
    OLED_ShowString(0, 16, "Try2: ACMD41 only", OLED_6X8);
    OLED_Update();

    res = SD_GoIdleState();
    if (res != 0x01) return 1;

    retry = 0;
    do {
        SD_Debug_CMD55_Res = SD_SendCmd(CMD55, 0, 0x65);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);

        SD_Debug_CMD41_Res = SD_SendCmd(CMD41, 0, 0x01);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);

        res = SD_Debug_CMD41_Res;
        retry++;
        SD_Debug_Retry = retry;
    } while (res != 0x00 && retry < 300);

    if (res == 0x00) { strategy = 2; SD_CardType = SD_TYPE_V1; goto init_ok_v1; }

    /*==========================================================
     * 策略3: CMD0 → CMD1 (MMC兼容方式)
     *==========================================================*/
    OLED_ClearArea(0, 16, 128, 48);
    OLED_ShowString(0, 16, "Try3: CMD1(MMC)", OLED_6X8);
    OLED_Update();

    res = SD_GoIdleState();
    if (res != 0x01) return 1;

    retry = 0;
    do {
        res = SD_SendCmd(CMD1, 0, 0x01);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
        retry++;
        SD_Debug_Retry = retry;
    } while (res != 0x00 && retry < 300);

    if (res == 0x00) { strategy = 3; SD_CardType = SD_TYPE_MMC; goto init_ok_v1; }

    /*==========================================================
     * 策略4: CMD0 → CMD8 → ACMD41(只有HCS, 无电压窗口)
     *         某些卡不接受带电压窗口的ACMD41
     *==========================================================*/
    OLED_ClearArea(0, 16, 128, 48);
    OLED_ShowString(0, 16, "Try4: ACMD41 0x4000", OLED_6X8);
    OLED_Update();

    res = SD_GoIdleState();
    if (res != 0x01) return 1;

    res = SD_SendCmd(CMD8, 0x000001AA, 0x87);
    if (res == 0x01)
    {
        for (i = 0; i < 4; i++)
            buf[i] = SD_SPI_ReadWriteByte(0xFF);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
    }
    else
    {
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
    }

    retry = 0;
    do {
        SD_Debug_CMD55_Res = SD_SendCmd(CMD55, 0, 0x65);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);

        SD_Debug_CMD41_Res = SD_SendCmd(CMD41, 0x40000000, 0x01);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);

        res = SD_Debug_CMD41_Res;
        retry++;
        SD_Debug_Retry = retry;
    } while (res != 0x00 && retry < 300);

    if (res == 0x00) { strategy = 4; goto init_ok_v2; }

    /* 全部失败 */
    return 3;

    /*==========================================================
     * V2 卡初始化成功后续: CMD58 读OCR判断SDHC
     *==========================================================*/
init_ok_v2:
    res = SD_SendCmd(CMD58, 0, 0x01);
    if (res == 0x00)
    {
        for (i = 0; i < 4; i++)
            buf[i] = SD_SPI_ReadWriteByte(0xFF);
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
        if (buf[0] & 0x40)
            SD_CardType = SD_TYPE_V2HC;
        else
            SD_CardType = SD_TYPE_V2;
    }
    else
    {
        SD_CS_HIGH();
        SD_SPI_ReadWriteByte(0xFF);
        SD_CardType = SD_TYPE_V2;
    }
    goto init_done;

    /*==========================================================
     * V1/MMC 卡初始化成功后续: CMD16设置块大小
     *==========================================================*/
init_ok_v1:
    res = SD_SendCmd(CMD16, 512, 0x01);
    SD_CS_HIGH();
    SD_SPI_ReadWriteByte(0xFF);
    goto init_done;

init_done:
    SD_SPI_SetSpeed(SPI_BAUDRATEPRESCALER_4);

    /* 在OLED上显示成功策略 */
    OLED_ClearArea(0, 24, 128, 16);
    OLED_ShowString(0, 26, "OK! S:", OLED_6X8);
    OLED_ShowNum(36, 26, strategy, 1, OLED_6X8);
    OLED_ShowString(48, 26, "R:", OLED_6X8);
    OLED_ShowNum(60, 26, SD_Debug_Retry, 4, OLED_6X8);
    OLED_Update();

    return 0;
}

/*========== 读取单个扇区 ==========*/

uint8_t SD_ReadBlock(uint32_t sector, uint8_t *buffer)
{
    uint8_t res;
    uint16_t i;
    uint32_t retry;

    if (SD_CardType != SD_TYPE_V2HC) sector <<= 9;

    res = SD_SendCmd(CMD17, sector, 0x01);
    if (res != 0x00) { SD_CS_HIGH(); SD_SPI_ReadWriteByte(0xFF); return 1; }

    retry = 0;
    do {
        res = SD_SPI_ReadWriteByte(0xFF);
        if (++retry > 0xFFFFF) { SD_CS_HIGH(); SD_SPI_ReadWriteByte(0xFF); return 2; }
    } while (res != 0xFE);

    for (i = 0; i < 512; i++)
        buffer[i] = SD_SPI_ReadWriteByte(0xFF);

    SD_SPI_ReadWriteByte(0xFF);
    SD_SPI_ReadWriteByte(0xFF);
    SD_CS_HIGH();
    SD_SPI_ReadWriteByte(0xFF);
    return 0;
}

/*========== 写入单个扇区 ==========*/

uint8_t SD_WriteBlock(uint32_t sector, const uint8_t *buffer)
{
    uint8_t res;
    uint16_t i;
    uint32_t retry;

    if (SD_CardType != SD_TYPE_V2HC) sector <<= 9;

    res = SD_SendCmd(CMD24, sector, 0x01);
    if (res != 0x00) { SD_CS_HIGH(); SD_SPI_ReadWriteByte(0xFF); return 1; }

    SD_SPI_ReadWriteByte(0xFF);
    SD_SPI_ReadWriteByte(0xFF);
    SD_SPI_ReadWriteByte(0xFE);

    for (i = 0; i < 512; i++)
        SD_SPI_ReadWriteByte(buffer[i]);

    SD_SPI_ReadWriteByte(0xFF);
    SD_SPI_ReadWriteByte(0xFF);

    res = SD_SPI_ReadWriteByte(0xFF);
    if ((res & 0x1F) != 0x05) { SD_CS_HIGH(); SD_SPI_ReadWriteByte(0xFF); return 2; }

    retry = 0;
    while (SD_SPI_ReadWriteByte(0xFF) == 0x00)
    {
        if (++retry > 0xFFFFFF) { SD_CS_HIGH(); SD_SPI_ReadWriteByte(0xFF); return 3; }
    }

    SD_CS_HIGH();
    SD_SPI_ReadWriteByte(0xFF);
    return 0;
}
