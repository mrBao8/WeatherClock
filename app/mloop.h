#ifndef __MAINLOOP_H__
#define __MAINLOOP_H__

#include <stdint.h>
#include "rtc.h"

/* ==================== 时间单位转换宏定义 ==================== */
#define MILLISECONDS(x)  (x)
#define SECONDS(x)       MILLISECONDS((x) * 1000)
#define MINUTES(x)       SECONDS((x) * 60)
#define HOURS(x)         MINUTES((x) * 60)
#define DAYS(x)          HOURS((x) * 24)

/* ====================  各项任务刷新周期 ==================== */
#define TIME_SYNC_INTERVAL        DAYS(1)         // 1天同步一次网络时间
#define WIFI_UPDATE_INTERVAL      SECONDS(5)      // 5秒检查一次WiFi状态
#define TIME_UPDATE_INTERVAL      SECONDS(1)      // 1秒刷新一次屏幕时间
#define INNIR_UPDATE_INTERVAL     SECONDS(3)      // 3秒采集一次室内温湿度
#define OUTDOOR_UPDATE_INTERVAL   MINUTES(1)      // 1分钟刷一次室外天气

/* ====================  对外核心接口 ==================== */
void main_loop_init(void);  // 初始化业务主循环（注册回调）
void main_loop_proc(void);  // 丢在 main 的 while(1) 里的业务处理器

#endif /* __MAINLOOP_H__ */
