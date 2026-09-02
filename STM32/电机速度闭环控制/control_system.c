#include "control_system.h"
#include "pwm.h"
#include "stdio.h"

int L_coder, R_coder;
int Motor_A, Motor_B;     
int OverflowTime = 100;           // ???? 100ms
volatile uint8_t control_flag = 0; // ???????

/******************************************************************
??: ????? PI ??? (?? A / ???)
******************************************************************/
int Incremental_PI_A(int Encoder, int Target)
{
    // 1. ??????????????? (?? C89 ????)
    float Velocity_KP = 7.0f;
    float Velocity_KI = 0.16f;
    float Velocity_KD = 0.03f;
    
    static int Pwm_A = 0;
    static float Error_A = 0, Error_prev_A = 0, Error_prev2_A = 0;
    float p_out, i_out, d_out;

    // 2. ?????
    Error_A = (float)(Target - Encoder); // ????

    // ????? PID ??
    p_out = Velocity_KP * (Error_A - Error_prev_A);
    i_out = Velocity_KI * Error_A;
    d_out = Velocity_KD * (Error_A - 2.0f * Error_prev_A + Error_prev2_A);

    Pwm_A += (int)(p_out + i_out + d_out); // ????

    // PWM ????
    if (Pwm_A > PWM_MAX)  Pwm_A = PWM_MAX;
    if (Pwm_A < -PWM_MAX) Pwm_A = -PWM_MAX;

    // ??????
    Error_prev2_A = Error_prev_A;
    Error_prev_A = Error_A;

    return Pwm_A; 
}

/******************************************************************
??: ????? PI ??? (?? B / ???)
******************************************************************/
int Incremental_PI_B(int Encoder, int Target)
{
    // 1. ???????????
    float Velocity_KP = 7.0f;
    float Velocity_KI = 0.16f;
    float Velocity_KD = 0.03f;
    
    static int Pwm_B = 0;
    static float Error_B = 0, Error_prev_B = 0, Error_prev2_B = 0;
    float p_out, i_out, d_out;

    // 2. ?????
    Error_B = (float)(Target - Encoder);

    p_out = Velocity_KP * (Error_B - Error_prev_B);
    i_out = Velocity_KI * Error_B;
    d_out = Velocity_KD * (Error_B - 2.0f * Error_prev_B + Error_prev2_B);

    Pwm_B += (int)(p_out + i_out + d_out);

    if (Pwm_B > PWM_MAX)  Pwm_B = PWM_MAX;
    if (Pwm_B < -PWM_MAX) Pwm_B = -PWM_MAX;

    Error_prev2_B = Error_prev_B;
    Error_prev_B = Error_B;

    return Pwm_B; 
}

/******************************************************************
??: ??????????? (rad/s -> ????)
******************************************************************/
int Rs_To_CPR(float rads)
{
    float cpr = rads * (700.0f * 4.0f) * ((float)OverflowTime / 1000.0f);
    return (int)cpr;
}

/******************************************************************
??: ????????
******************************************************************/
void System_Control(void)
{
    int TageA = Rs_To_CPR(1.0f); 
    int TageB = Rs_To_CPR(1.0f);

    L_coder = Read_Encoder(2);
    R_coder = Read_Encoder(3);

    Motor_A = Incremental_PI_A(L_coder, TageA);
    Motor_B = Incremental_PI_B(R_coder, TageB);

    Set_Pwm(Motor_A, Motor_B);
}

/******************************************************************
??: ???????????
******************************************************************/
void SysTick_Handler(void)
{
    static uint32_t tick_count = 0;
    tick_count++;
    
    if (tick_count >= OverflowTime) 
    {
        tick_count = 0;
        control_flag = 1; 
    }
}
