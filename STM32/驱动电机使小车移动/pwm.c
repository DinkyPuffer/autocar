#include "pwm.h"

// 1. ???????????
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ?? PB ????
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_13; // PB13, PB14 ??
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        // ????
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 50MHz
    GPIO_Init(GPIOB, &GPIO_InitStructure);                  // ??? GPIOB
    
    // ?? AIN ? BIN ?????? GPIO ???
    // ???????? AIN/BIN ???,????????:
    // AIN = 0; 
    // BIN = 0;
}

// 2. ???????? PWM ??
void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    Motor_Init(); // ????????????

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);  // ?? TIM4 ??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ?? GPIOB ??

    // ?? PB6, PB7 ???????(?? TIM4_CH1 ? TIM4_CH2 ?? PWM)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ????? TIM4 ?????
    TIM_TimeBaseStructure.TIM_Period = arr;                     // ??(ARR ????)
    TIM_TimeBaseStructure.TIM_Prescaler = psc;                 // ?????(PSC)
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;               // ????
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;// ????
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // ?? PWM ??(????)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             // PWM ?? 1
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;// ????
    TIM_OCInitStructure.TIM_Pulse = 0;                             // ?????? 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     // ?????
    
    TIM_OC1Init(TIM4, &TIM_OCInitStructure); // ????? 1 (PB6)
    TIM_OC2Init(TIM4, &TIM_OCInitStructure); // ????? 2 (PB7)

    TIM_CtrlPWMOutputs(TIM4, ENABLE); // ?????

    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable); // ?? 1 ???
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable); // ?? 2 ???

    TIM_ARRPreloadConfig(TIM4, ENABLE); // ?? ARR ???
    TIM_Cmd(TIM4, ENABLE);              // ????? TIM4
}
u32 myabs(long int a)
{
    u32 temp;
    if(a < 0
)
        temp = -a;
    else
        temp = a;
    
    return
 temp;
}
void Set_Pwm(int moto1, int moto2)
{
    // ??/?????PWM?? (? moto2 ??)
    if(moto2 >= 0) {
        AIN = 0;
        PWMA = myabs(moto2);
    } else {
        AIN = 1;
        PWMA = 7199 - myabs(moto2);
    }

    // ??/?????PWM?? (? moto1 ??)
    if(moto1 >= 0) {
        BIN = 0;
        PWMB = myabs(moto1);
    } else {
        BIN = 1;
        PWMB = 7199 - myabs(moto1);
    }
}
