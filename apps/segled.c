//
//  Description; 
//    7 segment led examples
//
//
/*
		MSP430 20pin

        Vc Gnd
   +=====================================================+
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  c  .  .  .  .  .  .  .  .  .  .  | connect c to c
   |  .  v  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  ---+--+--+-(0)-A--F-(1)(2)-B--+--+--+  .  .  | connect v to v
   |       |- b6 b7 CK IO a7 a6 b5 b4 b3|       |        |
   |       |+ a0 a1 a2 a3 a4 a5 b0 b1 b2|       |        |
   |  .  .  --+--+--+--+--+--+--+--+--+-  .  .  |  .  .  |
   |  .  .  .  +  -  -  E  D (.) C  G (3) -  -  +  .  .  |
   |  .  .  v  .  c  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  +--B--+  .  . +.  .  .  .  .  .  .  .  .  .  | +--B--+ tactile button
   |  .  .  .  .  .  .  +Bz+  .  .  .  .  .  .  .  .  .  | +Bz+    buzzer
   +=====================================================+

		MSP430 20pin (reverse)

        Vc Gnd
   +=====================================================+
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  c  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  ---+--+--+-(3) G  C (.) D  E                 |
   |       |- b6 b7 CK IO a7 a6 b5 b4 b3|                |
   |       |+ a0 a1 a2 a3 a4 a5 b0 b1 b2|                |
   |  .  .  --+--+--+--+--+--+--+--+--+-  .  .  .  .  .  |
   |  .  .  .  .  .  .  B (2)(1) F  A  0  .  .  .  .  .  |
   |  .  .  v  .  c  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  +--B--+  .  . +.  .  .  .  .  .  .  .  .  .  | +--B--+ tactile button
   |  .  .  .  .  .  .  +Bz+  .  .  .  .  .  .  .  .  .  | +Bz+    buzzer
   +=====================================================+



*/


#define DEVICE_20P		// uncomment for 20 pin devices
#define DEVICE_REVERSE
#define LED_3DIGITS
#define LED_COMMONANODE

#include <msp430.h>

#include <stdint.h>
#include <stdio.h>


#define NO_TEMP_CHOICE
//#define METRIC_TEMP	// uncomment for oC

#ifdef DEVICE_20P

//____ this is the pin map for 20pin devices, i just shift the led module right 3 steps
#ifdef DEVICE_REVERSE

#define SEG_A_P1	0
#define SEG_B_P1	(1<<3)
#define SEG_C_P1	(1<<6)
#define SEG_D_P1	0
#define SEG_E_P1	0
#define SEG_F_P1	0
#define SEG_G_P1	(1<<7)
#define SEG_d_P1	0
#define DIGIT_0_P1	0
#define DIGIT_1_P1	(1<<5)
#define DIGIT_2_P1	(1<<4)
#define DIGIT_3_P1	(1<<1)

#define SEG_A_P2	(1<<1)
#define SEG_B_P2	0
#define SEG_C_P2	0
#define SEG_D_P2	(1<<4)
#define SEG_E_P2	(1<<3)
#define SEG_F_P2	(1<<0)
#define SEG_G_P2	0
#define SEG_d_P2	(1<<5)
#define DIGIT_0_P2	(1<<2)
#define DIGIT_1_P2	0
#define DIGIT_2_P2	0
#define DIGIT_3_P2	0

#else

#define SEG_A_P1	(1<<7)
#define SEG_B_P1	0x00
#define SEG_C_P1	0x00
#define SEG_D_P1	(1<<4)
#define SEG_E_P1	(1<<3)
#define SEG_F_P1	(1<<6)
#define SEG_G_P1	0x00
#define SEG_d_P1	(1<<5)
//#define DIGIT_0_P1	(1<<1)
#define DIGIT_0_P1	0x00
#define DIGIT_1_P1	0x00
#define DIGIT_2_P1	0x00
#define DIGIT_3_P1	0x00

#define SEG_A_P2	0x00
#define SEG_B_P2	(1<<3)
#define SEG_C_P2	(1<<0)
#define SEG_D_P2	0x00
#define SEG_E_P2	0x00
#define SEG_F_P2	0x00
#define SEG_G_P2	(1<<1)
#define SEG_d_P2	0x00
#define DIGIT_0_P2	0x00
#define DIGIT_1_P2	(1<<5)
#define DIGIT_2_P2	(1<<4)
#define DIGIT_3_P2	(1<<2)

#endif

//#define BTNP_P1 	(1<<1)		// button uses p1.1
#define BTNP_P1 	(1<<6)		// button uses p1.1
#define _P2DIR 		(1<<7)		// we want to try use 32khz crystal


#else

//____ this is the original pin map for 14pin devices

#define SEG_A_P1	0x00
#define SEG_B_P1	(1<<6)
#define SEG_C_P1	(1<<3)
#define SEG_D_P1	(1<<1)
#define SEG_E_P1	(1<<0)
#define SEG_F_P1	0x00
#define SEG_G_P1	(1<<4)
#define SEG_d_P1	(1<<2)
#define DIGIT_0_P1	0x00
#define DIGIT_1_P1	(1<<2)
#define DIGIT_2_P1	(1<<7)
#define DIGIT_3_P1	(1<<5)

#define SEG_A_P2	(1<<7)
#define SEG_B_P2	0x00
#define SEG_C_P2	0x00
#define SEG_D_P2	0x00
#define SEG_E_P2	0x00
#define SEG_F_P2	(1<<6)
#define SEG_G_P2	0x00
#define SEG_d_P2	0x00
#define DIGIT_0_P2	(1<<6)
#define DIGIT_1_P2	0x00
#define DIGIT_2_P2	0x00
#define DIGIT_3_P2	0x00

#define BTNP_P1 	(1<<2)		// button uses p1.2
#define _P2DIR 		0

#endif
// calcuate number of segments on individual digits, letters show
// will use to decide how long a digit / letter stays "on"
#define SEGS_STAY(v) \
   (((v & (1<<6)) ? 1 : 0) +\
	((v & (1<<5)) ? 1 : 0) +\
	((v & (1<<4)) ? 1 : 0) +\
	((v & (1<<3)) ? 1 : 0) +\
	((v & (1<<2)) ? 1 : 0) +\
	((v & (1<<1)) ? 1 : 0) +\
	((v & (1<<0)) ? 1 : 0)) | 0x20

// macro magic
// what the io ports output for individual digits / letters
// we do this at compile time so that we don't need to use runtime cycles
// to map segment and port pins
#define SEGS_PORT_DET(p, v) \
   (((v & (1<<6)) ? SEG_A_P##p : 0) |	\
	((v & (1<<5)) ? SEG_B_P##p : 0) |	\
	((v & (1<<4)) ? SEG_C_P##p : 0) |	\
	((v & (1<<3)) ? SEG_D_P##p : 0) |	\
	((v & (1<<2)) ? SEG_E_P##p : 0) |	\
	((v & (1<<1)) ? SEG_F_P##p : 0) |	\
	((v & (1<<0)) ? SEG_G_P##p : 0))

#define SEGS_PORT(v)	{SEGS_STAY(v),SEGS_PORT_DET(1, v),SEGS_PORT_DET(2, v)}
// port 1/2 pins used to turn segments on, led anodes
#define SEGS_1 (SEG_A_P1|SEG_B_P1|SEG_C_P1|SEG_D_P1|SEG_E_P1|SEG_F_P1|SEG_G_P1)
#define SEGS_2 (SEG_A_P2|SEG_B_P2|SEG_C_P2|SEG_D_P2|SEG_E_P2|SEG_F_P2|SEG_G_P2|SEG_d_P2)

// port 1/2 pins used turn digits on, led cathodes
#define DIGITS_1 (DIGIT_0_P1|DIGIT_1_P1|DIGIT_2_P1|DIGIT_3_P1)
#define DIGITS_2 (DIGIT_0_P2|DIGIT_1_P2|DIGIT_2_P2|DIGIT_3_P2)

// port 1/2 pins used
#define USED_1 (SEGS_1|DIGITS_1)
#define USED_2 (SEGS_2|DIGITS_2)

/*
       ___a__
      |      |
     f|      | b
       ___g__
     e|      | c
      |      |
       ___d__
*/
// composition of digits and letters we need
//_____________________ abc defg
#define LTR_0 0x7e	// 0111 1110
#define LTR_1 0x30	// 0011 0000
#define LTR_2 0x6d	// 0110 1101
#define LTR_3 0x79	// 0111 1001
#define LTR_4 0x33	// 0011 0011
#define LTR_5 0x5b	// 0101 1011
#define LTR_6 0x5f	// 0101 1111
#define LTR_7 0x70	// 0111 0000
#define LTR_8 0x7f	// 0111 1111
#define LTR_9 0x7b	// 0111 1011
#define BLANK 0x00	// 0000 0000
#define BAR_1 0x40	// 0100 0000
#define BAR_2 0x01	// 0000 0001
#define BAR_3 (BAR_1|BAR_2)
#define LTRdg 0x63	// 0110 0011
#define LTR_C 0x4e	// 0100 1110

#define LTR_c 0x4e	// 0000 1101
#define LTR_A 0x77	// 0111 0111
#define LTR_b 0x1f	// 0001 1111
#define LTR_J 0x3c	// 0011 1100
#define LTR_L 0x0e	// 0000 1110
#define LTR_S 0x5b	// 0101 1011
#define LTR_E 0x4f	// 0100 1111
#define LTR_t 0x0f	// 0000 1111
#define LTR_n 0x15	// 0001 0101
#define LTR_d 0x3d	// 0011 1101
#define LTR_i 0x10	// 0001 0000
#define LTR_H 0x37	// 0011 0111
#define LTR_r 0x05	// 0000 0101
#define LTR_o 0x1d	// 0001 1101
#define LTR_f 0x47	// 0100 0111
#define LTRml 0x66	// 0110 0110
#define LTRmr 0x72	// 0111 0010
#define LTR__ 0x00	// 0000 0000

// port io values for individual digits / letters
// 1st byte cycles to stay
// 2nd byte port 1 value
// 3rd byte port 2 value
static const uint8_t digit2ports[][3] = { 
	SEGS_PORT(LTR_0), SEGS_PORT(LTR_1), SEGS_PORT(LTR_2), SEGS_PORT(LTR_3),
	SEGS_PORT(LTR_4), SEGS_PORT(LTR_5), SEGS_PORT(LTR_6), SEGS_PORT(LTR_7),
	SEGS_PORT(LTR_8), SEGS_PORT(LTR_9), SEGS_PORT(BLANK), SEGS_PORT(LTR_o),
	SEGS_PORT(BAR_2), SEGS_PORT(LTR_b), SEGS_PORT(LTR_f), SEGS_PORT(LTR_C), 
	SEGS_PORT(LTR_t), SEGS_PORT(LTR_H), SEGS_PORT(LTR_n), SEGS_PORT(LTR_r), 
	SEGS_PORT(LTR_J), SEGS_PORT(LTR_d), SEGS_PORT(LTR_L), SEGS_PORT(LTR_i),
	SEGS_PORT(LTR_E), SEGS_PORT(LTR_A),
};

// digits / letters we are using
enum {
	POS_0, POS_1, POS_2, POS_3, POS_4, POS_5, POS_6, POS_7,
	POS_8, POS_9, POS__, POS_o, POSb2, POS_b, POS_f, POS_C, 
	POS_t, POS_H, POS_n, POS_r, POS_J, POS_d, POS_L, POS_i,
	POS_E, POS_A,
};

// storage for multiplexing, 3 bytes output x 4 digits
uint8_t output[3 * 4];

// loads data into output[] for immediate led multiplexing
//__________________________________________________
void seg2port(uint16_t val) {

#ifdef LED_3DIGITS
	uint8_t which = 2;
#else
	uint8_t which = 3;
#endif
	do {
		uint8_t *pp = output + which * 3;
		uint8_t offset = 3;
		uint8_t digit = val % 10;
		if (val) val /= 10;
		do {
			*pp++ = digit2ports[digit][--offset];
		} while (offset);
	} while (which--);

}

volatile uint8_t ticks=0;

//______________________________________________________________________
void main(void) {

	WDTCTL = WDTPW + WDTHOLD;

	BCSCTL1 = CALBC1_1MHZ;		// Set DCO to 1MHz
	DCOCTL = CALDCO_1MHZ;

	CCTL0 = CCIE;                             // CCR0 interrupt enabled
	CCR0 = 50000;
	TACTL = TASSEL_2 + MC_1;                  // SMCLK, upmode
	_BIS_SR(GIE);                 

	/*
	P1DIR = BIT3|BIT4;
	P1OUT = 0;
	P1OUT = BIT4;
	while (1) { asm("nop"); }
	*/
	uint8_t stays=0;
	uint16_t cnt=0;

	uint8_t pos=0; 
	static const uint8_t digit_map1[] = { DIGIT_0_P1, DIGIT_1_P1, DIGIT_2_P1, DIGIT_3_P1, };
	static const uint8_t digit_map2[] = { DIGIT_0_P2, DIGIT_1_P2, DIGIT_2_P2, DIGIT_3_P2, };
	while (1) {


		// allow led segments stays on
		if (stays & 0x0f) { stays--; continue; }
		// turn off all io pins, led blanks
		P2DIR = 0; P1DIR = 0;
		P2OUT = 0; P1OUT = 0;
		// allow led segments stays blank out, dimming
		stays >>= 2;		// adjust
		if (stays) { stays--; continue; }

		pos &= 0x03;

		if (!pos) {
			if (ticks >= 5) {
				seg2port(cnt++);
				ticks = 0;
			}//if
		}//if

		//___________ load segments and turn on 1 of 4 digits
		uint8_t *ioptr = output + (pos*3);

#ifdef LED_COMMONANODE
		P2OUT = ~(*ioptr & ~digit_map2[pos]);
#else
		P2OUT = *ioptr & ~digit_map2[pos];
#endif
		P2DIR = (*ioptr++ | digit_map2[pos]) | _P2DIR;
#ifdef LED_COMMONANODE
		P1OUT = ~(*ioptr & ~digit_map1[pos]);
#else
		P1OUT = *ioptr & ~digit_map1[pos];
#endif
		P1DIR = *ioptr++ | digit_map1[pos];
		stays = *ioptr;

		pos++;

	}//while

}

//______________________________________________________________________
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
	__bic_SR_register_on_exit(CPUOFF);        // Clear CPUOFF bit from 0(SR)
}

//______________________________________________________________________
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void) {
	ticks++;
	//_BIC_SR_IRQ(LPM3_bits);
}


