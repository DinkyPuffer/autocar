#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"         // OpenHarmony GPIO 头文件
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"

#define LED_GPIO_IDX 6       // 原理图 17 脚对应的 IO06

// 传感器触发阈值定义（可按试训现场光线调校）
#define ALS_LOW_THRES   100  // 光强小于 100 认定为暗光环境
#define PS_NEAR_THRES   50   // 接近值大于 50 认定为有人/物体靠近

void Task1(void)
{
    uint8_t displayBuff[20] = {0};
    uint16_t ir = 0, als = 0, ps = 0;

    // 1. 初始化 GPIO06 (控制 LED)
    IoTGpioInit(LED_GPIO_IDX);
    IoTGpioSetDir(LED_GPIO_IDX, IOT_GPIO_DIR_OUT); // 设置为输出模式

    // 2. 初始化 AP3216C 传感器与 OLED 屏幕
    AP3216C_Init();
    SSD1306_Init();
    SSD1306_CLS();

    // 在 OLED 绘制静态标题
    SSD1306_ShowStr(0, 0, (uint8_t *)"=== AP3216C ===", 16);

    while (1)
    {
        // 3. 采集传感器数据
        AP3216C_ReadData(&ir, &als, &ps);

        // 4. 串行显示 (串口调试助手)
        printf("人体红外(ir) = %d  光强(als) = %d  接近(ps) = %d\r\n", ir, als, ps);

        // 5. OLED 显示 (第2行集中显示三组纯数字)
        memset(displayBuff, 0, sizeof(displayBuff));
        sprintf((char*)displayBuff, "%-4d  %-4d  %-4d", ir, als, ps);
        SSD1306_ShowStr(0, 2, (uint8_t *)displayBuff, 16);

        // 6. 条件联动判断：环境变暗(als低) 且 有人靠近(ps高)
        if ((als < ALS_LOW_THRES) && (ps > PS_NEAR_THRES))
        {
            IoTGpioSetOutputVal(LED_GPIO_IDX, IOT_GPIO_VALUE1); // 点亮 IO06 LED
        }
        else
        {
            IoTGpioSetOutputVal(LED_GPIO_IDX, IOT_GPIO_VALUE0); // 熄灭 IO06 LED
        }

        sleep(1);
    }
}

static void i2c_ap3216c_demo(void)
{
    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = 1024;
    options.priority = osPriorityNormal;
    osThreadId_t Task1_ID;
    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL)
    {
        printf("ID = %d, Create Task1_ID is OK!", Task1_ID);
    }
}

APP_FEATURE_INIT(i2c_ap3216c_demo);
