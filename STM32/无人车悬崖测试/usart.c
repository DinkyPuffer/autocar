#include "sys.h"
#include "usart.h"
#include "pwm.h"
#include "stm32f10x.h"
#include "stm32f10x_usart.h"
#include <string.h>
#include "colorful_led.h"

//////////////////////////////////////////////////////////////////
// ??????,?? printf ??,?????? use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
// ??????????                
struct __FILE 
{ 
	int handle; 
}; 

FILE __stdout;       
// ?? _sys_exit() ??????????    
void _sys_exit(int x) 
{ 
	x = x; 
} 
// ??? fputc ?? 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR & 0X40) == 0); // ????,??????   
	USART1->DR = (u8) ch;      
	return ch;
}
#endif 


#if EN_USART1_RX   // ???????

u8 USART_RX_BUF[USART_REC_LEN]; // ?????,?? USART_REC_LEN ???.
u8 USART_RX_STA = 0;           // ??????	  
u8 count = 0;

// ????????
u8 protocol_buf[6] = {0};
uint8_t USART_RX_COUNT = 0;    // ????

uint8_t CAR_buff[4] = {0};     // ?????? [0:??A??, 1:??A??, 2:??B??, 3:??B??]
volatile uint8_t uart_rec_flag = 0; // ??????,?? volatile ???????

// ??????(?? float ? PID ? PWM ??)
float Target_MotorA = 0.0f;
float Target_MotorB = 0.0f;

void uart_init(u32 bound)
{
    // GPIO????
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
     
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE); // ??USART1,GPIOA??
  
    // USART1_TX GPIOA.9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; // PA.9
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // ??????
    GPIO_Init(GPIOA, &GPIO_InitStructure); // ???GPIOA.9
   
    // USART1_RX GPIOA.10???
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // PA10
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // ????
    GPIO_Init(GPIOA, &GPIO_InitStructure); // ???GPIOA.10  

    // Usart1 NVIC ??
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // ?????3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // ????3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // IRQ????
    NVIC_Init(&NVIC_InitStructure); // ??????????VIC???
  
    // USART ?????
    USART_InitStructure.USART_BaudRate = bound; // ?????
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // ???8?????
    USART_InitStructure.USART_StopBits = USART_StopBits_1; // ?????
    USART_InitStructure.USART_Parity = USART_Parity_No; // ??????
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // ????????
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // ????

    USART_Init(USART1, &USART_InitStructure); // ?????1
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // ????????
    USART_Cmd(USART1, ENABLE);                     // ????1 
}

// ??????? PWM ??
void CalculateAndControlMotors(float Target_MotorA, float Target_MotorB)
{
    // 1. ? float ??? PWM ??
    int motoA_pwm = (int)(Target_MotorA * 4800); 
    int motoB_pwm = (int)(Target_MotorB * 4800); 

    // 2. ????,?? PWM ?????? 7199
    if (motoA_pwm > 7199)  motoA_pwm = 7199;
    if (motoA_pwm < -7199) motoA_pwm = -7199;
    if (motoB_pwm > 7199)  motoB_pwm = 7199;
    if (motoB_pwm < -7199) motoB_pwm = -7199;

    // 3. ?? pwm.c ????????
    Set_Pwm(motoA_pwm, motoB_pwm); 
}

// ??1??????(??????????)
void USART1_IRQHandler(void)                    
{
    u8 Res;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART1); // ????(??????? RXNE ???)

        // ?????????? [0xFC, D0, D1, D2, D3, 0xFD]
        if (USART_RX_COUNT == 0)
        {
            if (Res == 0xFC) // ????????? 0xFC ?????
            {
                protocol_buf[0] = Res;
                USART_RX_COUNT = 1;
            }
        }
        else if (USART_RX_COUNT < 5)
        {
            protocol_buf[USART_RX_COUNT] = Res;
            USART_RX_COUNT++;
        }
        else if (USART_RX_COUNT == 5)
        {
            protocol_buf[5] = Res;
            if (protocol_buf[5] == 0xFD) // ???? 0xFD
            {
                CAR_buff[0] = protocol_buf[1]; // ??A??
                CAR_buff[1] = protocol_buf[2]; // ??A??
                CAR_buff[2] = protocol_buf[3]; // ??B??
                CAR_buff[3] = protocol_buf[4]; // ??B??
                
                uart_rec_flag = 1;             // ????,???????
            }
            USART_RX_COUNT = 0; // ????????,?6??????,????
        }
    }
}

void UART_Protocol_Control(void)
{
    /****** ????????? ******/
    if (uart_rec_flag)                    
    {
        Target_MotorA = CAR_buff[1] / 100.00f; // ???????
        Target_MotorB = CAR_buff[3] / 100.00f;

        // ????
        if (CAR_buff[0] == 1) {
            Target_MotorA = -1.0f * Target_MotorA;
        }
        if (CAR_buff[2] == 1) {
            Target_MotorB = -1.0f * Target_MotorB;
        }

        // ?????
        if (CAR_buff[0] == 1 && CAR_buff[2] == 1) {
            R_led_mode();
        } else {
            R_led_CLC();
        }

        uart_rec_flag = 0; // ???????
    }
    
    CalculateAndControlMotors(Target_MotorA, Target_MotorB); // ??????
}

void uart2_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. ?? USART2 ? GPIOA ?? (??:USART2 ? APB1 ???)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. USART2_TX (PA2) ??????
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. USART2_RX (PA3) ????
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. ?? USART2 ? NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. USART2 ?????
    USART_InitStructure.USART_BaudRate = bound;               
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);            // ????2????
    USART_Cmd(USART2, ENABLE);                                 // ????2
}

#endif

