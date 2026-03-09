# ECG Monitor v1.4

基于 STM32F411CEU6 的便携式心电图 (ECG) 监测设备，使用 AD8232 模拟前端采集心电信号，OLED 屏幕实时显示波形与心率，支持录制回放与用户管理。

## 硬件平台

| 项目 | 规格 |
|------|------|
| MCU | STM32F411CEU6 (Cortex-M4, UFQFPN48) |
| 系统时钟 | 100 MHz (HSI 16 MHz → PLL) |
| 封装 | UFQFPN48 |
| 工具链 | Keil MDK-ARM V5.32 |
| HAL 版本 | STM32Cube FW_F4 V1.28.3 |

## 外设与引脚分配

| 模块 | 外设 | 引脚 | 说明 |
|------|------|------|------|
| AD8232 ECG | ADC1_CH5 | PA5 | ECG 模拟信号输入 |
| AD8232 LO- | GPIO Input | PB1 | 导联脱落检测 (负) |
| AD8232 LO+ | GPIO Input | PB2 | 导联脱落检测 (正) |
| OLED 显示屏 | I2C1 | PB6 (SCL) / PB7 (SDA) | 0.96" OLED |
| SD 卡 | SPI1 | PB3 (SCK) / PB4 (MISO) / PA7 (MOSI) | SPI 模式 |
| SD 卡 CS | GPIO Output | PB5 | 片选 |
| 电池检测 | ADC1_CH4 | PA4 | 分压比 (20K+10K)/10K |
| 按键 K1 | GPIO Input | PA11 | 上一页 |
| 按键 K2 | GPIO Input | PA12 | 功能键 |
| 按键 K3 | GPIO Input | PA15 | 下一页 |
| 串口调试 | USART1 | PA9 (TX) / PA10 (RX) | — |
| 蓝牙/扩展 | USART2 | PA2 (TX) / PA3 (RX) | — |
| LED | GPIO Output | PA8 | 状态指示灯 |
| 蜂鸣器 | GPIO Output | PB10 | 低电量报警 |

## 软件架构

```
ecg_v1.4/
├── Core/                       STM32CubeMX 生成代码
│   ├── Inc/                    外设头文件 (adc, gpio, i2c, spi, usart, tim ...)
│   └── Src/                    外设初始化 + main.c
├── User/                       用户业务模块
│   ├── ad8232/                 AD8232 驱动 (采样、滤波、心率检测、数据上传)
│   ├── battery/                电池电压监测 (ADC + 分压计算)
│   ├── beep/                   蜂鸣器驱动
│   ├── display/                OLED 多页面显示管理
│   ├── key/                    按键扫描 (消抖、短按/长按)
│   ├── led/                    LED 驱动
│   ├── oled/                   OLED 底层驱动 + 字库
│   ├── recorder/               ECG 录制模块 (块存储、索引管理)
│   ├── sd/                     SD 卡 SPI 驱动
│   ├── storage/                存储抽象层 (Flash / SD 双后端)
│   ├── userinfo/               用户信息管理 (姓名 + 年龄)
│   └── config.h                全局功能开关
├── Drivers/                    CMSIS + HAL 驱动库
└── MDK-ARM/                    Keil 工程文件
```

## 功能特性

### 实时心电监测
- AD8232 模拟前端采集 ECG 信号
- 导联脱落检测 (LO+/LO-)
- 实时波形绘制与心率计算
- OLED 屏幕显示波形、心率、录制时长、电池电量

### 录制与回放
- 支持 **连续录制** 和 **定时录制** 两种模式
- 最多保存 64 条录制记录
- 每条记录包含：录制 ID、模式、用户信息、采样总数、时长、平均/最大心率
- 块存储架构（512 字节/块），Block 0 主索引，Block 1-4 记录索引，Block 256+ 数据区

### 存储后端
- **Internal Flash**：使用 Sector 5-7（384 KB），编译时选择 `STORAGE_USE_FLASH`
- **SD 卡**：SPI 模式，支持 SD V1/V2/SDHC/MMC，编译时选择 `STORAGE_USE_SD`
- 统一的 `Storage_ReadBlock` / `Storage_WriteBlock` 抽象 API

### 用户管理
- 支持设置用户姓名（最多 7 个 ASCII 字符）与年龄
- 用户信息持久化存储，关联到每条录制记录

### 显示页面
| 页面 | 名称 | 功能 |
|------|------|------|
| 0 | ECG | 实时波形 + 心率 + 录制时间 + 电池 + 导联状态 |
| 1 | History | 录制历史列表 / 波形回放 |
| 2 | User | 用户信息编辑（姓名 / 年龄） |
| 3 | Settings | 监测模式 + 定时时长 + 清除记录 |
| 4 | Debug | 电池 ADC / 按键 / LED 调试信息（可选） |

### 按键操作
| 按键 | 短按 | 长按 |
|------|------|------|
| K1 (PA11) | 上一页 / 向上 | — |
| K2 (PA12) | 功能键（录制/确认） | — |
| K3 (PA15) | 下一页 / 向下 | — |

### 电池管理
- ADC 采样 + 分压电阻计算实际电压
- 电池百分比估算（满电 5000 mV，低电 4000 mV，亏电 3500 mV）
- 低电量蜂鸣器报警

## 编译配置

在 `User/config.h` 中通过宏定义控制可选功能：

```c
#define FEATURE_DEBUG_PAGE      // 启用 Debug 调试页面
```

在 `User/storage/storage.h` 中选择存储后端：

```c
#define STORAGE_USE_FLASH   1   // 使用内部 Flash
// #define STORAGE_USE_SD   1   // 使用 SD 卡
```

## 构建与烧录

1. 使用 Keil MDK-ARM V5 打开 `MDK-ARM/ecg_v1.4.uvprojx`
2. 编译工程 (Build → Rebuild All)
3. 通过 ST-Link/J-Link 烧录到 STM32F411CEU6 开发板
