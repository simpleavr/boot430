#include <msp430.h>
#include <intrinsics.h>


void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;
  BCSCTL1 = CALBC1_1MHZ;
  DCOCTL  = CALDCO_1MHZ;

	P2SEL = 0;
  P1DIR = 0xFF;
  P2DIR = BIT6|BIT7;
  P1OUT = 0;
  P2OUT = 0;

  for(;;)
  {
    P1OUT ^= 0xFF;
    P2OUT ^= BIT6;
    __delay_cycles(1000000);
  }
}
