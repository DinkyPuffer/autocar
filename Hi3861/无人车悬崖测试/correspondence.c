#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifiiot_uart.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "hi_io.h"

// 引脚宏定义
#define GPIOL WIFI_IOT_IO_NAME_GPIO_13
#define GPIOR WIFI_IOT_IO_NAME_GPIO_14

static uint8_t uart_sendbuf[6];

/******************* 1. 串口硬件初始化 *******************/

void stm32_uart_init(void)
{
    // 初始化 UART2 引脚 (GPIO_11 -> TXD, GPIO_12 -> RXD)
    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);

    // 配置串口参数：波特率 115200，8数据位，1停止位，无校验
    WifiIotUartAttribute uart_attr2 = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    UartInit(WIFI_IOT_UART_IDX_2, &uart_attr2, NULL);
}

/******************* 2. 电机控制协议与基础动作 *******************/

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

void car_forward(void)  { stm32motor_control(50, 50); }
void car_backward(void) { stm32motor_control(-50, -50); }
void car_left(void)     { stm32motor_control(-60, 60); }  
void car_right(void)    { stm32motor_control(60, -60); }  
void car_stop(void)     { stm32motor_control(0, 0); }

void car_emergency_brake(void)
{
    stm32motor_control(-80, -80);
    usleep(100000); // 100ms 强反推
    car_stop();
}

/******************* 3. 悬崖避障任务 *******************/

static void CliffAvoidanceTask(void)
{
    // 初始化串口与传感器 GPIO
    stm32_uart_init();

    IoSetFunc(GPIOL, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(GPIOR, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(GPIOL, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(GPIOR, WIFI_IOT_GPIO_DIR_IN);

    printf("Cliff Avoidance Task Started!\r\n");

    WifiIotGpioValue status_l = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue status_r = WIFI_IOT_GPIO_VALUE0;

    while (1) {
        // 读取左右传感器引脚电平
        GpioGetInputVal(GPIOL, &status_l);
        GpioGetInputVal(GPIOR, &status_r);

        // 1. 判断左红外是否检测到悬崖（高电平 1 代表悬空/无反射）
        if (status_l == WIFI_IOT_GPIO_VALUE1) {
            printf("Left Cliff Detected!\r\n");
            
            car_emergency_brake();  // 急刹
            usleep(100000);         // 停顿 100ms
            
            car_backward();         // 倒车
            usleep(400000);         // 倒车 0.4 秒
            
            car_right();            // 右转
            usleep(900000);         // 修改点：右转时间提高 3 倍 (0.3s -> 0.9s)
        } 
        // 2. 若左侧无悬崖，判断右红外是否检测到悬崖
        else if (status_r == WIFI_IOT_GPIO_VALUE1) {
            printf("Right Cliff Detected!\r\n");
            
            car_emergency_brake();  // 急刹
            usleep(100000);         // 停顿 100ms
            
            car_backward();         // 倒车
            usleep(400000);         // 倒车 0.4 秒
            
            car_left();             // 左转
            usleep(900000);         // 修改点：左转时间提高 3 倍 (0.3s -> 0.9s)
        } 
        // 3. 两侧均未检测到悬崖，保持前进
        else {
            car_forward();
        }

        // 轮询周期 20ms
        usleep(20000); 
    }
}

/******************* 4. 入口线程注册 *******************/

void RobotCarInit(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "CliffAvoidanceTask";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)CliffAvoidanceTask, NULL, &attr) == NULL) {
        printf("Failed to create CliffAvoidanceTask!\r\n");
    }
}

APP_FEATURE_INIT(RobotCarInit);
