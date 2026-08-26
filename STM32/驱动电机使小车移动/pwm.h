#ifndef __PWM_H
#define __PWM_H

#include "sys.h"

// ????????
#define AIN PBout(14)
#define BIN PBout(13)
#define PWMA TIM4->CCR1
#define PWMB TIM4->CCR2

// ????
u32 myabs(long int a);
void Motor_Init(void);
void PWM_Init(u16 arr, u16 psc);
void Set_Pwm(int moto1, int moto2);

#endif
