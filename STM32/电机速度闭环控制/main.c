#include "stm32f10x.h"
#include "sys.h"
#include "pwm.h"
#include "encoder.h"
#include "nfc.h"

int main(void)
  { 
		RCC->CSR |= 1<<24; 
		Stm32_Clock_Init(9);						//Íâ²¿Ê±ÖÓ8Mhz 9±¶Æµ  8*9= 72mhz±¶Æµ72mhz
		MY_NVIC_PriorityGroupConfig(2);	//=====ÖÐ¶ÏÓÅÏÈ¼¶·Ö×é		
		uart_init(115200);		//=====´®¿Ú³õÊ¼»¯Îª115200
		uart2_init(115200); // ?????2,??? NFC ????
		JTAG_Set(JTAG_SWD_DISABLE);     //=====¹Ø±ÕJTAG½Ó¿Ú
		JTAG_Set(SWD_ENABLE);           //=====´ò¿ªSWD½Ó¿Ú ¿ÉÒÔÀûÓÃÖ÷°åµÄSWD½Ó¿Úµ÷ÊÔ
		Encoder_Init_TIM2();           
    Encoder_Init_TIM3();

		colorful_led_Init();            //=====ìÅ²ÊµÆ³õÊ¼»¯
		PWM_Init(7199, 9);
		SysTick_Config(72000000/1000);
		printf("QSTÇàÈí\r\n");
		/**Ö÷Òª³ÌÐò**/
		NFC_Init();
	while(1)
	{
			//if(USART_RX_STA==1)  //ÅÐ¶Ï½ÓÊÕµ½ÍêÕûÊý¾ÝµãµÆ
			NFC_Handler();
			delay_ms(100);
	}
}
	
	
