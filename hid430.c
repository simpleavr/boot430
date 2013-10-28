//
//    Copyright © 2012  Kevin Timmerman
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
    #include    <msp430.h>
    #include    "bbusb.h"


static unsigned FrequencyCounter;
static uint8_t IrqInSendPacketBuffer[12];
static const uint8_t usb_packet_stall[] = {
    1, USB_PID_STALL
};


uint8_t TokenPacketBuffer[16];
uint8_t DataPacketBuffer[16] = { 0 };
uint8_t *ReadyForTransmitIrqIn = 0;
unsigned CurrentAddress = 0;
unsigned NewAddress = 0;
unsigned Data_PID_ToggleIrqIn = USB_PID_DATA1;
uint8_t *ReceiveBufferPointer = TokenPacketBuffer;


extern unsigned Data_PID_Toggle;
extern unsigned DataToTransmitOffset;
extern const uint8_t *DataToTransmit;
extern const uint8_t *ReadyForTransmit;




void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog
                                        //
    P1OUT = 0;                          // Prepare Port 1
    usbdir = ~(usbplus | usbminus);     // USB lines are inputs
    //P1SEL |= BIT4;                      // SMCLK for measurements on P1.4
                                        //
    FrequencyCounter = 0;               //
                                        //
#ifdef USE_32768HZ_XTAL
    BCSCTL1 = DIVA_3 | 15;              // ACLK / 8, Range 15
#else
    BCSCTL1 = 15;                       // Range 15
#endif
    DCOCTL = 127;                       // ~15 MHz
                                        //
    TACTL = TASSEL_2 | MC_2 | TACLR;    // Continous up mode with clocking from SMCLK
#ifdef USE_32768HZ_XTAL
    TACCTL0 = CM_1 | CCIS_1 | CAP | CCIE; // Prepare CCR0 for capturing on rising edges of ACLK with enabled interrupts
#endif
                                        //
    //_enable_interrupts();             // Begin DCO clock tuning
    _BIS_SR(GIE);


#ifndef USE_32768HZ_XTAL
    uint8_t wait=32;
    while (--wait)
#endif
    __delay_cycles(1000000);                // Allow some time to stabilize
                                        //
                                        //
    usbies = 0;                         // Interrupt on rising edge of D+ because there is keep-alive signaling on
                                        //   D- that would cause false syncs
    usbifg = 0;                         // No interrupt pending
#ifdef USE_32768HZ_XTAL
    usbie = usbplus;                    // Interrupt enable for D+
#endif
                                        //
    for(;;) {                           // for-ever
        unsigned n = 40;                // Reset if both D+ and D- stay low for a while
                                        // 10 ms in SE0 is reset
                                        // Should this be done in the ISR somehow???
        while(!(usbin & (usbplus | usbminus))) {
            if(!--n) {                  //
                CurrentAddress = 0;     // Device has initial address of 0
                NewAddress = 0;         //
#ifndef USE_32768HZ_XTAL
                if (!wait) {
                    wait = 1;
                while (!(usbin&usbminus));
                while ((usbin&usbminus));


                n = 250;
                while (--n) {
                    while (!(usbin&usbminus));
                    TACTL |= TACLR;
                    while ((usbin&usbminus));


                    if (TAR < 15000) ++DCOCTL;
                    else --DCOCTL;
                }//while


                P1OUT |= BIT2;
                usbie = usbplus;
                }//if
#endif
                break;                  //
            }                           //
        }                               //
        if(DataPacketBuffer[0]) {       // Check if the USB SIE has received a packet
                                        // At the end of the buffer is the PID of the preceding token packet
                                        // Check if it is a SETUP packet
            if(DataPacketBuffer[15] == USB_PID_SETUP) {
                                        // Handle the packet, get a response
                if(DataToTransmit = ProtocolSetupPacket(DataPacketBuffer)) {
                                        // Setup to send the response
                    Data_PID_Toggle = USB_PID_DATA0;
                    DataToTransmitOffset = 1;
                } else {                //
                                        // Send STALL PID if there is no response
                    ReadyForTransmit = usb_packet_stall;
                }                       //
                                        // Check if it is an OUT packet
            } else if(DataPacketBuffer[15] == USB_PID_OUT) {
            }                           //
            DataPacketBuffer[0] = 0;    // Done with received packet, don't process again, allow USB SIE
                                        //  to receive the next packet
        }                               //
                                        // Check if an outgoing packet needs chunking and the USB SIE
                                        //   is ready for it
        if(DataToTransmit && !ReadyForTransmit) {
            PacketGenerator();          // Make a chunk with CRC
        }                               //
                                        //
        if(!ReadyForTransmitIrqIn) {    // Check if the USB SIE is ready for an endpoint 1 packet
            IrqIn(IrqInSendPacketBuffer); // Build the packet
            CRC(IrqInSendPacketBuffer); // Append CRC
                                        // Send it
            ReadyForTransmitIrqIn = IrqInSendPacketBuffer;
        }                               //
    }                                   //
}




#ifdef USE_32768HZ_XTAL
#pragma vector = TIMER0_A0_VECTOR       // Timer A CCR0 interrupt
__interrupt void dco_adjust(void)       //
{                                       //
    TA0CCTL0 &= ~CCIFG;                 // Clear interrupt flag
    //_enable_interrupts();             // Re-enable interrupts to allow fast response to USB
    _BIS_SR(GIE);
                                        // Adjust DCO
                                        // 15,000,000 / (32,768 / 8) = 3662.11
                                        // 3662.5 * (32,768 / 8) = 15,001,600
    if((TA0CCR0 - FrequencyCounter) < 3662) ++DCOCTL; else --DCOCTL;
    FrequencyCounter = TA0CCR0;         //
}                                       //
#endif

