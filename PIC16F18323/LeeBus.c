/*
 * File:   LeeBus.c
 * Author: bquiroz
 *
 * Created on 15 de mayo de 2016, 10:07 PM
 */

#include <xc.h>
//#include <pps.h>

#define _XTAL_FREQ 32000000 // El reloj del pic corre a esta velocidad por default
#define RETRASO 100

#pragma config WDTE = OFF       // Watchdog Timer Enable bits (WDT disabled; SWDTEN is ignored)
#pragma config FEXTOSC = OFF    // FEXTOSC External Oscillator mode Selection bits (Oscillator not enabled)
//#pragma config RSTOSC = HFINT32  // Power-up default value for COSC bits (HFINTOSC with 2x PLL (32MHz))

int inRecibido;

unsigned char chMsjRecibido[9];
//unsigned char chBufferEnvio[8] ={0x30,0x31,0x32,0x33,0x34,0x35,0x36};

const unsigned char chAcciones[8] = {0b11000001,0b10000011,0b00000111,0b00001110,0b00011100,0b00111000,0b01110000,0b11100000};

const unsigned char chPatron01[9] = {0b11000001,0b10000011,0b00000111,0b00001110,0b00011100,0b00111000,0b01110000,0b11100000,0b00000000};
const unsigned char chPatron02[9] = {0b10000011,0b11000001,0b11100000,0b01110000,0b00111000,0b00011100,0b00001110,0b00000111,0b00000000};

//const unsigned char chMsjAccion
//const unsigned char chMsj

char UART_Init(const long int baudrate)
{
   unsigned int x;
   x = (_XTAL_FREQ - baudrate*64)/(baudrate*64);
   if(x>255)
   {
      x = (_XTAL_FREQ - baudrate*16)/(baudrate*16);
      BRGH = 1;
   }
   if(x<256)
   {
     SPBRG  = x;
     SYNC   = 0;
     SPEN   = 1;
     TRISA0 = 1; 
     TRISA1 = 1;
     CREN   = 1;
     TXEN   = 1;
     return 1;
   }
   return 0;
}

char UART_TX_Empty()
{
  return TRMT;
}

char UART_Data_Ready()
{
   return RCIF;
}

char UART_Read()
{
 
  while(!RCIF);
  return RCREG;
}

void UART_Read_Text(char *Output, unsigned int length)
{
   unsigned int i;
   for( i=0;i<length;i++)
      Output[i] = UART_Read();
}

void UART_Write(char data)
{
  while(!TRMT);
  TXREG = data;
}

void UART_Write_Text(char *text)
{
  int i;
  for(i=0;text[i]!='\0';i++)
     UART_Write(text[i]);
}

//void interrupt decod_ISR(void)
void decod_ISR(void)
{
    int i;
    //reset the interrupt flag
    for( i = 0 ; PIR1bits.RCIF && (i < 9); i++ )  
        chMsjRecibido[i] = RC1REGbits.RC1REG;
    
    i++;
    chMsjRecibido[i] = 0b00000000;
    inRecibido = 1;
    return;
}

void intRecibe()
{
    return;
}

void muestraPuerto(unsigned char chValor)
{
    unsigned char chTempRA, chTempRC;
    chTempRA = ( chValor << 1 ) & 0b00110000; // Recorre 1 bit a la izquierda y hace 0 todo lo demas
    chTempRA = chTempRA | (chValor & 0b00000111); // Hacer 0 el bit 4 queda lista la salida del RA
    chTempRC = ( chValor >> 5 )& 0b00000111 ; // Hacer 0 a partir del bit 4 queda lista la salida del RC
    PORTA    = chTempRA;
    PORTC    = chTempRC;
}

void rutinaLED(unsigned char *chPatron)
{
    int i;
    for( i = 0 ; chPatron[i] != 0b00000000 ; i++ )
    {
        muestraPuerto( chPatron[i] );
        __delay_ms(90);
    }
}

void main() 
{
    int i;
    // Limpieza del Tri-estado, en cada etapa de la 
    // configuracion se cambiara el valor de este registro
    TRISA  = 0b00000000; 
    TRISC  = 0b00000000; 

//  -------------------------------------------------  
    // Configuramos el UART
    TRISCbits.TRISC4 = 0 ; // Se habilita el terminal RC4 como salida
    TRISCbits.TRISC5 = 1 ; // Se habilita el terminal RC5 como entrada
    RC4PPSbits.RC4PPS = 0b00010100; // Ajusta el pin RC5 como salida para TX - DS40001799A-page 141
    ANSELCbits.ANSC5 = 0; // Clear the ANSEL bit for the RX pin (if applicable)
    RC4PPS = 0b00010100; // Ajusta el pin RC4 como salida para TX - DS40001799A-page 141

    // Configuracion de la transmision
    TX1STAbits.TXEN    =   1; // Habilita el envio por el uart
    TX1STAbits.SYNC    =   0; // Modo asincrono
    TX1STAbits.TX9     =   0; // Envia solo 8 bits
    TX1STAbits.BRGH    =   0; // Alta velocidad 
    BAUD1CONbits.BRG16 =   1; // Se usa el generador de BAUD de 16 bits
    SPBRGbits.SP1BRGL  = 191; // 10,417 kbaudios velocidad
//    BAUD1CONbits.ABDEN  =   1; // Auto-Baud Detect Enable bit 
//    BAUD1CONbits.ABDOVF =   1;
        
    // Configuracion de la recepcion
    RC1STAbits.SPEN = 1; // Habilita la recepcion
    RC1STAbits.CREN = 1; // Habilita la recepcion continua

//  -------------------------------------------------  
    // Aviso que vamos al bucle principal - Enciende y apaga el RA4
    PORTA  = 0b00110111; PORTC  = 0b00001111; __delay_ms(1000); // 5 Second Delay
    PORTA  = 0b00000000; PORTC  = 0b00001000; //__delay_ms(1000); // 5 Second Delay
   
//  -------------------------------------------------  
    // Antes de entrar al bucle principal, habilitamos las interrupciones globales.
    INTCONbits.GIE  = 1; // Interrupciones globales activadas
    INTCONbits.PEIE = 1; // Interrupciones de perifericos activadas
    PIE1bits.RCIE   = 1; // Se habilita notificacion de interrupcion por Rx
    
    inRecibido = 0;
    
    // El bucle principal
    while(1)
    {
        PORTC = LATC | 0b00001000;
        PORTC = LATC & 0b00001000;
        PORTA = LATA & 0b00000000;
    
        inRecibido = 1;
        for( i = 0 ; PIR1bits.RCIF && (i < 9); i++ )  
            chMsjRecibido[i] = RC1REGbits.RC1REG;
    
        i++;
        chMsjRecibido[i] = 0b00000000;

        if( inRecibido == 1)
        {    
//            rutinaLED(chBufRecibido); 
            for( i = 0 ; chMsjRecibido[i] != 0b00000000 ; i++ )
            {
                muestraPuerto(chMsjRecibido[i]);
//                __delay_ms(600);
            }
            inRecibido = 0; 
        }
    }
}
