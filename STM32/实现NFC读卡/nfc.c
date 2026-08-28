#include "nfc.h"
#include "string.h"
#include "colorful_led.h"

// ????
u8 const NFC_WakeUp[] = {0x55,0x55,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xFF,0x03,0xFD,0xD4,0x14,0x01,0x17,0x00};   //????
u8 const NFC_SearchCard[] = {0x00,0x00,0xFF,0x04,0xFC,0xD4,0x4A,0x01,0x00,0xE1,0x00};   //??

// ??????????
u8 NFC_WakeUp_Ok = 0;           //NFC????
u8 NFC_find_Card = 0;           //NFC?????
u8 NFC_sendcmd_find = 1;        //NFC???????
u8 NFC_wait_Card = 0;
u8 NFC_read_id_flag=0;
u8 NFC_DataBlock[16];           //????BLOCK???
u8 USART2_RX_BUF[USART2_REC_LEN];     //????,??USART2_REC_LEN???.
u16 USART2_RX_STA=0;            //??????
u16 slen;                       //??????
u8 Sys_Stat;                    //nfc id???
u8 Sum = 0;                     //???
u8 REC_LEN=0;
u8 led_flag=0;
UART_Frame_TypeDef UART2Frame;

void FoundCard_Handler(void)        
{
    NFC_find_Card = 0;    
    
    if(led_flag == 0)     
    {
        led_flag = 1;
        R_led_mode();
    }
    else
    {
        led_flag = 0;
        R_led_CLC();
    }

    // ?????????????????
    UART2Frame.RxCounter = 0;
    NFC_sendcmd_find = 1; // ?????????
    delay_ms(200);
}
void UART2SendFrame(u8 *buf, u16 len)
{
    u16 i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(USART2, buf[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET); // ????????
    }
}

void NFC_Handler(void)
{
    if(NFC_WakeUp_Ok) // ???
    {
        if(NFC_find_Card == 1)  // ????
        {
            FoundCard_Handler();
        }
        else if(NFC_find_Card == 0 && NFC_sendcmd_find == 1)
        {
            UART2Frame.RxCounter = 0;
            // ??????
            UART2SendFrame((u8*)NFC_SearchCard, sizeof(NFC_SearchCard));
            NFC_sendcmd_find = 0;
            delay_ms(200);
        }
    }
    else // ????????????,???????
    {
        UART2Frame.RxCounter = 0;
        UART2SendFrame((u8*)NFC_WakeUp, sizeof(NFC_WakeUp));
        delay_ms(200);
    }
}
void put_HEX(USART_TypeDef* USARTx, u8 *buf, u16 len)
{
    u16 i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(USARTx, buf[i]);
        while(USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
    }
}

void USART2_IRQHandler(void)
{
    u8 i;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) // ????
    {
        UART2Frame.RxBuffer[ UART2Frame.RxCounter ] = USART_ReceiveData(USART2);

        if(NFC_WakeUp_Ok == 0) // ???
        {
            UART2Frame.RxCounter++;
            if(UART2Frame.RxCounter == 15)
            {
                memcpy(USART2_RX_BUF, (uint8_t*)UART2Frame.RxBuffer, 15);
                memset((uint8_t*)UART2Frame.RxBuffer, 0, 20);
                UART2Frame.RxCounter = 0;
                NFC_WakeUp_Ok = 1;
            }
        }
        else // ???,?? 25 ??????
        {
            UART2Frame.RxCounter++;
            if(UART2Frame.RxCounter >= 25) // ??????,? >= 25
            {
                memcpy(USART2_RX_BUF, (uint8_t*)UART2Frame.RxBuffer, 25);
                
                // ?? 16 ????
                printf("NFC Raw: ");
                for(i = 0; i < 25; i++)
                {
                    printf("%02X ", USART2_RX_BUF[i]);
                }
                printf("\r\n");

                // ????
                if(
                    ((0x7A==USART2_RX_BUF[19])&&(0xC1==USART2_RX_BUF[20])&&(0x43==USART2_RX_BUF[21])&&(0x06==USART2_RX_BUF[22]))
                    ||((0x50==USART2_RX_BUF[19])&&(0x84==USART2_RX_BUF[20])&&(0xFC==USART2_RX_BUF[21])&&(0x23==USART2_RX_BUF[22])) // <--- ?? 19
                    ||((0x40==USART2_RX_BUF[19])&&(0x74==USART2_RX_BUF[20])&&(0x80==USART2_RX_BUF[21])&&(0x23==USART2_RX_BUF[22]))
                )
                {
                    NFC_find_Card = 1;
                }
                else
                {
                    // ?????????,????????????,????????
                    NFC_sendcmd_find = 1; 
                }
                
                memset((uint8_t*)UART2Frame.RxBuffer, 0, 50);
                memset((uint8_t*)USART2_RX_BUF, 0, 50);
                UART2Frame.RxCounter = 0;
            }
        }
    }
}
void NFC_Init(void)
{
    // ??????,???/???????
    UART2SendFrame((u8*)NFC_WakeUp, sizeof(NFC_WakeUp));
    delay_ms(100);
}


