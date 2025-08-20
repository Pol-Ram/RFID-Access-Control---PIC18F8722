
/*
 * File:   main.c
 * Author: Paul Ramirez
 *
 * Created on 9 janvier 2025, 14:15
 */

//HEADER PIC
#include <p18f8722.h>
#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <stdint.h>
#define _XTAL_FREQ 10000000  // Define the system clock frequency (10 MHz)
#pragma config OSC = HS        // High-Speed Oscillator
#pragma config FCMEN = OFF     // Fail-Safe Clock Monitor Disabled
#pragma config WDT = OFF       // Watchdog Timer Disabled
#pragma config LVP = OFF       // Low-Voltage Programming Disabled

//GLOBAL VARIABLES
char flag_tmr0=0;  // Timeout flag for Timer0 interrupt


//------------------------------------------------------------------------------------
//Function to initialize the Timer0
void int_timer()
{
    T0CONbits.T0CS    = 0;   // Select internal instruction cycle clock
    T0CONbits.T0PS    = 0b001; // Prescaler = 256
    T0CONbits.PSA     = 0;     // Prescaler assigned to Timer0
    T0CONbits.T08BIT  = 0;  // 8-bit mode
    INTCONbits.GIE    = 1;    // Global interrupt enable
    INTCONbits.PEIE   = 1;   // Peripheral interrupt enable
    INTCONbits.TMR0IE = 1; // Enable Timer0 interrupt
    T0CONbits.TMR0ON  = 1;  // Turn on Timer0
}

//Interruption Function
void __interrupt() isr(void)
{
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF)
    {
        flag_tmr0 = 1;  // Set 'flag_tmr0' when timeout occurs
        INTCONbits.TMR0IF = 0;  // Clear the interrupt flag
    }
}
//--------------------------------------------------------------------------------

//Function to initialize UART
void init_uart1(void)
{
    TXSTA1 = 0;
    RCSTA1 = 0;

    RCSTA1bits.CREN = 1;     // Enable continuous receive
    TXSTA1bits.BRGH = 1;     // High-speed baud rate
    RCSTA1bits.ADDEN = 0;    // Disable address detection (not using multi-address mode)
    BAUDCON1bits.BRG16 = 1;

    SPBRG1 = 42;              // Set the baud rate for 57600 with a 10 MHz clock

    TXSTA1bits.TXEN = 1;     // Enable transmitter
    RCSTA1bits.SPEN = 1;     // Enable serial port (RX/TX pins)
    TRISCbits.TRISC6 = 0;    // Set TX as output
    TRISCbits.TRISC7 = 1;    // Set RX as input
}


// UART Write function
void UART_Write(char data)
{
    while (!PIR1bits.TX1IF);   // Wait for the transmit buffer to be empty
    TXREG1 = data;             // Write data to TXREG1 to transmit
}

// UART Read function
char UART_Read(void)
{
    while (!PIR1bits.RC1IF);   // Wait for data to be received
    return RCREG1;             // Return received data
}

//----------------------------------------------------------------------------------

// UART Echo function (returns 0x0F on timeout, else the received byte)
char read_echo(void)
{
    //while (!RCIF && !flag_tmr0);    // Wait for either data or timeout (flag_tmr0 flag)
    while (!PIR1bits.RC1IF && !flag_tmr0);    // Wait for either data or timeout (flag_tmr0 flag)

    if (flag_tmr0==1)
    {
        flag_tmr0 = 0;              // Reset 'toto' after timeout
        return 0x0F;           // Return 0x0F if timeout occurred
    }
    else
    {
        return RCREG1;     // Return the byte from RCREG1 if received
    }
}

// Function: ECHO
char echo(void)
{
    char data;
    unsigned char command = 0x55;  // The echo command byte
    UART_Write(command);            // Send the echo command
    PORTD=0x0F;
    data=read_echo();             // Wait and return the response
    PORTD=0xF0;
    return data;
}



// Function to send a command in RFID module
void SEND_RFID(unsigned char* command, unsigned char length)
{
    for (unsigned char i = 0; i < length; i++)
    {
        UART_Write(command[i]);  // Send each byte of the command
    }
}


//----------------------------------------------------------------------------------

// Function: IDENTITY
void identity(void)
{
    char identity_data[16];
    char i;
    char code;
    char length;
    UART_Write(0x01);  // Send first byte (IDN command)
    UART_Write(0x00);  // Send second byte

    code =  UART_Read();
    length = UART_Read();


    for (i = 0; i < length; i++)
    {
        identity_data[i] = UART_Read();
        //PORTD = identity_data[i];
        //__delay_ms(70); // Wait a bit before sending again (for debugging)
    }
    for (i = 0; i < length; i++)
    {
       // identity_data[i] = UART_Read();
        PORTD = identity_data[i];
        __delay_ms(70); // Wait a bit before sending again (for debugging)
    }
}

void calibration() {
    unsigned char command[16] = {0x07, 0x0E, 0x03, 0xA1, 0x00, 0xF8, 0x01, 0x18, 0x00, 0x20, 0x60, 0x60, 0x00, 0x00, 0x3F, 0x01};
    unsigned char response[3];  // Espacio para respuesta
    unsigned char power_values[] = {0x00, 0xFC, 0x7C, 0x3C, 0x5C, 0x6C, 0x74, 0x70};

    TRISD = 0x00;   // Configurar PORTD como salida
    PORTD = 0x00;   // Inicializar PORTD apagado

    for (int i = 0; i < 8; i++) {
        command[12] = power_values[i];  // Actualizar el valor de potencia
        SEND_RFID(command, 16);        // Enviar comando

        // Leer la respuesta del CR95HF (3 bytes)
        for (int j = 0; j < 3; j++) {
            response[j] = UART_Read();
        }

        // Proyectar el resultado en LEDs
        if (response[2] == 0x02) {
            PORTD = 0x0F;  // Encender los 4 LEDs inferiores (TAG detectado)
        } else if (response[2] == 0x01) {
            PORTD = 0xF0;  // Encender los 4 LEDs superiores (Timeout)
        } else {
            PORTD = 0xFF;  // Encender todos los LEDs (Error inesperado)
        }

        __delay_ms(500);   // Pausa para observar los resultados
    }

    PORTD = 0x00;  // Apagar todos los LEDs al finalizar
}


void select_protocol() {
    unsigned char command[] = {0x02, 0x02, 0x02, 0x00};  // Comando PROTOCOLSELECT
    unsigned char response[2];  // Para guardar la respuesta

    TRISD = 0x00;  // Configurar PORTD como salida
    PORTD = 0x00;  // Inicializar PORTD apagado

    // Enviar comando PROTOCOLSELECT
    SEND_RFID(command, 4);

    // Leer los 2 bytes de respuesta del RFID
    for (int i = 0; i < 2; i++) {
        response[i] = UART_Read();
    }

    // Visualizar resultado en LEDs
    if (response[0] == 0x00) {
        PORTD = 0x03;  // Encender los 2 LEDs inferiores (éxito)
    } else if (response[0] == 0x82 || response[0] == 0x83) {
        PORTD = 0xC0;  // Encender los 2 LEDs superiores (error)
    } else {
        PORTD = 0xFF;  // Encender todos los LEDs (respuesta inesperada)
    }

    __delay_ms(1000);  // Pausa para observar el resultado

    PORTD = 0x00;  // Apagar los LEDs
}



void get_NFC_tag() {
    unsigned char command1[] = {0x04, 0x02, 0x26, 0x07};  // Comando de detección de TAG
    unsigned char response[2];  // Respuesta esperada (2 bytes)

    TRISD = 0x00;  // Configurar PORTD como salida
    PORTD = 0x00;  // Inicializar PORTD apagado

    // Paso 1: Enviar el comando para detectar el TAG
    SEND_RFID(command1, 4);

    // Leer la respuesta del comando de detección
    for (int i = 0; i < 2; i++) {
        response[i] = UART_Read();
    }

    // Verificar si el TAG fue detectado y reflejarlo en los LEDs del PORTD
    if (response[0] == 0x80) {
        PORTD = 0x0A;  // Encender los 4 LEDs inferiores (TAG detectado)
    } else if (response[0] == 0x87) {
        PORTD = 0xA0;  // Encender los 4 LEDs superiores (Timeout o TAG no detectado)
    } else {
        PORTD = 0xAA;  // Encender todos los LEDs (Error inesperado)
    }

    __delay_ms(1000);  // Pausa para observar el resultado en los LEDs
    PORTD = 0x00;      // Apagar todos los LEDs
}



void ouverture() {
    LATD = 0x01;        // Simular activación de motor (PIN específico)
    __delay_us(2000);   // Pulso de 2 ms para abrir
    LATD = 0x00;        // Apagar señal
}

void fermeture() {
    LATD = 0x01;        // Simular activación de motor (PIN específico)
    __delay_us(1000);   // Pulso de 1 ms para cerrar
    LATD = 0x00;        // Apagar señal
}





int main(void) {
    TRISD = 0x00;    // Set PORTD as output (for debugging)
    PORTD = 0;       // Initialize PORTD to 0

    init_uart1();    // Initialize UART
    int_timer();


    char instruction;
    instruction=0;
    while (instruction != 0x55)
    {
       instruction = echo();
       __delay_ms(70);
    }

     identity();   // Call identity function to send the IDN command and read the identity
     __delay_ms(70); // Wait a bit before sending again (for debugging)

     calibration();
     select_protocol();
     get_NFC_tag();



     while (1) {PORTD=0x07;  }

    return 0;
}





























/*
int main(void) {
    unsigned char tag_id[16];     // Arreglo para almacenar el ID del TAG
    unsigned char tag_length = 0; // Longitud del ID del TAG

    init_uart1();    // Inicializar UART
    int_timer();     // Inicializar Timer0

    get_NFC_tag(tag_id, &tag_length);  // Llamar a la función de detección de TAG

    while (1) {
        // Puedes agregar lógica adicional aquí para usar el ID del TAG almacenado
    }

    return 0;
}
*/


/*
 void get_NFC_tag(unsigned char* tag_id, unsigned char* tag_length) {
    unsigned char command1[] = {0x04, 0x02, 0x26, 0x07};  // Comando de detección
    unsigned char command2[] = {0x04, 0x03, 0x93, 0x20, 0x08};  // Comando para leer ID
    unsigned char response[16];  // Respuesta máxima de 16 bytes

    TRISD = 0x00;  // Configurar PORTD como salida
    PORTD = 0x00;  // Inicializar PORTD apagado

    // Paso 1: Enviar comando de detección
    SEND_RFID(command1, 4);

    // Leer la respuesta del comando de detección
    for (int i = 0; i < 2; i++) {
        response[i] = UART_Read();
    }

    // Verificar si el TAG fue detectado (respuestas válidas son `0x80`)
    if (response[0] == 0x80) {
        PORTD = 0x0F;  // Encender los LEDs inferiores para indicar detección exitosa

        // Paso 2: Leer el ID del TAG (comando de anticolisión)
        SEND_RFID(command2, 5);

        // Leer respuesta del TAG (incluye el ID y longitud del TAG)
        for (int i = 0; i < 16; i++) {
            response[i] = UART_Read();
        }

        // Guardar la longitud del TAG y su ID en las variables
        *tag_length = response[1];  // Longitud del ID (segundo byte de la respuesta)
        for (int i = 0; i < *tag_length; i++) {
            tag_id[i] = response[i + 2];  // El ID comienza en el tercer byte
        }

    } else {
        // Si no se detecta el TAG o hay un error
        if (response[0] == 0x87) {
            PORTD = 0xF0;  // Encender LEDs superiores para indicar Timeout
        } else {
            PORTD = 0xFF;  // Encender todos los LEDs para indicar un error inesperado
        }
    }

    __delay_ms(1000);  // Pausa para observar el estado en los LEDs
    PORTD = 0x00;      // Apagar todos los LEDs
}


 */
