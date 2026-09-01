#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "wifiiot_uart.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "hi_io.h"
#include "hi_time.h"
#include "wifiiot_gpio_ex.h"

#define GPIOL 13
#define GPIOR 14

static uint8_t uart_sendbuf[6];

/******************* 串口通信与电机驱动 *******************/

void stm32motor_control(int motorA, int motorB)
{
    uint8_t A_dir = (motorA < 0) ? 1 : 0;
    uint8_t B_dir = (motorB < 0) ? 1 : 0;

    int speedA = abs(motorA);
    int speedB = abs(motorB);

    if (speedA > 150) speedA = 150;
    if (speedB > 150) speedB = 150;

    uart_sendbuf[0] = 0xFC;
    uart_sendbuf[1] = A_dir;
    uart_sendbuf[2] = (uint8_t)speedA;
    uart_sendbuf[3] = B_dir;
    uart_sendbuf[4] = (uint8_t)speedB;
    uart_sendbuf[5] = 0xFD;

    UartWrite(WIFI_IOT_UART_IDX_2, (unsigned char *)uart_sendbuf, 6);
}

// 动作封装
void car_forward(void)  { stm32motor_control(2, 2); }
void car_backward(void) { stm32motor_control(-10, -10); }
void car_left(void)     { stm32motor_control(10, -10); }  
void car_right(void)    { stm32motor_control(-10, 10); }  
void car_stop(void)     { stm32motor_control(0, 0); }

// 强力反接急刹：给予 60ms 的 -50 反向脉冲，瞬间抵消向前惯性
void car_emergency_brake(void)
{
    stm32motor_control(-50, -50);
    usleep(100000); // 强力反推 60ms
    car_stop();
}

/******************* 心跳延时函数 *******************/

void action_delay_with_heartbeat(void (*car_action_func)(void), uint32_t total_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < total_ms)
    {
        car_action_func(); // 持续刷新发送指令
        usleep(50000);     // 延时 50ms
        elapsed += 50;
    }
}

/******************* 悬崖避障主任务 *******************/

static void CliffAvoidanceTask(void)
{
    WifiIotGpioValue left_val, right_val;

    // 1. 串口初始化
    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);

    WifiIotUartAttribute uart_attr2 = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    UartInit(WIFI_IOT_UART_IDX_2, &uart_attr2, NULL);

    // 2. 传感器 GPIO 初始化
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);

    printf("Cliff Avoidance Task Started!\r\n");

    while (1)
    {
        // 1. 判断左侧传感器
        GpioGetInputVal(GPIOL, &left_val);
        
        if (left_val == WIFI_IOT_GPIO_VALUE1)
        {
            printf("Left Cliff Detected! Emergency Braking & Turning Right...\r\n");
            
            // 执行反接急刹，抵消惯性
            car_emergency_brake();
            usleep(100000); // 静止 0.1s 缓冲
            
            // 倒车 1 秒
            action_delay_with_heartbeat(car_backward, 1000);
            
            // 右转 0.8 秒
            action_delay_with_heartbeat(car_right, 800); 
            
            car_stop();
            usleep(200000);
        }
        else
        {
            // 2. 左侧安全，判断右侧传感器
            GpioGetInputVal(GPIOR, &right_val);
            
            if (right_val == WIFI_IOT_GPIO_VALUE1)
            {
                printf("Right Cliff Detected! Emergency Braking & Turning Left...\r\n");
                
                // 执行反接急刹，抵消惯性
                car_emergency_brake();
                usleep(100000); // 静止 0.1s 缓冲
                
                // 倒车 1 秒
                action_delay_with_heartbeat(car_backward, 1000);
                
                // 左转 0.8 秒
                action_delay_with_heartbeat(car_left, 800); 
                
                car_stop();
                usleep(200000);
            }
            else
            {
                // 3. 两侧均安全，保持前进
                car_forward();
            }
        }

        // 👈 采样间隔缩短至 10ms (10000 微秒)，提升检测敏感度度
        usleep(10000);
    }
}

/******************* 任务注册 *******************/

void StartCliffAvoidance(void)
{
    osThreadAttr_t attr;
    attr.name = "CliffAvoidanceTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)CliffAvoidanceTask, NULL, &attr) == NULL) {
        printf("Failed to create CliffAvoidanceTask!\n");
    }
}

APP_FEATURE_INIT(StartCliffAvoidance);
