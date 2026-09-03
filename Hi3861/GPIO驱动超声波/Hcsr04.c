#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"
#include "hi_io.h"
#include "hi_time.h"

#define TRIG_GPIO 7
#define ECHO_GPIO 8
#define GPIO_FUNC 0

// 初始化GPIO引脚配置
static void Hcsr04_GpioInit(void)
{
    // 配置GPIO_8 (ECHO) 为输入
    hi_io_set_func(ECHO_GPIO, GPIO_FUNC);
    GpioSetDir(ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);

    // 配置GPIO_7 (TRIG) 为输出
    hi_io_set_func(TRIG_GPIO, GPIO_FUNC);
    GpioSetDir(TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
}

// 测距功能实现 (带超时保护)
float GetDistance(void)
{
    uint32_t start_time = 0;
    uint32_t duration = 0;
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    uint32_t timeout = 0;

    // 1. 发送 20us 的高电平触发脉冲
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    // 2. 等待 ECHO 引脚变为高电平（带 10ms 超时）
    timeout = 0;
    while (1) {
        GpioGetInputVal(ECHO_GPIO, &value);
        if (value == WIFI_IOT_GPIO_VALUE1) {
            start_time = hi_get_us();
            break;
        }
        hi_udelay(1);
        timeout++;
        if (timeout > 10000) { // 10ms 超时
            return -1.0f;     // 超时未接收到Echo起始信号
        }
    }

    // 3. 等待 ECHO 引脚变为低电平（带 30ms 超时，最大可测约5米）
    timeout = 0;
    while (1) {
        GpioGetInputVal(ECHO_GPIO, &value);
        if (value == WIFI_IOT_GPIO_VALUE0) {
            duration = hi_get_us() - start_time;
            break;
        }
        hi_udelay(1);
        timeout++;
        if (timeout > 30000) { // 30ms 超时
            return -2.0f;     // 超时未接收到Echo结束信号
        }
    }

    // 声速 340m/s = 0.034 cm/us，往返除以 2
    return ((float)duration * 0.034f) / 2.0f;
}

void Hsrtext(void* parame)
{
    (void)parame;
    printf("start test hcsr04\r\n");

    Hcsr04_GpioInit();

    while (1) {
        float distance = GetDistance();
        if (distance < 0) {
            printf("HC-SR04 Read Error: %.0f\r\n", distance);
        } else {
            printf("distance is %.1f (cm)\r\n", distance);
        }
        osDelay(200); // 每 200ms 测距一次
    }
}

/* 任务入口 */
static void Hcsr04(void)
{
    WatchDogDisable(); // 关闭看门狗

    osThreadAttr_t attr = {0};
    attr.name = "Hcsr04";
    attr.stack_size = 4096; // 4KB 深度足以满足打印和测距
    attr.priority = osPriorityNormal;

    if (osThreadNew(Hsrtext, NULL, &attr) == NULL) {
        printf("Failed to create Task!\n");
    }
}

APP_FEATURE_INIT(Hcsr04);
