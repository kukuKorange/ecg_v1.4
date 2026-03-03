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

#include "i2c.h"
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

/**
  * 函    数：OLED写命令
  * 参    数：Command 要写入的命令值，范围：0x00~0xFF
  * 返 回 值：无
  */
void OLED_WriteCommand(uint8_t Command)
{
	uint8_t SendData[2];
	SendData[0] = 0x00;		//控制字节，给0x00，表示即将写命令
	SendData[1] = Command;	//写入指定的命令
	HAL_I2C_Transmit(&hi2c1, 0x78, SendData, 2, HAL_MAX_DELAY);
}

/**
  * 函    数：OLED写数据
  * 参    数：Data 要写入数据的起始地址
  * 参    数：Count 要写入数据的数量
  * 返 回 值：无
  */
void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
	uint8_t SendData[Count + 1];
	SendData[0] = 0x40;		//控制字节，给0x40，表示即将写数据
	memcpy(&SendData[1], Data, Count);
	HAL_I2C_Transmit(&hi2c1, 0x78, SendData, Count + 1, HAL_MAX_DELAY);
}

/*********************引脚配置*/


/*硬件配置*********************/

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

/**
  * 函    数：OLED设置显示区域
  * 参    数：X 目标区域左上角的横坐标，范围：0~127
  * 参    数：Y 目标区域左上角的纵坐标，范围：0~63
  * 参    数：Width 目标区域的宽度，范围：0~128
  * 参    数：Height 目标区域的高度，范围：0~64
  * 返 回 值：无
  * 说    明：此函数仅供内部调用，用于OLED_Update和OLED_UpdateArea函数
  */
void OLED_SetArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	OLED_WriteCommand(0x21);	//设置列地址
	OLED_WriteCommand(X);		//列起始地址
	OLED_WriteCommand(X + Width - 1);	//列终止地址

	OLED_WriteCommand(0x22);	//设置页地址
	OLED_WriteCommand(Y / 8);	//页起始地址
	OLED_WriteCommand((Y + Height - 1) / 8);	//页终止地址
}

/**
  * 函    数：OLED更新
  * 参    数：无
  * 返 回 值：无
  * 说    明：将显存数组的内容更新到OLED硬件进行显示
  */
void OLED_Update(void)
{
	OLED_SetArea(0, 0, 128, 64);	//设置区域为整个屏幕
	OLED_WriteData((uint8_t *)OLED_DisplayBuf, 8 * 128);	//将显存数组的数据全部发送
}

/**
  * 函    数：OLED更新局部区域
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  * 说    明：将显存数组中指定的区域更新到OLED硬件进行显示
  *           此区域的左上角坐标X，必须为8的倍数
  *           区域的高度Height，必须为8的倍数
  */
void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t i;
	/*设置区域坐标*/
	OLED_SetArea(X, Y, Width, Height);
	
	/*遍历指定的区域*/
	for (i = 0; i < Height / 8; i ++)
	{
		/*将显存数组指定区域的数据发送*/
		OLED_WriteData(&OLED_DisplayBuf[Y / 8 + i][X], Width);
	}
}

/**
  * 函    数：OLED清屏
  * 参    数：无
  * 返 回 值：无
  * 说    明：将显存数组全部清零
  */
void OLED_Clear(void)
{
	memset(OLED_DisplayBuf, 0, 8 * 128);	//显存数组清零
}

/**
  * 函    数：OLED清局部区域
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  * 说    明：将显存数组指定的区域清零
  */
void OLED_ClearArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t i, j;
	for (j = Y; j < Y + Height; j ++)		//遍历纵坐标
	{
		for (i = X; i < X + Width; i ++)	//遍历横坐标
		{
			OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));	//将显存数组指定数据清零
		}
	}
}

/**
  * 函    数：OLED反色
  * 参    数：无
  * 返 回 值：无
  * 说    明：将显存数组全部取反
  */
void OLED_Reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//遍历页
	{
		for (i = 0; i < 128; i ++)			//遍历列
		{
			OLED_DisplayBuf[j][i] ^= 0xFF;	//将显存数组指定数据取反
		}
	}
}

/**
  * 函    数：OLED局部反色
  * 参    数：X 指定区域左上角的横坐标，范围：0~127
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~63
  * 参    数：Width 指定区域的宽度，范围：0~128
  * 参    数：Height 指定区域的高度，范围：0~64
  * 返 回 值：无
  * 说    明：将显存数组指定的区域取反
  */
void OLED_ReverseArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t i, j;
	for (j = Y; j < Y + Height; j ++)		//遍历纵坐标
	{
		for (i = X; i < X + Width; i ++)	//遍历横坐标
		{
			OLED_DisplayBuf[j / 8][i] ^= (0x01 << (j % 8));		//将显存数组指定数据取反
		}
	}
}

/**
  * 函    数：OLED显示字符
  * 参    数：X 指定字符左上角的横坐标，范围：0~127
  * 参    数：Y 指定字符左上角的纵坐标，范围：0~63
  * 参    数：Char 待显示的字符，范围：ASCII可见字符
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowChar(uint8_t X, uint8_t Y, char Char, uint8_t FontSize)
{
	if (FontSize == OLED_8X16)		//字体为8*16
	{
		/*将字体字模复制到显存数组*/
		OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[Char - ' ']);
	}
	else if(FontSize == OLED_6X8)	//字体为6*8
	{
		/*将字体字模复制到显存数组*/
		OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[Char - ' ']);
	}
}

/**
  * 函    数：OLED显示字符串
  * 参    数：X 指定字符串左上角的横坐标，范围：0~127
  * 参    数：Y 指定字符串左上角的纵坐标，范围：0~63
  * 参    数：String 待显示的字符串，范围：ASCII可见字符组成的字符串
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowString(uint8_t X, uint8_t Y, char *String, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)		//遍历字符串
	{
		OLED_ShowChar(X + i * FontSize, Y, String[i], FontSize);		//显示单个字符
	}
}

/**
  * 函    数：OLED次方
  * 返回值：X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;
	}
	return Result;
}

/**
  * 函    数：OLED显示数字（十进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 待显示的数字，范围：0~4294967295
  * 参    数：Length 指定数字的长度，范围：0~10
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)		//遍历数字的每一位
	{
		/*依次显示数字的每一位*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
	}
}

/**
  * 函    数：OLED显示有符号数字（十进制，整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 待显示的数字，范围：-2147483648~2147483647
  * 参    数：Length 指定数字的长度，范围：0~10
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowSignedNum(uint8_t X, uint8_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint32_t Number1;
	if (Number >= 0)					//数字大于等于0
	{
		OLED_ShowChar(X, Y, '+', FontSize);	//显示+号
		Number1 = Number;				//Number1直接赋值为Number
	}
	else								//数字小于0
	{
		OLED_ShowChar(X, Y, '-', FontSize);	//显示-号
		Number1 = -Number;				//Number1赋值为Number的绝对值
	}
	OLED_ShowNum(X + FontSize, Y, Number1, Length, FontSize);	//显示数字
}

/**
  * 函    数：OLED显示十六进制数字（十六进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 待显示的数字，范围：0~4294967295
  * 参    数：Length 指定数字的长度，范围：0~8
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowHexNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i ++)		//遍历数字的每一位
	{
		/*依次取出数字的每一位*/
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)			//数字小于10
		{
			/*显示0~9*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontSize);
		}
		else							//数字大于10
		{
			/*显示A~F*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontSize);
		}
	}
}

/**
  * 函    数：OLED显示二进制数字（二进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 待显示的数字，范围：0~4294967295
  * 参    数：Length 指定数字的长度，范围：0~32
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowBinNum(uint8_t X, uint8_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)		//遍历数字的每一位
	{
		/*依次显示数字的每一位*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(2, Length - i - 1) % 2 + '0', FontSize);
	}
}

/**
  * 函    数：OLED显示浮点数字（十进制，小数）
  * 参    数：X 指定数字左上角的横坐标，范围：0~127
  * 参    数：Y 指定数字左上角的纵坐标，范围：0~63
  * 参    数：Number 待显示的数字，范围：-1.7E308~1.7E308
  * 参    数：IntLength 指定整数部分的长度，范围：0~10
  * 参    数：FraLength 指定小数部分的长度，范围：0~10
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowFloatNum(uint8_t X, uint8_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
	uint32_t PowNum, IntNum, FraNum;
	
	if (Number >= 0)					//数字大于等于0
	{
		OLED_ShowChar(X, Y, '+', FontSize);	//显示+号
	}
	else								//数字小于0
	{
		OLED_ShowChar(X, Y, '-', FontSize);	//显示-号
		Number = -Number;				//Number取绝对值
	}
	
	/*提取整数部分和小数部分*/
	IntNum = Number;
	Number -= IntNum;
	PowNum = OLED_Pow(10, FraLength);
	FraNum = round(Number * PowNum);
	if (FraNum >= PowNum)				//如果四舍五入造成了进位
	{
		IntNum ++;
		FraNum -= PowNum;
	}
	
	/*显示整数部分*/
	OLED_ShowNum(X + FontSize, Y, IntNum, IntLength, FontSize);
	
	/*显示小数点*/
	OLED_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontSize);
	
	/*显示小数部分*/
	OLED_ShowNum(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength, FontSize);
}

/**
  * 函    数：OLED显示汉字
  * 参    数：X 指定汉字左上角的横坐标，范围：0~127
  * 参    数：Y 指定汉字左上角的纵坐标，范围：0~63
  * 参    数：Chinese 待显示的汉字，范围：字库中已有的汉字
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowChinese(uint8_t X, uint8_t Y, char *Chinese)
{
	/* 此函数暂未实现，如需使用汉字，请在OLED_Data.c中添加字库并在此处实现查找逻辑 */
}

/**
  * 函    数：OLED显示图像
  * 参    数：X 指定图像左上角的横坐标，范围：0~127
  * 参    数：Y 指定图像左上角的纵坐标，范围：0~63
  * 参    数：Width 图像的宽度，范围：0~128
  * 参    数：Height 图像的高度，范围：0~64
  * 参    数：Image 待显示的图像
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_ShowImage(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
	uint8_t i, j;
	
	/*参数检查，限制显示区域*/
	if (X > 127) return;
	if (Y > 63) return;
	
	/*遍历图像*/
	for (j = 0; j < (Height + 7) / 8; j ++)		//遍历页
	{
		for (i = 0; i < Width; i ++)			//遍历列
		{
			if (X + i > 127) break;				//超出屏幕横向范围
			
			/*遍历页内的Bit*/
			for (uint8_t b = 0; b < 8; b ++)
			{
				if (Y + j * 8 + b > 63) break;	//超出屏幕纵向范围
				if (j * 8 + b >= Height) break;	//超出图像高度范围
				
				/*判断图像中该点是否为1*/
				if (Image[j * Width + i] & (0x01 << b))
				{
					/*在显存数组中将对应点置1*/
					OLED_DisplayBuf[(Y + j * 8 + b) / 8][X + i] |= (0x01 << ((Y + j * 8 + b) % 8));
				}
				else
				{
					/*在显存数组中将对应点置0*/
					OLED_DisplayBuf[(Y + j * 8 + b) / 8][X + i] &= ~(0x01 << ((Y + j * 8 + b) % 8));
				}
			}
		}
	}
}

/**
  * 函    数：OLED格式化显示
  * 参    数：X 指定字符串左上角的横坐标，范围：0~127
  * 参    数：Y 指定字符串左上角的纵坐标，范围：0~63
  * 参    数：FontSize 指定字符的大小
  *           范围：OLED_8X16/OLED_6X8
  * 参    数：format 格式化字符串
  * 参    数：... 可变参数列表
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_Printf(uint8_t X, uint8_t Y, uint8_t FontSize, char *format, ...)
{
	char String[30];						//暂存字符串的数组
	va_list arg;							//定义可变参数列表
	va_start(arg, format);					//从format开始
	vsprintf(String, format, arg);			//格式化
	va_end(arg);							//结束
	OLED_ShowString(X, Y, String, FontSize);	//显示字符串
}

/**
  * 函    数：OLED画点
  * 参    数：X 指定点的横坐标，范围：0~127
  * 参    数：Y 指定点的纵坐标，范围：0~63
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawPoint(uint8_t X, uint8_t Y)
{
	if (X > 127 || Y > 63)					//参数检查
	{
		return;
	}
	OLED_DisplayBuf[Y / 8][X] |= (0x01 << (Y % 8));		//将显存数组指定数据置1
}

/**
  * 函    数：OLED获取点
  * 参    数：X 指定点的横坐标，范围：0~127
  * 参    数：Y 指定点的纵坐标，范围：0~63
  * 返 回 值：指定点的值，范围：0/1
  */
uint8_t OLED_GetPoint(uint8_t X, uint8_t Y)
{
	if (X > 127 || Y > 63)					//参数检查
	{
		return 0;
	}
	/*读取显存数组指定数据*/
	if (OLED_DisplayBuf[Y / 8][X] & (0x01 << (Y % 8)))
	{
		return 1;
	}
	return 0;
}

/**
  * 函    数：OLED画线
  * 参    数：X0 指定起始点的横坐标，范围：0~127
  * 参    数：Y0 指定起始点的纵坐标，范围：0~63
  * 参    数：X1 指定终止点的横坐标，范围：0~127
  * 参    数：Y1 指定终止点的纵坐标，范围：0~63
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawLine(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1)
{
	int16_t x = X0, y = Y0;
	int16_t dx = X1 - X0, dy = Y1 - Y0;
	int16_t sx = (X1 > X0) ? 1 : -1, sy = (Y1 > Y0) ? 1 : -1;
	int16_t err = dx - dy, e2;
	
	while (1)
	{
		OLED_DrawPoint(x, y);				//画点
		if (x == X1 && y == Y1) break;		//到达终点，跳出循环
		e2 = 2 * err;
		if (e2 > -dy)
		{
			err -= dy;
			x += sx;
		}
		if (e2 < dx)
		{
			err += dx;
			y += sy;
		}
	}
}

/**
  * 函    数：OLED画矩形
  * 参    数：X 指定矩形左上角的横坐标，范围：0~127
  * 参    数：Y 指定矩形左上角的纵坐标，范围：0~63
  * 参    数：Width 指定矩形的宽度，范围：0~128
  * 参    数：Height 指定矩形的高度，范围：0~64
  * 参    数：IsFilled 指定矩形是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawRectangle(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	uint8_t i, j;
	if (!IsFilled)			//如果不填充
	{
		/*绘制四条边*/
		OLED_DrawLine(X, Y, X + Width - 1, Y);
		OLED_DrawLine(X, Y, X, Y + Height - 1);
		OLED_DrawLine(X, Y + Height - 1, X + Width - 1, Y + Height - 1);
		OLED_DrawLine(X + Width - 1, Y, X + Width - 1, Y + Height - 1);
	}
	else					//如果填充
	{
		/*遍历矩形区域*/
		for (i = X; i < X + Width; i ++)
		{
			for (j = Y; j < Y + Height; j ++)
			{
				OLED_DrawPoint(i, j);		//画点
			}
		}
	}
}

/**
  * 函    数：OLED画三角形
  * 参    数：X0 指定第一个点横坐标，范围：0~127
  * 参    数：Y0 指定第一个点纵坐标，范围：0~63
  * 参    数：X1 指定第二个点横坐标，范围：0~127
  * 参    数：Y1 指定第二个点纵坐标，范围：0~63
  * 参    数：X2 指定第三个点横坐标，范围：0~127
  * 参    数：Y2 指定第三个点纵坐标，范围：0~63
  * 参    数：IsFilled 指定三角形是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawTriangle(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1, uint8_t X2, uint8_t Y2, uint8_t IsFilled)
{
	uint8_t i, j;
	uint8_t minx, maxx, miny, maxy;
	
	if (!IsFilled)			//如果不填充
	{
		/*绘制三条边*/
		OLED_DrawLine(X0, Y0, X1, Y1);
		OLED_DrawLine(X0, Y0, X2, Y2);
		OLED_DrawLine(X1, Y1, X2, Y2);
	}
	else					//如果填充
	{
		/*计算三个点的包围矩形*/
		minx = X0;
		if (X1 < minx) minx = X1;
		if (X2 < minx) minx = X2;
		maxx = X0;
		if (X1 > maxx) maxx = X1;
		if (X2 > maxx) maxx = X2;
		miny = Y0;
		if (Y1 < miny) miny = Y1;
		if (Y2 < miny) miny = Y2;
		maxy = Y0;
		if (Y1 > maxy) maxy = Y1;
		if (Y2 > maxy) maxy = Y2;
		
		/*遍历矩形区域*/
		for (i = minx; i <= maxx; i ++)
		{
			for (j = miny; j <= maxy; j ++)
			{
				/*判断点是否在三角形内*/
				/*使用重心坐标系法判断*/
				if ((X1 - X0) * (j - Y0) - (Y1 - Y0) * (i - X0) >= 0 &&
					(X2 - X1) * (j - Y1) - (Y2 - Y1) * (i - X1) >= 0 &&
					(X0 - X2) * (j - Y2) - (Y0 - Y2) * (i - X2) >= 0)
				{
					OLED_DrawPoint(i, j);	//画点
				}
				if ((X1 - X0) * (j - Y0) - (Y1 - Y0) * (i - X0) <= 0 &&
					(X2 - X1) * (j - Y1) - (Y2 - Y1) * (i - X1) <= 0 &&
					(X0 - X2) * (j - Y2) - (Y0 - Y2) * (i - X2) <= 0)
				{
					OLED_DrawPoint(i, j);	//画点
				}
			}
		}
	}
}

/**
  * 函    数：OLED画圆
  * 参    数：X 指定圆心的横坐标，范围：0~127
  * 参    数：Y 指定圆心的纵坐标，范围：0~63
  * 参    数：Radius 指定圆的半径，范围：0~64
  * 参    数：IsFilled 指定圆是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawCircle(uint8_t X, uint8_t Y, uint8_t Radius, uint8_t IsFilled)
{
	int16_t x = 0, y = Radius;
	int16_t d = 1 - Radius;
	
	if (!IsFilled)			//如果不填充
	{
		while (y >= x)
		{
			/*绘制八分圆*/
			OLED_DrawPoint(X + x, Y + y);
			OLED_DrawPoint(X + y, Y + x);
			OLED_DrawPoint(X - x, Y + y);
			OLED_DrawPoint(X - y, Y + x);
			OLED_DrawPoint(X + x, Y - y);
			OLED_DrawPoint(X + y, Y - x);
			OLED_DrawPoint(X - x, Y - y);
			OLED_DrawPoint(X - y, Y - x);
			
			if (d < 0)
			{
				d += 2 * x + 3;
			}
			else
			{
				d += 2 * (x - y) + 5;
				y --;
			}
			x ++;
		}
	}
	else					//如果填充
	{
		while (y >= x)
		{
			/*绘制填充圆*/
			/*遍历八分圆区域，绘制四条水平扫描线*/
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - y, Y + x, X + y, Y + x);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			OLED_DrawLine(X - y, Y - x, X + y, Y - x);
			
			if (d < 0)
			{
				d += 2 * x + 3;
			}
			else
			{
				d += 2 * (x - y) + 5;
				y --;
			}
			x ++;
		}
	}
}

/**
  * 函    数：OLED画椭圆
  * 参    数：X 指定椭圆中心的横坐标，范围：0~127
  * 参    数：Y 指定椭圆中心的纵坐标，范围：0~63
  * 参    数：A 指定椭圆横向半轴的长度，范围：0~64
  * 参    数：B 指定椭圆纵向半轴的长度，范围：0~32
  * 参    数：IsFilled 指定椭圆是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawEllipse(uint8_t X, uint8_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x = 0, y = B;
	int32_t a2 = A * A, b2 = B * B;
	int32_t d1 = b2 + a2 * (-B + 0.25);
	
	if (!IsFilled)			//如果不填充
	{
		while (b2 * (x + 1) < a2 * (y - 0.5))
		{
			/*绘制四分椭圆*/
			OLED_DrawPoint(X + x, Y + y);
			OLED_DrawPoint(X - x, Y + y);
			OLED_DrawPoint(X + x, Y - y);
			OLED_DrawPoint(X - x, Y - y);
			
			if (d1 < 0)
			{
				d1 += b2 * (2 * x + 3);
			}
			else
			{
				d1 += b2 * (2 * x + 3) + a2 * (-2 * y + 2);
				y --;
			}
			x ++;
		}
		
		int32_t d2 = b2 * (x + 0.5) * (x + 0.5) + a2 * (y - 1) * (y - 1) - a2 * b2;
		while (y > 0)
		{
			/*绘制四分椭圆*/
			OLED_DrawPoint(X + x, Y + y);
			OLED_DrawPoint(X - x, Y + y);
			OLED_DrawPoint(X + x, Y - y);
			OLED_DrawPoint(X - x, Y - y);
			
			if (d2 < 0)
			{
				d2 += b2 * (2 * x + 2) + a2 * (-2 * y + 3);
				x ++;
			}
			else
			{
				d2 += a2 * (-2 * y + 3);
			}
			y --;
		}
		/*绘制最后四个点*/
		OLED_DrawPoint(X + x, Y);
		OLED_DrawPoint(X - x, Y);
	}
	else					//如果填充
	{
		while (b2 * (x + 1) < a2 * (y - 0.5))
		{
			/*绘制填充椭圆*/
			/*遍历四分椭圆区域，绘制两条水平扫描线*/
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			
			if (d1 < 0)
			{
				d1 += b2 * (2 * x + 3);
			}
			else
			{
				d1 += b2 * (2 * x + 3) + a2 * (-2 * y + 2);
				y --;
			}
			x ++;
		}
		
		int32_t d2 = b2 * (x + 0.5) * (x + 0.5) + a2 * (y - 1) * (y - 1) - a2 * b2;
		while (y > 0)
		{
			/*绘制填充椭圆*/
			/*遍历四分椭圆区域，绘制两条水平扫描线*/
			OLED_DrawLine(X - x, Y + y, X + x, Y + y);
			OLED_DrawLine(X - x, Y - y, X + x, Y - y);
			
			if (d2 < 0)
			{
				d2 += b2 * (2 * x + 2) + a2 * (-2 * y + 3);
				x ++;
			}
			else
			{
				d2 += a2 * (-2 * y + 3);
			}
			y --;
		}
		/*绘制最后一条水平扫描线*/
		OLED_DrawLine(X - x, Y, X + x, Y);
	}
}

/**
  * 函    数：OLED画弧线
  * 参    数：X 指定弧线圆心的横坐标，范围：0~127
  * 参    数：Y 指定弧线圆心的纵坐标，范围：0~63
  * 参    数：Radius 指定弧线的半径，范围：0~64
  * 参    数：StartAngle 指定弧线的起始角度，范围：-180~180
  * 参    数：EndAngle 指定弧线的终止角度，范围：-180~180
  * 参    数：IsFilled 指定弧线是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，需调用OLED_Update函数才能生效
  */
void OLED_DrawArc(uint8_t X, uint8_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x = 0, y = Radius;
	int16_t d = 1 - Radius;
	
	/*角度检查*/
	while (StartAngle < -180) StartAngle += 360;
	while (StartAngle > 180) StartAngle -= 360;
	while (EndAngle < -180) EndAngle += 360;
	while (EndAngle > 180) EndAngle -= 360;
	
	if (!IsFilled)			//如果不填充
	{
		while (y >= x)
		{
			/*计算弧线上的点所在的夹角*/
			/*并根据起始角度和终止角度进行显示判断*/
			if (atan2(y, x) * 180 / 3.14159 >= StartAngle && atan2(y, x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + x, Y + y);
			if (atan2(x, y) * 180 / 3.14159 >= StartAngle && atan2(x, y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + y, Y + x);
			if (atan2(y, -x) * 180 / 3.14159 >= StartAngle && atan2(y, -x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - x, Y + y);
			if (atan2(x, -y) * 180 / 3.14159 >= StartAngle && atan2(x, -y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - y, Y + x);
			if (atan2(-y, x) * 180 / 3.14159 >= StartAngle && atan2(-y, x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + x, Y - y);
			if (atan2(-x, y) * 180 / 3.14159 >= StartAngle && atan2(-x, y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + y, Y - x);
			if (atan2(-y, -x) * 180 / 3.14159 >= StartAngle && atan2(-y, -x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - x, Y - y);
			if (atan2(-x, -y) * 180 / 3.14159 >= StartAngle && atan2(-x, -y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - y, Y - x);
			
			if (d < 0)
			{
				d += 2 * x + 3;
			}
			else
			{
				d += 2 * (x - y) + 5;
				y --;
			}
			x ++;
		}
	}
	else					//如果填充
	{
		while (y >= x)
		{
			/*根据起始角度和终止角度进行填充绘制*/
			/*此处简单起见，仅提供不填充弧线的显示方案，如需填充方案，可自行补全*/
			if (atan2(y, x) * 180 / 3.14159 >= StartAngle && atan2(y, x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + x, Y + y);
			if (atan2(x, y) * 180 / 3.14159 >= StartAngle && atan2(x, y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + y, Y + x);
			if (atan2(y, -x) * 180 / 3.14159 >= StartAngle && atan2(y, -x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - x, Y + y);
			if (atan2(x, -y) * 180 / 3.14159 >= StartAngle && atan2(x, -y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - y, Y + x);
			if (atan2(-y, x) * 180 / 3.14159 >= StartAngle && atan2(-y, x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + x, Y - y);
			if (atan2(-x, y) * 180 / 3.14159 >= StartAngle && atan2(-x, y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X + y, Y - x);
			if (atan2(-y, -x) * 180 / 3.14159 >= StartAngle && atan2(-y, -x) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - x, Y - y);
			if (atan2(-x, -y) * 180 / 3.14159 >= StartAngle && atan2(-x, -y) * 180 / 3.14159 <= EndAngle) OLED_DrawPoint(X - y, Y - x);
			
			if (d < 0)
			{
				d += 2 * x + 3;
			}
			else
			{
				d += 2 * (x - y) + 5;
				y --;
			}
			x ++;
		}
	}
}

/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
