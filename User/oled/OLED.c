/***************************************************************************************
  * 本程序由江协科技创建并免费开源共享
  * 你可以任意查看、使用和修改，并应用到自己的项目之中
  * 程序版权归江协科技所有，任何人或组织不得将其据为己有
  * 
  * 程序名称：				0.96寸OLED显示屏驱动程序（4针脚I2C接口）
  * 程序创建时间：			2023.10.24
  * 当前程序版本：			V1.1
  * 当前版本发布时间：		2023.12.8
  * 
  * 江协科技官方网站：		jiangxiekeji.com
  * 江协科技官方淘宝店：	jiangxiekeji.taobao.com
  * 程序介绍及更新动态：	jiangxiekeji.com/tutorial/oled.html
  * 
  * 如果你发现程序中的漏洞或者笔误，可通过邮件向我们反馈：feedback@jiangxiekeji.com
  * 发送邮件之前，你可以先到更新动态页面查看最新程序，如果此问题已经修改，则无需再发邮件
  ***************************************************************************************
  */

#include "main.h"
#include "OLED.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/**
  * 数据存储格式：
  * 纵向8点，高位在下，先从左到右，再从上到下
  * 每一个Bit对应一个像素点
  * 
  *      B0 B0                  B0 B0
  *      B1 B1                  B1 B1
  *      B2 B2                  B2 B2
  *      B3 B3  ------------->  B3 B3 --
  *      B4 B4                  B4 B4  |
  *      B5 B5                  B5 B5  |
  *      B6 B6                  B6 B6  |
  *      B7 B7                  B7 B7  |
  *                                    |
  *  -----------------------------------
  *  |   
  *  |   B0 B0                  B0 B0
  *  |   B1 B1                  B1 B1
  *  |   B2 B2                  B2 B2
  *  --> B3 B3  ------------->  B3 B3
  *      B4 B4                  B4 B4
  *      B5 B5                  B5 B5
  *      B6 B6                  B6 B6
  *      B7 B7                  B7 B7
  * 
  * 坐标轴定义：
  * 左上角为(0, 0)点
  * 横向向右为X轴，取值范围：0~127
  * 纵向向下为Y轴，取值范围：0~63
  * 
  *       0             X轴           127 
  *      .------------------------------->
  *    0 |
  *      |
  *      |
  *      |
  *  Y轴 |
  *      |
  *      |
  *      |
  *   63 |
  *      v
  * 
  */


/*全局变量*********************/

/**
  * OLED显存数组
  * 所有的显示函数，都只是对此显存数组进行读写
  * 随后调用OLED_Update函数或OLED_UpdateArea函数
  * 才会将显存数组的数据发送到OLED硬件，进行显示
  */
uint8_t OLED_DisplayBuf[8][128];

/*********************全局变量*/


/*引脚配置*********************/

#define OLED_W_SCL(x) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, (GPIO_PinState)(!!(x)))
#define OLED_W_SDA(x) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, (GPIO_PinState)(!!(x)))

/**
  * 函    数：I2C延时
  */
static void OLED_I2C_Delay(void)
{
	volatile uint32_t i = 120;  /* 100MHz主频下约6us, 保持与16MHz时相同的I2C速率 */
	while(i--);
}

/**
  * 函    数：I2C起始
  * 参    数：无
  * 返 回 值：无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_I2C_Delay();
	OLED_W_SDA(0);
	OLED_I2C_Delay();
	OLED_W_SCL(0);
	OLED_I2C_Delay();
}

/**
  * 函    数：I2C终止
  * 参    数：无
  * 返 回 值：无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_I2C_Delay();
	OLED_W_SCL(1);
	OLED_I2C_Delay();
	OLED_W_SDA(1);
	OLED_I2C_Delay();
}

/**
  * 函    数：I2C发送一个字节
  * 参    数：Byte 要发送的一个字节
  * 返 回 值：无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(Byte & (0x80 >> i));
		OLED_I2C_Delay();
		OLED_W_SCL(1);
		OLED_I2C_Delay();
		OLED_W_SCL(0);
		OLED_I2C_Delay();
	}
	OLED_W_SDA(1);	// 释放SDA线，准备接收ACK
	OLED_I2C_Delay();
	OLED_W_SCL(1);	// 额外的一个时钟，用于应答
	OLED_I2C_Delay();
	OLED_W_SCL(0);
	OLED_I2C_Delay();
}

/**
  * 函    数：OLED写命令
  * 参    数：Command 要写入的命令值，范围：0x00~0xFF
  * 返 回 值：无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * 函    数：OLED写数据
  * 参    数：Data 要写入数据的起始地址
  * 参    数：Count 要写入数据的数量
  * 返 回 值：无
  */
void OLED_WriteData(uint8_t *Data, uint16_t Count)
{
	uint16_t i;
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	for (i = 0; i < Count; i++)
	{
		OLED_I2C_SendByte(Data[i]);
	}
	OLED_I2C_Stop();
}

/*********************引脚配置*/


/*硬件配置*********************/

/**
  * 函    数：OLED设置光标位置
  * 参    数：Page 指定光标所在的页，范围：0~7
  * 参    数：X 指定光标所在的X轴坐标，范围：0~127
  * 返 回 值：无
  */
void OLED_SetCursor(uint8_t Page, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Page);					//设置页位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/**
  * 函    数：OLED初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用前，需要调用此初始化函数
  */
void OLED_Init(void)
{
	HAL_Delay(200);			//在初始化前，加入适量延时，待OLED供电稳定
	
	/*写入一系列的命令，对OLED进行初始化配置*/
	OLED_WriteCommand(0xAE);	//设置显示开启/关闭，0xAE关闭，0xAF开启
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);	//0x00~0xFF
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);	//0x0E~0x3F
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);	//0x00~0x7F
	
	OLED_WriteCommand(0x40);	//设置显示开始行，0x40~0x7F
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常，0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常，0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度
	OLED_WriteCommand(0xCF);	//0x00~0xFF

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/反色显示，0xA6正常，0xA7反色

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//设置显示开启/关闭，0xAE关闭，0xAF开启
	
	OLED_Clear();				//清空显存数组
	OLED_Update();				//更新显存数组到OLED
}

/*********************硬件配置*/


/*工具函数*********************/

/**
  * 函    数：次方函数
  * 参    数：X 底数
  * 参    数：Y 指数
  * 返 回 值：等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//结果默认为1
	while (Y --)			//累乘Y次
	{
		Result *= X;		//每次把X累乘到结果上
	}
	return Result;
}

/**
  * 函    数：判断指定点是否在指定角度内部
  * 参    数：X Y 指定点的坐标
  * 参    数：StartAngle EndAngle 起始角度和终止角度，范围：-180~180
  *           水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
  * 返 回 值：指定点是否在指定角度内部，1：在内部，0：不在内部
  */
uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14159 * 180;	//计算指定点的弧度，并转换为角度表示
	if (StartAngle < EndAngle)	//起始角度小于终止角度的情况
	{
		/*如果指定角度在起始终止角度之间，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle && PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	else			//起始角度大于于终止角度的情况
	{
		/*如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle || PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	return 0;		//不满足以上条件，则判断判定指定点不在指定角度
}

/*********************工具函数*/


/*功能函数*********************/

/**
  * 函    数：将OLED显存数组更新到OLED屏幕
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Update(void)
{
	uint8_t j;
	/*遍历每一页*/
	for (j = 0; j < 8; j ++)
	{
		/*设置光标位置为每一页的第一列*/
		OLED_SetCursor(j, 0);
		/*连续写入128个数据，将显存数组的数据写入到OLED硬件*/
		OLED_WriteData(OLED_DisplayBuf[j], 128);
	}
}

/**
  * 函    数：将OLED显存数组部分更新到OLED屏幕
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  */
void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t j;
	
	/*参数检查，保证指定区域不会超出屏幕范围*/
	if (X > 127) {return;}
	if (Y > 63) {return;}
	if (X + Width > 128) {Width = 128 - X;}
	if (Y + Height > 64) {Height = 64 - Y;}
	
	/*遍历指定区域涉及的相关页*/
	for (j = Y / 8; j < (Y + Height - 1) / 8 + 1; j ++)
	{
		/*设置光标位置为相关页的指定列*/
		OLED_SetCursor(j, X);
		/*连续写入Width个数据，将显存数组的数据写入到OLED硬件*/
		OLED_WriteData(&OLED_DisplayBuf[j][X], Width);
	}
}

/**
  * 函    数：将OLED显存数组全部清零
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)
	{
		for (i = 0; i < 128; i ++)
		{
			OLED_DisplayBuf[j][i] = 0x00;
		}
	}
}

/**
  * 函    数：将OLED显存数组部分清零
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  */
void OLED_ClearArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t i, j;
	
	if (X > 127) {return;}
	if (Y > 63) {return;}
	if (X + Width > 128) {Width = 128 - X;}
	if (Y + Height > 64) {Height = 64 - Y;}
	
	for (j = Y; j < Y + Height; j ++)
	{
		for (i = X; i < X + Width; i ++)
		{
			OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
		}
	}
}

/**
  * 函    数：将OLED显存数组全部取反
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)
	{
		for (i = 0; i < 128; i ++)
		{
			OLED_DisplayBuf[j][i] ^= 0xFF;
		}
	}
}
	
/**
  * 函    数：将OLED显存数组部分取反
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  */
void OLED_ReverseArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t i, j;
	
	if (X > 127) {return;}
	if (Y > 63) {return;}
	if (X + Width > 128) {Width = 128 - X;}
	if (Y + Height > 64) {Height = 64 - Y;}
	
	for (j = Y; j < Y + Height; j ++)
	{
		for (i = X; i < X + Width; i ++)
		{
			OLED_DisplayBuf[j / 8][i] ^= 0x01 << (j % 8);
		}
	}
}

/**
  * 函    数：OLED显示一个字符
  * 参    数：X 指定字符左上角的横坐标，范围：0~127
  * 参    数：Y 指定字符左上角的纵坐标，范围：0~63
  * 参    数：Char 指定要显示的字符，范围：ASCII码可见字符
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowChar(uint8_t X, uint8_t Y, char Char, uint8_t FontSize)
{
	if (FontSize == OLED_8X16)
	{
		OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[Char - ' ']);
	}
	else if(FontSize == OLED_6X8)
	{
		OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[Char - ' ']);
	}
}

/**
  * 函    数：OLED显示字符串
  * 参    数：X 指定字符串左上角的横坐标，范围：0~127
  * 参    数：Y 指定字符串左上角的纵坐标，范围：0~63
  * 参    数：String 指定要显示的字符串，范围：ASCII码可见字符组成的字符串
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowString(uint8_t X, uint8_t Y, char *String, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(X + i * FontSize, Y, String[i], FontSize);
	}
}

/**
  * 函    数：OLED显示数字（十进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 指定要显示的数字，范围：0~4294967295
  * 参    数：Length 指定数字的长度，范围：0~10
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
	}
}

/**
  * 函    数：OLED显示有符号数字（十进制，整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 指定要显示的数字，范围：-2147483648~2147483647
  * 参    数：Length 指定数字的长度，范围：0~10
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowSignedNum(uint8_t X, uint8_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	uint32_t Number1;
	
	if (Number >= 0)
	{
		OLED_ShowChar(X, Y, '+', FontSize);
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(X, Y, '-', FontSize);
		Number1 = -Number;
	}
	
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(X + (i + 1) * FontSize, Y, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
	}
}

/**
  * 函    数：OLED显示十六进制数字（十六进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
  * 参    数：Length 指定数字的长度，范围：0~8
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowHexNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		
		if (SingleNumber < 10)
		{
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontSize);
		}
		else
		{
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontSize);
		}
	}
}

/**
  * 函    数：OLED显示二进制数字（二进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
  * 参    数：Length 指定数字的长度，范围：0~16
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowBinNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(2, Length - i - 1) % 2 + '0', FontSize);
	}
}

/**
  * 函    数：OLED显示浮点数字（十进制，小数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 指定要显示的数字，范围：-4294967295.0~4294967295.0
  * 参    数：IntLength 指定数字的整数位长度，范围：0~10
  * 参    数：FraLength 指定数字的小数位长度，范围：0~9，小数进行四舍五入显示
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  */
void OLED_ShowFloatNum(uint8_t X, uint8_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
	uint32_t PowNum, IntNum, FraNum;
	
	if (Number >= 0)
	{
		OLED_ShowChar(X, Y, '+', FontSize);
	}
	else
	{
		OLED_ShowChar(X, Y, '-', FontSize);
		Number = -Number;
	}
	
	IntNum = Number;
	Number -= IntNum;
	PowNum = OLED_Pow(10, FraLength);
	FraNum = round(Number * PowNum);
	IntNum += FraNum / PowNum;
	
	OLED_ShowNum(X + FontSize, Y, IntNum, IntLength, FontSize);
	OLED_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontSize);
	OLED_ShowNum(X + (IntLength + 2) * FontSize, Y, FraNum % PowNum, FraLength, FontSize);
}

/**
  * 函    数：OLED显示汉字串
  */
void OLED_ShowChinese(uint8_t X, uint8_t Y, char *Chinese)
{
	/* 此函数暂未实现 */
}

/**
  * 函    数：OLED显示图像
  * 参    数：X 指定图像左上角的横坐标，范围：0~127
  * 参    数：Y 指定图像左上角的纵坐标，范围：0~63
  * 参    数：Width 指定图像的宽度，范围：0~128
  * 参    数：Height 指定图像的高度，范围：0~64
  * 参    数：Image 指定要显示的图像
  * 返 回 值：无
  */
void OLED_ShowImage(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
	uint8_t i, j;
	
	if (X > 127) {return;}
	if (Y > 63) {return;}
	
	OLED_ClearArea(X, Y, Width, Height);
	
	for (j = 0; j < (Height - 1) / 8 + 1; j ++)
	{
		for (i = 0; i < Width; i ++)
		{
			if (X + i > 127) {break;}
			if (Y / 8 + j > 7) {return;}
			
			OLED_DisplayBuf[Y / 8 + j][X + i] |= Image[j * Width + i] << (Y % 8);
			
			if (Y / 8 + j + 1 > 7) {continue;}
			
			OLED_DisplayBuf[Y / 8 + j + 1][X + i] |= Image[j * Width + i] >> (8 - Y % 8);
		}
	}
}

/**
  * 函    数：OLED使用printf函数打印格式化字符串
  */
void OLED_Printf(uint8_t X, uint8_t Y, uint8_t FontSize, char *format, ...)
{
	char String[30];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	OLED_ShowString(X, Y, String, FontSize);
}

/**
  * 函    数：OLED在指定位置画一个点
  */
void OLED_DrawPoint(uint8_t X, uint8_t Y)
{
	if (X > 127) {return;}
	if (Y > 63) {return;}
	
	OLED_DisplayBuf[Y / 8][X] |= 0x01 << (Y % 8);
}

/**
  * 函    数：OLED获取指定位置点的值
  */
uint8_t OLED_GetPoint(uint8_t X, uint8_t Y)
{
	if (X > 127) {return 0;}
	if (Y > 63) {return 0;}
	
	if (OLED_DisplayBuf[Y / 8][X] & 0x01 << (Y % 8))
	{
		return 1;
	}
	
	return 0;
}

/**
  * 函    数：OLED画图线
  */
void OLED_DrawChart(uint16_t Chart[],uint8_t Width)
{
	int i=0,j=0;
	for(i=0;i<118;i++)
	{
		OLED_DrawLine(j+3,Chart[i],j+4+Width,Chart[i+1]);
		j+=2;
	}
}

/**
  * 函    数：OLED图线数组清空
  */
void OLED_DrawChartClean(uint16_t Chart[])
{
	int i=0;
	for(i=0;i<120;i++)
	{
		Chart[i]=0;
	}
}

/**
  * 函    数：OLED画线
  */
void OLED_DrawLine(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1)
{
	int16_t x, y, dx, dy, d, incrE, incrNE, temp;
	int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
	uint8_t yflag = 0, xyflag = 0;
	
	if (y0 == y1)
	{
		if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
		for (x = x0; x <= x1; x ++)
		{
			OLED_DrawPoint(x, y0);
		}
	}
	else if (x0 == x1)
	{
		if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
		for (y = y0; y <= y1; y ++)
		{
			OLED_DrawPoint(x0, y);
		}
	}
	else
	{
		if (x0 > x1)
		{
			temp = x0; x0 = x1; x1 = temp;
			temp = y0; y0 = y1; y1 = temp;
		}
		if (y0 > y1)
		{
			y0 = -y0;
			y1 = -y1;
			yflag = 1;
		}
		if (y1 - y0 > x1 - x0)
		{
			temp = x0; x0 = y0; y0 = temp;
			temp = x1; x1 = y1; y1 = temp;
			xyflag = 1;
		}
		
		dx = x1 - x0;
		dy = y1 - y0;
		incrE = 2 * dy;
		incrNE = 2 * (dy - dx);
		d = 2 * dy - dx;
		x = x0;
		y = y0;
		
		if (yflag && xyflag){OLED_DrawPoint(y, -x);}
		else if (yflag)		{OLED_DrawPoint(x, -y);}
		else if (xyflag)	{OLED_DrawPoint(y, x);}
		else				{OLED_DrawPoint(x, y);}
		
		while (x < x1)
		{
			if (d <= 0)
			{
				d += incrE;
				x ++;
			}
			else
			{
				d += incrNE;
				x ++;
				y ++;
			}
			
			if (yflag && xyflag){OLED_DrawPoint(y, -x);}
			else if (yflag)		{OLED_DrawPoint(x, -y);}
			else if (xyflag)	{OLED_DrawPoint(y, x);}
			else				{OLED_DrawPoint(x, y);}
		}	
	}
}

/**
  * 函    数：OLED画矩形
  */
void OLED_DrawRectangle(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	uint8_t i, j;
	if (!IsFilled)
	{
		OLED_DrawLine(X, Y, X + Width - 1, Y);
		OLED_DrawLine(X, Y, X, Y + Height - 1);
		OLED_DrawLine(X, Y + Height - 1, X + Width - 1, Y + Height - 1);
		OLED_DrawLine(X + Width - 1, Y, X + Width - 1, Y + Height - 1);
	}
	else
	{
		for (i = X; i < X + Width; i ++)
		{
			for (j = Y; j < Y + Height; j ++)
			{
				OLED_DrawPoint(i, j);
			}
		}
	}
}

/**
  * 函    数：OLED画三角形
  */
void OLED_DrawTriangle(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1, uint8_t X2, uint8_t Y2, uint8_t IsFilled)
{
	if (!IsFilled)
	{
		OLED_DrawLine(X0, Y0, X1, Y1);
		OLED_DrawLine(X0, Y0, X2, Y2);
		OLED_DrawLine(X1, Y1, X2, Y2);
	}
	else
	{
		/* 简单实现填充：寻找外接矩形并判断点是否在三角形内 */
		uint8_t minx = X0, maxx = X0, miny = Y0, maxy = Y0;
		if (X1 < minx) minx = X1; if (X1 > maxx) maxx = X1;
		if (X2 < minx) minx = X2; if (X2 > maxx) maxx = X2;
		if (Y1 < miny) miny = Y1; if (Y1 > maxy) maxy = Y1;
		if (Y2 < miny) miny = Y2; if (Y2 > maxy) maxy = Y2;
		
		for (uint8_t i = minx; i <= maxx; i++)
		{
			for (uint8_t j = miny; j <= maxy; j++)
			{
				/* 重心坐标法判断 */
				if (((X1 - X0) * (j - Y0) - (Y1 - Y0) * (i - X0) >= 0 &&
					 (X2 - X1) * (j - Y1) - (Y2 - Y1) * (i - X1) >= 0 &&
					 (X0 - X2) * (j - Y2) - (Y0 - Y2) * (i - X2) >= 0) ||
					((X1 - X0) * (j - Y0) - (Y1 - Y0) * (i - X0) <= 0 &&
					 (X2 - X1) * (j - Y1) - (Y2 - Y1) * (i - X1) <= 0 &&
					 (X0 - X2) * (j - Y2) - (Y0 - Y2) * (i - X2) <= 0))
				{
					OLED_DrawPoint(i, j);
				}
			}
		}
	}
}

/**
  * 函    数：OLED画圆
  */
void OLED_DrawCircle(uint8_t X, uint8_t Y, uint8_t Radius, uint8_t IsFilled)
{
	int16_t x = 0, y = Radius;
	int16_t d = 1 - Radius;
	
	if (!IsFilled)
	{
		while (y >= x)
		{
			OLED_DrawPoint(X + x, Y + y); OLED_DrawPoint(X + y, Y + x);
			OLED_DrawPoint(X - x, Y + y); OLED_DrawPoint(X - y, Y + x);
			OLED_DrawPoint(X + x, Y - y); OLED_DrawPoint(X + y, Y - x);
			OLED_DrawPoint(X - x, Y - y); OLED_DrawPoint(X - y, Y - x);
			if (d < 0) { d += 2 * x + 3; }
			else { d += 2 * (x - y) + 5; y --; }
			x ++;
		}
	}
	else
	{
		while (y >= x)
		{
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - y, Y + x, X + y, Y + x);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			OLED_DrawLine(X - y, Y - x, X + y, Y - x);
			if (d < 0) { d += 2 * x + 3; }
			else { d += 2 * (x - y) + 5; y --; }
			x ++;
		}
	}
}

/**
  * 函    数：OLED画椭圆
  */
void OLED_DrawEllipse(uint8_t X, uint8_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x = 0, y = B;
	int32_t a2 = A * A, b2 = B * B;
	int32_t d1 = b2 + a2 * (-B + 0.25);
	
	if (!IsFilled)
	{
		while (b2 * (x + 1) < a2 * (y - 0.5))
		{
			OLED_DrawPoint(X + x, Y + y); OLED_DrawPoint(X - x, Y + y);
			OLED_DrawPoint(X + x, Y - y); OLED_DrawPoint(X - x, Y - y);
			if (d1 < 0) { d1 += b2 * (2 * x + 3); }
			else { d1 += b2 * (2 * x + 3) + a2 * (-2 * y + 2); y --; }
			x ++;
		}
		int32_t d2 = b2 * (x + 0.5) * (x + 0.5) + a2 * (y - 1) * (y - 1) - a2 * b2;
		while (y > 0)
		{
			OLED_DrawPoint(X + x, Y + y); OLED_DrawPoint(X - x, Y + y);
			OLED_DrawPoint(X + x, Y - y); OLED_DrawPoint(X - x, Y - y);
			if (d2 < 0) { d2 += b2 * (2 * x + 2) + a2 * (-2 * y + 3); x ++; }
			else { d2 += a2 * (-2 * y + 3); }
			y --;
		}
		OLED_DrawPoint(X + x, Y); OLED_DrawPoint(X - x, Y);
			}
	else
	{
		while (b2 * (x + 1) < a2 * (y - 0.5))
		{
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			if (d1 < 0) { d1 += b2 * (2 * x + 3); }
			else { d1 += b2 * (2 * x + 3) + a2 * (-2 * y + 2); y --; }
			x ++;
		}
		int32_t d2 = b2 * (x + 0.5) * (x + 0.5) + a2 * (y - 1) * (y - 1) - a2 * b2;
		while (y > 0)
			{
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			if (d2 < 0) { d2 += b2 * (2 * x + 2) + a2 * (-2 * y + 3); x ++; }
			else { d2 += a2 * (-2 * y + 3); }
			y --;
		}
		OLED_DrawLine(X - x, Y, X + x, Y);
	}
}

/**
  * 函    数：OLED画弧线
  */
void OLED_DrawArc(uint8_t X, uint8_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x = 0, y = Radius;
	int16_t d = 1 - Radius;
	
	while (StartAngle < -180) StartAngle += 360;
	while (StartAngle > 180) StartAngle -= 360;
	while (EndAngle < -180) EndAngle += 360;
	while (EndAngle > 180) EndAngle -= 360;
	
	if (!IsFilled)
	{
		while (y >= x)
		{
			if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) OLED_DrawPoint(X + y, Y + x);
			if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) OLED_DrawPoint(X + x, Y + y);
			if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) OLED_DrawPoint(X - x, Y + y);
			if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) OLED_DrawPoint(X - y, Y + x);
			if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) OLED_DrawPoint(X - y, Y - x);
			if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) OLED_DrawPoint(X - x, Y - y);
			if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) OLED_DrawPoint(X + x, Y - y);
			if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) OLED_DrawPoint(X + y, Y - x);
			
			if (d < 0) { d += 2 * x + 3; }
			else { d += 2 * (x - y) + 5; y --; }
			x ++;
		}
	}
	else
	{
		/* 填充弧线逻辑复杂，暂仅提供边框 */
		while (y >= x)
		{
			if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) OLED_DrawPoint(X + y, Y + x);
			if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) OLED_DrawPoint(X + x, Y + y);
			if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) OLED_DrawPoint(X - x, Y + y);
			if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) OLED_DrawPoint(X - y, Y + x);
			if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) OLED_DrawPoint(X - y, Y - x);
			if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) OLED_DrawPoint(X - x, Y - y);
			if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) OLED_DrawPoint(X + x, Y - y);
			if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) OLED_DrawPoint(X + y, Y - x);
			
			if (d < 0) { d += 2 * x + 3; }
			else { d += 2 * (x - y) + 5; y --; }
			x ++;
		}
	}
}

/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
