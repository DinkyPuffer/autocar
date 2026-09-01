#include "sys.h"
#include "usart.h"
#include "pwm.h"
#include "stm32f10x.h"      
#include "stm32f10x_usart.h" 
#include <string.h>          
#include "colorful_led.h"

//////////////////////////////////////////////////////////////////
// ??????, ?? printf ??, ?????? use MicroLIB     
#if 1
#pragma import(__use_no_semihosting)             
struct __FILE 
{ 
    int handle; 
}; 

FILE __stdout;       
_sys_exit(int x) 
{ 
    x = x; 
} 
int fputc(int ch, FILE *f)
{      
    while((USART1->SR & 0X40) == 0); // ????, ??????    
    USART1->DR = (u8) ch;      
    return ch;
}
#endif 

#if EN_USART1_RX   // ???????

u8 USART_RX_BUF[USART_REC_LEN];     
u8 USART_RX_STA = 0;      
u8 count = 0;

// ??????
u8 protocol_buf[6] = {0};
uint8_t USART_RX_COUNT = 0;    
uint8_t CAR_buff[4] = {0};     // ?????? [0:??A, 1:??A, 2:??B, 3:??B]
volatile u8 uart_rec_flag = 0;     // ???????

// ??????
float Target_MotorA = 0.0f;
float Target_MotorB = 0.0f;

// ???????(?? 100ms ????)
volatile uint32_t uart_timeout_cnt = 0; 

void uart_init(u32 bound){
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
     
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);    

    // USART1_TX PA.9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;    
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // USART1_RX PA.10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);  

    // Usart1 NVIC ??
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            
    NVIC_Init(&NVIC_InitStructure);    
    
    // USART ?????
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;    

    USART_Init(USART1, &USART_InitStructure); 
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // ??????
    USART_Cmd(USART1, ENABLE);                     // ????1 
}

// ????? PWM ????
void CalculateAndControlMotors(float Target_MotorA, float Target_MotorB)
{
    int motoA_pwm = (int)(Target_MotorA * 4800); 
    int motoB_pwm = (int)(Target_MotorB * 4800); 

    // ??
    if (motoA_pwm > 7199)  motoA_pwm = 7199;
    if (motoA_pwm < -7199) motoA_pwm = -7199;
    if (motoB_pwm > 7199)  motoB_pwm = 7199;
    if (motoB_pwm < -7199) motoB_pwm = -7199;

    Set_Pwm(motoA_pwm, motoB_pwm); 
}

// ??????????
void USART1_IRQHandler(void)                    
{
    u8 Res;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART1); // ????(?????? RXNE ???)

        if (USART_RX_COUNT == 0) {
            // ???? 0xFC
            if (Res == 0xFC) {
                protocol_buf[0] = Res;
                USART_RX_COUNT = 1;
            }
        } 
        else if (USART_RX_COUNT < 5) {
            // ??????(???????)
            protocol_buf[USART_RX_COUNT] = Res;
            USART_RX_COUNT++;
        } 
        else if (USART_RX_COUNT == 5) {
            // ???? 0xFD
            if (Res == 0xFD) {
                protocol_buf[5] = Res;
                CAR_buff[0] = protocol_buf[1];    // ??A
                CAR_buff[1] = protocol_buf[2];    // ??A
                CAR_buff[2] = protocol_buf[3];    // ??B
                CAR_buff[3] = protocol_buf[4];    // ??B
                
                uart_rec_flag = 1; 
                uart_timeout_cnt = 0; // ?????,???????
            }
            USART_RX_COUNT = 0; // ????????,????6????????
        }
    }
}

// ???????????
void UART_Protocol_Control(void)
{
    // 1. ??????(???? 100ms ????,??? > 5 ???? 0.5s ???????)
    uart_timeout_cnt++;
    if (uart_timeout_cnt > 5)
    {
        Target_MotorA = 0.0f;
        Target_MotorB = 0.0f;
        CalculateAndControlMotors(0, 0); // ?????????,??????
        return;
    }

    // 2. ???????
    if(uart_rec_flag)                    
    {
        Target_MotorA = CAR_buff[1] / 100.00f; 
        Target_MotorB = CAR_buff[3] / 100.00f; 

        // ????(1 ???,0 ???)
        if(CAR_buff[0] == 1) {
            Target_MotorA = -1.0f * Target_MotorA;
        }
        if(CAR_buff[2] == 1) {
            Target_MotorB = -1.0f * Target_MotorB;
        }

        // ??/???????
        if(CAR_buff[0] == 1 && CAR_buff[2] == 1) {
            R_led_mode();
        } else {
            R_led_CLC();
        }

        uart_rec_flag = 0; // ?????
    }

    // 3. ??????
    CalculateAndControlMotors(Target_MotorA, Target_MotorB); 
}

void uart2_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;               
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);            
    USART_Cmd(USART2, ENABLE);                                 
}

#endif
