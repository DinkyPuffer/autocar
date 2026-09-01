#include "sys.h"
#include "usart.h"
#include "pwm.h"
#include "stm32f10x.h"      // STM32 ????????
#include "stm32f10x_usart.h" // ???????
#include <string.h>          // memset ??????
#include "colorful_led.h"

//////////////////////////////////////////////////////////////////
//¼ÓÈëÒÔÏÂ´úÂë,Ö§³Öprintfº¯Êý,¶ø²»ÐèÒªÑ¡Ôñuse MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//±ê×¼¿âÐèÒªµÄÖ§³Öº¯Êý                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//¶¨Òå_sys_exit()ÒÔ±ÜÃâÊ¹ÓÃ°ëÖ÷»úÄ£Ê½    
_sys_exit(int x) 
{ 
	x = x; 
} 
//ÖØ¶¨Òåfputcº¯Êý 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//Ñ­»··¢ËÍ,Ö±µ½·¢ËÍÍê±Ï   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 


#if EN_USART1_RX   //Èç¹ûÊ¹ÄÜÁË½ÓÊÕ

u8 USART_RX_BUF[USART_REC_LEN];     //½ÓÊÕ»º³å,×î´óUSART_REC_LEN¸ö×Ö½Ú.
u8 USART_RX_STA=0;       //½ÓÊÕ×´Ì¬±ê¼Ç	  
u8 count=0;

// ????????
u8 protocol_buf[6] = {0};
uint8_t USART_RX_COUNT = 0;    // ????????
uint8_t CAR_buff[4] = {0};     // ????????? [0:??A, 1:??A, 2:??B, 3:??B]
uint8_t uart_rec_flag = 0;     // ????????????

// ??????????(???? float ? double ??)
float Target_MotorA = 0.0f;
float Target_MotorB = 0.0f;

void uart_init(u32 bound){
  //GPIO¶Ë¿ÚÉèÖÃ
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);	//Ê¹ÄÜUSART1£¬GPIOAÊ±ÖÓ
  
	//USART1_TX   GPIOA.9
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //PA.9
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//¸´ÓÃÍÆÍìÊä³ö
  GPIO_Init(GPIOA, &GPIO_InitStructure);//³õÊ¼»¯GPIOA.9
   
  //USART1_RX	  GPIOA.10³õÊ¼»¯
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//PA10
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//¸¡¿ÕÊäÈë
  GPIO_Init(GPIOA, &GPIO_InitStructure);//³õÊ¼»¯GPIOA.10  

  //Usart1 NVIC ÅäÖÃ
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;//ÇÀÕ¼ÓÅÏÈ¼¶3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		//×ÓÓÅÏÈ¼¶3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQÍ¨µÀÊ¹ÄÜ
	NVIC_Init(&NVIC_InitStructure);	//¸ù¾ÝÖ¸¶¨µÄ²ÎÊý³õÊ¼»¯VIC¼Ä´æÆ÷
  
   //USART ³õÊ¼»¯ÉèÖÃ

	USART_InitStructure.USART_BaudRate = bound;//´®¿Ú²¨ÌØÂÊ
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//×Ö³¤Îª8Î»Êý¾Ý¸ñÊ½
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//Ò»¸öÍ£Ö¹Î»
	USART_InitStructure.USART_Parity = USART_Parity_No;//ÎÞÆæÅ¼Ð£ÑéÎ»
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//ÎÞÓ²¼þÊý¾ÝÁ÷¿ØÖÆ
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//ÊÕ·¢Ä£Ê½

  USART_Init(USART1, &USART_InitStructure); //³õÊ¼»¯´®¿Ú1
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//¿ªÆô´®¿Ú½ÓÊÜÖÐ¶Ï
  USART_Cmd(USART1, ENABLE);                    //Ê¹ÄÜ´®¿Ú1 

}

// ????????????? PWM ??
void CalculateAndControlMotors(float Target_MotorA, float Target_MotorB)
{
    // 1. ? float ??????? PWM ??(??? 7200 ?,?? 1.50 ?? PWM 7200)
    int motoA_pwm = (int)(Target_MotorA * 4800); 
    int motoB_pwm = (int)(Target_MotorB * 4800); 

    // 2. ????,?? PWM ????? 7199
    if (motoA_pwm > 7199)  motoA_pwm = 7199;
    if (motoA_pwm < -7199) motoA_pwm = -7199;
    if (motoB_pwm > 7199)  motoB_pwm = 7199;
    if (motoB_pwm < -7199) motoB_pwm = -7199;

    // 3. ??? pwm.c ????????
    // ??:?? motoA ?? moto1,motoB ?? moto2;???????????????
    Set_Pwm(motoA_pwm, motoB_pwm); 
}

void USART1_IRQHandler(void)                    //??1??????
{
    u8 Res;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        Res =USART_ReceiveData(USART1); //????????
        protocol_buf[USART_RX_COUNT]=Res;

        if(protocol_buf[0]==0xFC)            //????
            USART_RX_COUNT++;
        else
            USART_RX_COUNT=0;

        if(protocol_buf[5]==0xFD)            //????    ????
        {
            USART_RX_COUNT=0;
            CAR_buff[0]=protocol_buf[1];    //??A
            CAR_buff[1]=protocol_buf[2];    //??A
            CAR_buff[2]=protocol_buf[3];    //??B
            CAR_buff[3]=protocol_buf[4];    //??B
            memset(protocol_buf, 0, 6);
            uart_rec_flag=1;                 //?????
        }

        USART_ClearFlag(USART1, USART_FLAG_RXNE);    //???????
    }
}

void UART_Protocol_Control(void)
{
/******???????********/
if(uart_rec_flag)                    //??????
{
Target_MotorA=CAR_buff[1]/100.00; //???????
Target_MotorB=CAR_buff[3]/100.00; //???????

//?????
if(CAR_buff[0]==1) {
Target_MotorA=-1*Target_MotorA;
}
if(CAR_buff[2]==1) {
Target_MotorB=-1*Target_MotorB;
}

//???
if(CAR_buff[0]==1 && CAR_buff[2]==1) {
    R_led_mode();
}else{
R_led_CLC();
}

uart_rec_flag=0;
memset(CAR_buff, 0, 4); //?? ???????
}
CalculateAndControlMotors(Target_MotorA, Target_MotorB); // ???? pid??
}



void uart2_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. ?? USART2 ? GPIOA ?? (??:USART2 ? APB1 ??)
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

    // 4. ?? USART2 ?? NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // ?????
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // ????
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. USART2 ????
    USART_InitStructure.USART_BaudRate = bound;               // ??? (NFC ????? 115200 ? 9600)
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

