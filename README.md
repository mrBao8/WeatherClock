# 基于 STM32F407 + FreeRTOS 的智能网络天气时钟

## 项目简介

本项目是一个基于 `STM32F407VET6` 的桌面智能天气时钟，使用 `FreeRTOS` 对原裸机版本进行任务化移植。系统集成了 RTC 本地走时、AHT20 室内温湿度采集、ESP32-C3 联网、SNTP 网络校时、心知天气 API 抓取以及 ST7789 LCD 图形界面显示。

项目目标不是堆叠功能，而是在保持原有硬件接线和页面效果的基础上，将裸机轮询逻辑拆分为更清晰的实时任务结构，让显示、网络、传感器和时间同步互不阻塞，提升演示稳定性和代码可维护性。

## 功能特性

- 欢迎页显示 2 秒，随后进入 WiFi 连接页，再进入主页面。
- RTC 负责本地走时，联网后通过 SNTP 校准时间，默认每天同步一次。
- AHT20 每 3 秒采集一次室内温湿度，并刷新主页面室内环境区域。
- ESP32-C3 通过 AT 指令连接 WiFi，并请求心知天气 API 获取室外天气。
- 天气开机联网后立即抓取一次，成功后每 1 分钟更新，失败后 5 秒重试。
- WiFi 断开时主页面显示 `wifi lost` / `Notnet`，RTC 时间仍可正常显示。
- LCD 显示使用 UI 队列串行化刷新，避免多任务同时写屏导致错位和闪烁。
- ST7789 写 GRAM 阶段使用 SPI2 + DMA 传输，提高图片和字体刷新速度。

## 硬件环境

| 模块 | 型号 / 说明 |
| --- | --- |
| 主控 | STM32F407VET6，ARM Cortex-M4 |
| 屏幕 | 2.4 寸 ST7789 LCD，240x320 |
| 无线模块 | ESP32-C3，ESP-AT 固件 |
| 温湿度传感器 | AHT20 |
| 开发环境 | Keil MDK5 + STM32F4 标准外设库 |
| RTOS | FreeRTOS |

## 当前工程接线

| 外设 | 引脚 |
| --- | --- |
| LCD SPI2 SCK | PB13 |
| LCD SPI2 MOSI | PB15 |
| LCD CS | PD12 |
| LCD DC | PD10 |
| LCD RST | PD11 |
| ESP USART2 TX/RX | PA2 / PA3 |
| AHT20 I2C2 SCL/SDA | PB10 / PB11 |

## 工程结构

```text
WeatherClock/
├── app/              # 应用层逻辑、页面业务、FreeRTOS 调度
│   ├── ui/           # UI 队列封装，统一串行执行 LCD 操作
│   ├── image/        # 欢迎页、WiFi 页、天气图标等图片资源
│   ├── font/         # LCD 字库资源
│   ├── app.c         # 时间、WiFi、室内外天气等应用调度
│   ├── main.c        # 系统入口与启动流程
│   ├── wireless.c    # 开机网络初始化与首帧天气缓存
│   └── workqueue.c   # 后台工作队列，承载耗时任务
├── devices/          # 组件层协议封装
│   ├── espat.c       # ESP32-C3 AT 指令、USART2 中断接收
│   └── weather.c     # 心知天气响应解析
├── driver/           # 底层驱动
│   ├── lcd.c         # ST7789 LCD、SPI2 DMA 刷新
│   ├── aht20.c       # AHT20 温湿度传感器
│   ├── rtc.c         # RTC 本地时间
│   └── serial.c      # 调试串口
├── system/           # 系统时基与板级初始化
├── third_lib/        # FreeRTOS 与 STM32 标准库
└── mdk/              # Keil MDK 工程
```

## 软件架构

裸机版本主要依赖 SysTick + 主循环轮询。FreeRTOS 版本将系统拆成三类执行路径：

- `Timer`：负责周期触发，如 1 秒时间刷新、3 秒室内温湿度采集、5 秒 WiFi 状态检测。
- `workqueue`：负责执行可能阻塞的任务，如 ESP AT 查询、HTTP 天气请求、SNTP 同步。
- `UI Task`：负责统一执行 LCD 清屏、填充、字符串和图片显示，避免多任务直接抢占 LCD。

这种结构保留了原裸机版本的业务流程，同时让耗时网络操作不再直接卡住页面刷新。

## 启动流程

```text
Board_LowLevel_Init()
    -> FreeRTOS scheduler
        -> Board_Init()
        -> ui_init()
        -> welcome_page_display() 2s
        -> wireless_init()
        -> app_network_start()
        -> WiFi page 2s
        -> main_page_display()
        -> app_start()
```

主页面启动后会先显示 RTC 时间和室内环境，网络后台连接成功后再刷新 WiFi 名称、室外城市、天气图标和室外温度。

## 关键实现

### FreeRTOS 任务解耦

应用层使用 FreeRTOS 软件定时器管理周期任务，并通过 `workqueue` 执行慢操作。天气请求使用 one-shot 定时器调度，避免上一轮 HTTP 未结束时下一轮请求继续堆积。

### LCD SPI DMA 刷新

LCD 命令仍使用普通 SPI 写入，GRAM 像素数据使用 SPI2 DMA 发送。图片数据按 RGB565 高低字节打包为 `uint16_t` 后再进行 16 位 DMA 传输，解决了直接搬运图片资源时可能出现的条纹问题。

### UI 队列

所有页面刷新接口通过 UI 队列投递消息，由 UI Task 单线程执行实际 LCD 操作。这样可以避免时间刷新、天气刷新、WiFi 刷新同时访问 LCD 导致显示错乱。

### ESP AT 通信

ESP32-C3 使用 USART2 与 STM32 通信。USART2 RX 中断接收 AT 响应，识别 `OK`、`ERROR`、`busy p...` 等返回状态，并通过信号量唤醒等待中的 AT 命令函数。

### 时间与天气策略

RTC 是本地走时核心，SNTP 只用于联网校准。天气数据在开机联网后立即获取，成功后 1 分钟更新一次，失败后 5 秒重试。这样既保证上电后尽快显示室外信息，又避免网络异常时串口和 AT 请求刷屏。

## 调试日志

日常运行只保留关键日志，例如：

```text
[SYSTEM] WeatherClock FreeRTOS start
[AT] base protocol inited
[WIFI] connected
[SYSTEM] main tasks started
[SNTP] sync success: 2026-07-09 12:20:38
[WEATHER] Luoyang, Cloudy, 34.0
```

完整 AT 收发调试默认关闭，可在 `devices/espat.c` 中打开 `ESP_AT_DEBUG`。

## 构建方式

1. 使用 Keil MDK5 打开 `mdk/stm32f407.uvprojx`。
2. 选择 `stm32f407` target。
3. 编译并下载到 STM32F407VET6 开发板。
4. 串口助手连接调试串口，观察系统启动、联网、校时和天气更新日志。

## 配置说明

WiFi 名称、密码和天气 API URL 当前位于 `app/wireless.h`。公开仓库使用前建议替换为自己的热点信息和心知天气 key。

## 项目效果

<img width="1066" height="1556" alt="8fbda3982ab4e7bfdf60417483789bb4" src="https://github.com/user-attachments/assets/da4be553-0a05-4882-91b7-944c5781cdfe" />

