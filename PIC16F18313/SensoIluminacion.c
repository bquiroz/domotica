/*
 * File:   SensoIluminacion.c
 * Author: bquiroz
 * 
 * Esta aplicacion sirve para notificar los cambios en la iluminacion 
 * que recibe el uC de una foto-resistencia, lo mejor seria poder hacerlo
 * para que todas las terminales disponibles puedan notificar su valor 
 *
 * Created on 15 de mayo de 2016, 10:02 PM
 */


#include <xc.h>

#define _XTAL_FREQ 32000000 // El reloj del pic corre a esta velocidad por default
#define RETRASO 500

#pragma config WDTE = OFF       // Watchdog Timer Enable bits (WDT disabled; SWDTEN is ignored)
#pragma config FEXTOSC = OFF    // FEXTOSC External Oscillator mode Selection bits (Oscillator not enabled)
//#pragma config RSTOSC = HFINT32  // Power-up default value for COSC bits (HFINTOSC with 2x PLL (32MHz))
#pragma config LVP = OFF        // Low Voltage Programming Enable bit (HV on MCLR/VPP must be used for programming.)


#define msgID        0b00011111
#define chSiHayLuz   0b10001111
#define chNoHayLuz   0b10000111
#define chFltStatLuz 0b11000011
#define chFltUmbrLuz 0b11100011
#define chMalFiltro  0b11100001
#define chUmbralLuz  0b00001000

//const unsigned char mensaje[9] = {0b00000111,0b00000111,0b00000111,0b00000111,0b00000111,0b00000111,0b00000111,0b00000111};
//const char mensaje[9]  = {0b00000111,0b00001110,0b00011100,0b00111000,0b01110000,0b11100000,0b11000001,0b10000011};

const char mensaje[10] = {0b00001111,0b11110000,0b00111100,
                          0b11000011,0b01010101,0b10101010,
                          0b11111111,0b11001100,0b00110011,
                          0b00000000};
char msjEnviar[9]; 
char msjRecibido[9];
int inRecibido = 0;

void UART_Init(void)
{
//  -------------------------------------------------  
    // Configuramos el UART
    TRISAbits.TRISA0 = 0; // Se habilita el terminal RA0 como salida
    TRISAbits.TRISA1 = 1; // Se habilita el terminal RA1 como entrada
    ANSELAbits.ANSA1 = 0; // Clear the ANSEL bit for the Rx pin (if applicable)
    ANSELAbits.ANSA0 = 0; // Clear the ANSEL bit for the Tx pin (if applicable)
    RA0PPS  = 0b00010100; // Ajusta el pin RA0 como salida para TX - DS40001799A-page 141
    
    // Configuracion de la transmision
    TX1STAbits.TXEN    =   1;  // Habilita el envio por el uart
    TX1STAbits.SYNC    =   0;  // Modo asincrono
    TX1STAbits.TX9     =   0;  // Envia solo 8 bits
    TX1STAbits.BRGH    =   0;  // Baja velocidad 
    BAUD1CONbits.BRG16 =   1;  // No usa el generador de BAUD de 16 bits
    SPBRGbits.SP1BRGL  = 191; // 10,417 baudios de velocidad
//    BAUD1CONbits.ABDEN = 1; // Auto-Baud Detect Enable bit 
        
    // Configuracion de la recepcion
    RC1STAbits.SPEN = 1; // Habilita la recepcion
    RC1STAbits.CREN = 1; // Habilita la recepcion continua
}

void UART_Write(char data)
{
  while(!TRMT);
  TXREG = data;
}

void enviaMensaje(char estado)
{
    int i;
    msjEnviar[0] = msgID;
    msjEnviar[1] = 0b00000011;
    msjEnviar[2] = chFltStatLuz;
    msjEnviar[3] = estado;
    msjEnviar[4] = 0b00000000;

    // Espera a poder enviar informacion
    while(!TRMT);
    
    for( i = 0 ; i < 5 ; i++ )
        TXREG = msjEnviar[i];
}

void ADC_Init()
{
//  -------------------------------------------------  
    // Configuramos el convertidor analogico
    TRISAbits.TRISA5 = 1; // Habilita el pin RA5 como entrada, todos los demas son salida
    ANSELAbits.ANSA5 = 1; // Se utiliza el RA5 como entrada Analogica, las demas quedan como estaban
    ADCON0 = 0b00010100; // ANA5 para utilizar el convertidor en RA5
    ADCON1 = 0b00000100; // Justificado a la Derecha utiliza FOSC/4    
}

int ADC_Read()
{
// Activamos, convertimos y desactivamos el ADC, hay que dar tiempo para que se habilite 30us
    ADCON0bits.ADON = 1;
    __delay_us(30);
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO == 1) 
    __delay_us(10);
    ADCON0bits.ADON = 0;
    return ADRESHbits.ADRESH;
}

void main() 
{
    int i;
    unsigned char chTemp = 0b00000000;
    char chNotificar = 'N';
    unsigned char chUmbrLuz = chUmbralLuz;
    unsigned char chTmpStat = chNoHayLuz;
    unsigned char chStatLuz = chNoHayLuz;

// Limpieza del Tri-estado, en cada etapa de la configuracion se cambiara el valor de este registro
    TRISA  = 0b00000000; 

    // Configuracion  de los dispositivos.
    ADC_Init();
    UART_Init();
 
//------------------------------------------------    
    // Aviso que vamos al bucle principal - Enciende y apaga el RA4
    PORTA = 0b00000100;

//  -------------------------------------------------  
    // El bucle principal
    while(1)
    {
        // Obtenemos el valor de si hay luz o no
        if(ADC_Read() > chUmbrLuz)
        {
            chStatLuz = chSiHayLuz ;
            PORTA = LATA | 0b00010000;            
        }
        else
        {
            chStatLuz = chNoHayLuz ;
            PORTA = LATA & 0b00000111;                        
        }

        // Evaluamos si hubo cambio en el valor de la luz
        chNotificar =(chStatLuz == chTmpStat)? 'N' : 'S' ;
        // Volvemos a colocar los dos valores iguales
        chTmpStat = chStatLuz;

        // Si hubo cambio en las condiciones de la luz notificamos
        if (chNotificar == 'S')
        {
//            chTemp = chTemp == 0b00000000? 0b00000001 : chTemp++;
//            TXREG = chTemp++; //mensaje[i];
            TXREG = msgID;
            __delay_ms(300);
            TXREG = chStatLuz; //mensaje[i];
            i = i > 9? 0 : i++;
            
//            enviaMensaje(chStatLuz);        
        }
        __delay_ms(10);
    }
}
