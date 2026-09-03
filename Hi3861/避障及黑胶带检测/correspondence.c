#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifiiot_uart.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"
#include "hi_io.h"
#include "hi_time.h"

// 1. 红外悬崖传感器引脚定义
#define GPIOL WIFI_IOT_IO_NAME_GPIO_13
#define GPIOR WIFI_IOT_IO_NAME_GPIO_14

// 2. 超声波传感器引脚定义
#define TRIG_GPIO 7
#define ECHO_GPIO 8
#define GPIO_FUNC 0

static uint8_t uart_sendbuf[6];

/******************* 串口硬件初始化 *******************/

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

/******************* 超声波 HC-SR04 驱动 *******************/

static void Hcsr04_GpioInit(void)
{
    // 配置 GPIO_8 (ECHO) 为输入
    hi_io_set_func(ECHO_GPIO, GPIO_FUNC);
    GpioSetDir(ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);

    // 配置 GPIO_7 (TRIG) 为输出
    hi_io_set_func(TRIG_GPIO, GPIO_FUNC);
    GpioSetDir(TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
}

float GetDistance(void)
{
    uint32_t start_time = 0;
    uint32_t duration = 0;
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    uint32_t timeout = 0;

    // 发送 20us 高电平脉冲
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    // 等待 ECHO 变高（10ms 超时）
    timeout = 0;
    while (1) {
        GpioGetInputVal(ECHO_GPIO, &value);
        if (value == WIFI_IOT_GPIO_VALUE1) {
            start_time = hi_get_us();
            break;
        }
        hi_udelay(1);
        timeout++;
        if (timeout > 10000) {
            return -1.0f; // 起始超时
        }
    }

    // 等待 ECHO 变低（30ms 超时）
    timeout = 0;
    while (1) {
        GpioGetInputVal(ECHO_GPIO, &value);
        if (value == WIFI_IOT_GPIO_VALUE0) {
            duration = hi_get_us() - start_time;
            break;
        }
        hi_udelay(1);
        timeout++;
        if (timeout > 30000) {
            return -2.0f; // 结束超时
        }
    }

    return ((float)duration * 0.034f) / 2.0f;
}

/******************* 电机控制协议与基础动作 *******************/

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

void car_forward(void)  { stm32motor_control(70, 70); }
void car_backward(void) { stm32motor_control(-70, -70); }
void car_left(void)     { stm32motor_control(-60, 60); }  
void car_right(void)    { stm32motor_control(60, -60); }  
void car_stop(void)     { stm32motor_control(0, 0); }

void car_emergency_brake(void)
{
    stm32motor_control(-80, -80);
    usleep(100000); // 100ms 强反推
    car_stop();
}

/******************* 综合避障与防跌落控制任务 *******************/

static void VehicleControlTask(void)
{
    // 1. 关闭看门狗，防止测距阻塞或软件延时触发复位
    WatchDogDisable();

    // 2. 初始化硬件（串口、红外 GPIO、超声波 GPIO）
    stm32_uart_init();
    
    IoSetFunc(GPIOL, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(GPIOR, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(GPIOL, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(GPIOR, WIFI_IOT_GPIO_DIR_IN);

    Hcsr04_GpioInit();

    printf("Vehicle Integrated Control Task Started!\r\n");

    WifiIotGpioValue status_l = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue status_r = WIFI_IOT_GPIO_VALUE0;

    while (1) {
        // 读取超声波测距结果
        float distance = GetDistance();

        // 读取左右悬崖红外传感器电平
        GpioGetInputVal(GPIOL, &status_l);
        GpioGetInputVal(GPIOR, &status_r);

        // 优先级 1：前方超声波障碍物检测 (< 20cm 且排除测距异常的负值)
        if (distance > 0.0f && distance < 20.0f) {
            printf("Obstacle Detected! Distance: %.1f cm -> Action: Back & Right\r\n", distance);
            
            car_emergency_brake();  // 急刹
            usleep(100000);         // 停顿 100ms
            
            car_backward();         // 倒车
            usleep(800000);         // 倒车 0.4s
            
            car_right();            // 右转
            usleep(900000);         // 右转 0.9s
        } 
        // 优先级 2：左红外检测到悬崖
        else if (status_l == WIFI_IOT_GPIO_VALUE1) {
            printf("Left Cliff Detected! -> Action: Back & Right\r\n");
            
            car_emergency_brake();  // 急刹
            usleep(100000);         // 停顿 100ms
            
            car_backward();         // 倒车
            usleep(800000);         // 倒车 0.4s
            
            car_right();            // 右转
            usleep(900000);         // 右转 0.9s
        } 
        // 优先级 3：右红外检测到悬崖
        else if (status_r == WIFI_IOT_GPIO_VALUE1) {
            printf("Right Cliff Detected! -> Action: Back & Left\r\n");
            
            car_emergency_brake();  // 急刹
            usleep(100000);         // 停顿 100ms
            
            car_backward();         // 倒车
            usleep(800000);         // 倒车 0.4s
            
            car_left();             // 左转
            usleep(900000);         // 左转 0.9s
        } 
        // 优先级 4：全安全，保持前进
        else {
            car_forward();
        }

        // 轮询延时
        usleep(20000); 
    }
}

/******************* 入口线程注册 *******************/

void RobotCarInit(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "VehicleControlTask";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)VehicleControlTask, NULL, &attr) == NULL) {
        printf("Failed to create VehicleControlTask!\r\n");
    }
}

APP_FEATURE_INIT(RobotCarInit);
