#include <stdio.h>
#include <unistd.h>
#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include <stdlib.h>
#include <memory.h>
#include "cmsis_os2.h"
#include "hi_io.h"

#define GPIOL 13
#define GPIOR 14

static osTimerId_t g_tcrt_timer_id = NULL;

/**
 * @brief 获取 TCRT5000 传感器状态并打印
 * @note  根据 LM393 原理图：
 *        低电平(0) -> 识别到白色/有反射 (传感器灯亮)
 *        高电平(1) -> 识别到黑色/无反射 (传感器灯灭)
 */
void get_tcrt5000_value(void) 
{
    WifiIotGpioValue status_l = WIFI_IOT_GPIO_VALUE1;
    WifiIotGpioValue status_r = WIFI_IOT_GPIO_VALUE1;

    // 读取左侧 (GPIO_13) 和右侧 (GPIO_14) 的电平状态
    GpioGetInputVal(GPIOL, &status_l);
    GpioGetInputVal(GPIOR, &status_r);

    // 判读左传感器状态
    if (status_l == WIFI_IOT_GPIO_VALUE0) {
        printf("left white\r\n");
    } else {
        printf("left black\r\n");
    }

    // 判读右传感器状态
    if (status_r == WIFI_IOT_GPIO_VALUE0) {
        printf("right white\r\n");
    } else {
        printf("right black\r\n");
    }
}

/**
 * @brief 定时器回调函数（每 50ms 触发一次检测）
 */
static void Timer_Callback(void *arg)
{
    (void)arg;
    get_tcrt5000_value();
}

/**
 * @brief TCRT 任务主函数
 */
static void TCRTTask(void)
{
    printf("start tcrt5000 test task...\r\n");

    // 创建周期性软件定时器 (5U = 50ms 检测一次)
    g_tcrt_timer_id = osTimerNew(Timer_Callback, osTimerPeriodic, NULL, NULL);
    
    if (g_tcrt_timer_id != NULL) {
        // 启动定时器，时间间隔为 5U (约 50ms)
        osStatus_t status = osTimerStart(g_tcrt_timer_id, 5U);
        if (status == osOK) {
            printf("TCRT timer started successfully!\r\n");
        } else {
            printf("Failed to start TCRT timer!\r\n");
        }
    } else {
        printf("Failed to create TCRT timer!\r\n");
    }
}

/**
 * @brief TCRT 模块 GPIO 及线程初始化
 */
void TCRT(void)
{
    // 1. 初始化 GPIO 功能
    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);

    // 2. 设置输入方向
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);

    // 3. 创建运行任务
    osThreadAttr_t attr = {0};
    attr.name = "TCRTTask";
    attr.stack_size = 4096; // 调整为常规推荐大小，避免占用过多栈空间
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)TCRTTask, NULL, &attr) == NULL) {
        printf("Failed to create TCRTTask!\r\n");
    }
}

APP_FEATURE_INIT(TCRT);
