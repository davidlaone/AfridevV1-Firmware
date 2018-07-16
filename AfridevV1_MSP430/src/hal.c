/** 
 * @file hal.c
 * \n Source File
 * \n Outpour MSP430 Firmware
 * 
 * \brief Hal support routines
 */

#include "outpour.h"

/*******************************************************************************
*
*  MSP430 GPIO Info
*
*  For ports:
*
*  PxDIR: Direction Registers PxDIR
*  Each bit in each PxDIR register selects the direction of the corresponding I/O
*  pin, regardless of the selected function for the pin. PxDIR bits for I/O pins that
*  are selected for other functions must be set as required by the other function.
*    Bit = 0: The port pin is switched to input direction
*    Bit = 1: The port pin is switched to output direction
*
*  PxOUT:
*  Each bit in each PxOUT register is the value to be output on
*  the corresponding I/O pin when the pin is configured as I/O function,
*  output direction, and the pull-up/down resistor is disabled.
*    Bit = 0: The output is low
*    Bit = 1: The output is high
*  If the pin’s pull-up/down resistor is enabled, the corresponding bit
*  in the PxOUT register selects pull-up or pull-down.
*    Bit = 0: The pin is pulled down
*    Bit = 1: The pin is pulled up
*
*  PxREN:  Pull-Up/Down Resistor Enable Registers
*  Each bit in each PxREN register enables or disables the pullup/pulldown
*  resistor of the corresponding I/O pin. The corresponding bit in the PxOUT
*  register selects if the pin is pulled up or pulled down.
*     Bit = 0: Pullup/pulldown resistor disabled
*     Bit = 1: Pullup/pulldown resistor enabled
*
*  PxSEL: PxSEL and PxSEL2 bit are used to select the pin function -
*         I/O port or peripheral module function (00 = I/O Port).
*
******************************************************************************/

/**
* \brief One time init of all clock related subsystems after 
*        boot up.
* \ingroup PUBLIC_API
*/
void hal_sysClockInit(void) {
    DCOCTL = 0;               // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;    // Set DCO
    DCOCTL = CALDCO_1MHZ;

    BCSCTL1 |= DIVA_0;        // ACLK/1 [ACLK/(0:1,1:2,2:4,3:8)]
    BCSCTL2 = 0;              // SMCLK [SMCLK/(0:1,1:2,2:4,3:8)]
    BCSCTL3 |= LFXT1S_0;      // LFXT1S0 32768-Hz crystal on LFXT1
}

/**
* \brief One time init of all GPIO port pin related items after 
*        boot up.
* \ingroup PUBLIC_API
*/
void hal_pinInit(void) {

    // I/O Control for UART
    P1SEL  |= RXD + TXD; // P1.1 = RXD, P1.2=TXD
    P1SEL2 |= RXD + TXD; // P1.1 = RXD, P1.2=TXD

    P1DIR  |= GSM_EN + GSM_DCDC + LS_VCC;
    P1OUT  &= 0x00;

    P2DIR |= BIT7; // P2.0-2.5 are pinosc, 6/7 are XIN/XOUT
    P2OUT &= 0x00; // All P2.x reset
    P2SEL  |= BIT6 + BIT7; // configuring crystal XIN for P2.6

    P3DIR |= 0xFF; // All P3.x outputs
    P3OUT &= 0x00; // All P3.x reset
}

/**
* \brief One time init of the UART subsystem after boot up.
* \ingroup PUBLIC_API
*/
void hal_uartInit(void) {
    //  ACLK source for UART
    UCA0CTL1 |= UCSSEL_1;         // ACLK
    UCA0BR0 = 0x03;               // 32 kHz 9600
    UCA0BR1 = 0x00;               // 32 kHz 9600
    UCA0MCTL = UCBRS0 + UCBRS1;   // Modulation UCBRSx = 3

    UCA0CTL1 &= ~UCSWRST;         // **Initialize USCI state machine**
    UC0IE |= UCA0RXIE;            // Enable USCI_A0 RX interrupt
}

