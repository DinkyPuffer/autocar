#include "pwm.h"
#include <stdlib.h> // ?? abs() ????

// ????????
static int Target_Limit(int val, int max_val)
{
    if (val > max_val)  return max_val;
    if (val < -max_val) return -max_val;
    return val;
}

// 1. ?? GPIO ???
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // ?? GPIOB ??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); 
    
    // ?? PB13 (?????), PB14 (?????) ?????
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       
    GPIO_Init(GPIOB, &GPIO_InitStructure);                  
    
    // ??????????,?????????
    GPIO_ResetBits(GPIOB, GPIO_Pin_13 | GPIO_Pin_14);
}

// 2. ??? PWM ???
void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    Motor_Init(); // ???????????

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);  // ?? TIM4 ??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ?? GPIOB ??

    // ?? PB6 (TIM4_CH1 ???PWM) ? PB7 (TIM4_CH2 ???PWM) ???????
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ?? TIM4 ??
    TIM_TimeBaseStructure.TIM_Period = arr;                     // ??????
    TIM_TimeBaseStructure.TIM_Prescaler = psc;                  // ?????
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;        
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // ????
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // ?? TIM4 PWM ??
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;                            
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     
    
    TIM_OC1Init(TIM4, &TIM_OCInitStructure); // ??? PWM (PB6)
    TIM_OC2Init(TIM4, &TIM_OCInitStructure); // ??? PWM (PB7)

    // ?????
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable); 
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable); 

    TIM_ARRPreloadConfig(TIM4, ENABLE); 
    TIM_Cmd(TIM4, ENABLE); // ????? TIM4
}

// 3. ?? PWM ????
void Set_Pwm(int moto_left, int moto_right)
{
    // 1. ????????
    moto_left = Target_Limit(moto_left, PWM_MAX);
    moto_right = Target_Limit(moto_right, PWM_MAX);

    // 2. ????? (PWM: TIM4_CH2/PB7, ??: PB14)
    if (moto_left >= 0) {
        L_IB = 0;                        // ?????
        PWMB = abs(moto_left);           // ?????
    } else {
        L_IB = 1;                        // ?????
        PWMB = PWM_MAX - abs(moto_left); // ???????
    }

    // 3. ????? (PWM: TIM4_CH1/PB6, ??: PB13)
    if (moto_right >= 0) {
        R_IA = 0;                        // ?????
        PWMA = abs(moto_right);          // ?????
    } else {
        R_IA = 1;                        // ?????
        PWMA = PWM_MAX - abs(moto_right);// ???????
    }
}
