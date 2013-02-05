//******************************************************************************
//  MSP430F20xx Demo - RGB LED
//
//  Description; RGB LED via P1.0-3, Tactile Button via P2.7
//  ACLK = n/a, MCLK = SMCLK = default DCO
//
//                MSP430F20xx
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          --|RST          P2.3|-->Button     in
//            |             P2.5|-->Button (-) out
//            |                 |
//            |             P2.2|-->LED-G
//            |             P2.1|-->LED-B
//            |             P2.0|-->LED-Common
//            |             P1.5|-->LED-R
/*
        Vc Gnd
   +=====================================================+
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  v  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  ---+--+--+--+--+--+--+--+--+  .  .  .  .  .  | connect v to v
   |       |- b6 b7 CK IO a7 a6 b5 b4 b3|                |
   |       |+ a0 a1 a2 a3 a4 a5 b0 b1 b2|                |
   |  .  .  ---+--+--+--+--+--+--+--+--+  .  .  .  .  .  |
   |  .  .  v  .  .  .  .  .  R (c) G  B  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   +=====================================================+

*/
//
//  CChung
//  Giftware, no license, no warranty
//  July 2010
//  Built with msp430-gcc
//******************************************************************************

#include  <msp430.h>
#include  <stdio.h>

volatile uint8_t rgb=7;

void main(void) {
	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	BCSCTL1 = CALBC1_1MHZ;
	DCOCTL  = CALDCO_1MHZ;

	P1DIR = BIT5;
	P2DIR = BIT0|BIT1|BIT2|BIT3;

	CCTL0 = CCIE;                             // CCR0 interrupt enabled
	CCR0 = 100;
	TACTL = TASSEL_2 + MC_1;                  // SMCLK, upmode

	P2SEL = 0x00;
	//P2REN = 0xc0;
	P2REN = BIT5;

	uint8_t button=0;

	_BIS_SR(GIE);
	while (1) {
		if (P2IN&BIT5) {
			if (button > 5) {		// button released
				rgb++;
			}//if
			button = 0;
		}//if
		else {
			button++;
		}//else
	}//while
}

volatile uint8_t clicks=0;
volatile uint8_t ticks=0;
// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void) {
	clicks++;
	if (clicks&0x03) {
		P1OUT = 0x00;
	}//if
	else {
		P2OUT = 0;
		P1OUT = 0;
		//_________ trying to compsensate for individual color brightness
		if (rgb&0x04) P1OUT |= BIT5;
		if (rgb&0x02) P2OUT |= BIT2;
		if (rgb&0x01) P2OUT |= BIT1;
		ticks++;
		if (!ticks) rgb++;
	}//else
}

