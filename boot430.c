//
//    Copyright (C) 2012  Kevin Timmerman
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
//    Ported to mspgcc by Chris Chung 2012.12 w/ the following changes
//    . adopt / implements logic from v-usb hid bootloader
//    . enhance skeleton and made into a bootloader frame
//    . remove 32khz xtal sync, only usb sync frame used to sync clock
//
/*
	sample breadboard layout

        Vc Gnd
   +=====================================================+
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  V  G  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  --+--+--+--+--+--+--+--+--+-  .  .  .  .  .  |
   |       |- b6 b7 CK IO a7 a6 b5 b4 b3|                |
   |       |+ a0 a1 a2 a3 a4 a5 b0 b1 b2|                |
   |  .  .  --+--+--+--+--+--+--+--+--+-  .  .  .  .  .  |
   |  .  G  V  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  . D+ D-  .  .  .  .  .  .  .  .  .  .  .  .  |
   +=====================================================+
       +-o--o--o--o-+
       | |  |  |  | |
       | |  |  =  = | jumper for D+ D-
       | |  |  |  | |
       | |  +[___]+ | 1k5 between D- and Vcc
       | |  |  |  | |
       | +()+  |  | | 10uF Cap
       | |  o  _  _ |
       | | |\ | || || 68ohm x 2
       | +-|o|| || ||
       | | |/ |_||_|| LP2950 LDO 5V->3.3V
       | |  o  |  | |
       | |  |  |  | |
       | |  |  |  | |
       | o  o  o  o |

              V+ Gnd
   +=====================================================+
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  +-----[    ]---+  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  .  +--+--+--+--+--+--+--+--+--+  .  .  .  |
   |             |- b6 b7 CK IO a7 a6 b5 b4 b3|          |
   |             |+ a0 a1 a2 a3 a4 a5 b0 b1 b2|          |
   |  .  .  .  .  +--+--+--+--+--+--+--+--+--+  .  .  .  |
   |  .  +----[    ]-+  .  .  .  .  .  .  .  .  .  .  .  |
   |  +----[    ]-+  .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  . (Vi-G-Vo) .  .  .  .  .  .  .  .  .  .  .  .  |
   |  .  .  .  +()+  .  .  .  .  .  .  .  .  .  .  .  .  |
   +=====================================================+
     D+ D- 5v Gnd V+

*/
//
	#include	<msp430.h>
	#include	"bbusb.h"

//static uint8_t IrqInSendPacketBuffer[12];
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



//________________________________________________________________________________
										
int main(void) {

	//if (!(P1IN&usbminus)) usbwait = 0;	// no usb D- pullup, no bootloader
	// no pull-up, app present, run app
	if (!(P1IN&usbminus) && (*((char*) *((uint16_t*) 0xffbe)) != -1))
		asm("br &0xffbe");

	WDTCTL = WDTPW | WDTHOLD;			// Stop watchdog
	P1OUT = 0;							// Prepare Port 1
	usbdir = ~(usbplus | usbminus);		// USB lines are inputs
	//P1SEL |= BIT4;					// SMCLK for measurements on P1.4
	P2SEL = 0;
	P2DIR = BIT6|BIT7;					// indicator led
	P2OUT = 0;
										//
										//
	BCSCTL1 = 15;						// Range 15
	DCOCTL = 127;						// ~15 MHz
										//
	__delay_cycles(1000000);			// Allow some time to stabilize power
	uint8_t usbwait=12;
										//
										//
	usbies = 0;							// Interrupt on rising edge of D+ because there is keep-alive signaling on
										//   D- that would cause false syncs
	usbifg = 0;							// No interrupt pending
										//
	uint16_t addrStart=0, addrEnd=0, addrCurrent=0, addrSave=0;
	uint8_t  initialized=0;
	uint16_t heartbeat = 0;

	uint8_t idx=0, inBuf[48];
	uint8_t *buffUp=0;

	for (;;) {							// for-ever
		if (!heartbeat++) {
			if (initialized) {		// usb reset from host occured
				P2OUT ^= BIT6;
			}//if
			else {			// no reset, run app if present
				//if (!usbwait && (*((char*) 0xc000) != -1)) {
				if (!usbwait && (*((char*) *((uint16_t*) 0xffbe)) != -1)) {
					_BIC_SR(GIE);
					TACTL = 0;
					P2DIR = P1DIR = 0;
					P2OUT = P1OUT = 0;
					asm("br &0xffbe");
				}//if
				else {
					usbwait--;
				}//else
			}//else
		}//if

		unsigned n = 40;				// Reset if both D+ and D- stay low for a while
										// 10 ms in SE0 is reset
										// Should this be done in the ISR somehow???
		while (!initialized && !(usbin & (usbplus | usbminus))) {
			if (!--n) {					//
				CurrentAddress = 0;		// Device has initial address of 0
				NewAddress = 0;			//
				addrCurrent = addrStart = addrEnd = 0;
				if (!initialized) {
					TACTL = TASSEL_2 | MC_2 | TACLR;	// Continous up mode with clocking from SMCLK
					initialized = 1;
					//P2OUT |= BIT6;	// debug use, led indicate we are trying sync
					n = 250;
					while (--n) {
						while (!(usbin&usbminus));
						TACTL |= TACLR;
						while ((usbin&usbminus));
						if (TAR < 15000) ++DCOCTL; 
						else --DCOCTL;
					}//while
					usbie = usbplus;
					_BIS_SR(GIE);
				}//if
				break;
			}//if
		}//while
		if (DataPacketBuffer[0]) {		// Check if the USB SIE has received a packet
			 							// At the end of the buffer is the PID of the preceding token packet
			 							// Check if it is a SETUP packet
			if (DataPacketBuffer[15] == USB_PID_SETUP) {
										// Handle the packet, get a response
				if((DataToTransmit = ProtocolSetupPacket(DataPacketBuffer))) {
										// Setup to send the response
					Data_PID_Toggle = USB_PID_DATA0;
					DataToTransmitOffset = 1;    
				} else {				//
										// Send STALL PID if there is no response
					ReadyForTransmit = usb_packet_stall;
				}//else	
										// Check if it is an OUT packet					
			} else if (DataPacketBuffer[15] == USB_PID_OUT) {
				// will be getting stuffs here
				// incoming LL-PP-cx-(d0-d1-d2-d3-d4-d5-d6)-C1-C2
				// ex.      0b-4b-0a-(00-4b-0e-8d-0d-4e-0a)-56-fa
				// LL - length, ours are always 0b (11 bytes)
				// PP - PID, should only be DATA0 or DATA1 (0xc3 or 0x4b)
				// cx - application level control byte, c is control, x is filler
				//      c == 1, request flash write, start address / length follows
				//      c == 0, no special instruction, just carry data
				//		x can be anything, it is used to impact the packet checksum
				//        so that there are no unstuffing needed for them
				// d? - firmware data, each packet carries up to 7 bytes of data
				// C? - packet checksum, application does not use these
				//
				uint8_t *cp = DataPacketBuffer+2;
				if (*DataPacketBuffer == 0x08) {			// command
					if (*cp == 0x10) {		// flash write request
						cp++;
						addrCurrent = addrStart = (*cp<<8) + *(cp+1);		// get start address high bytes
						cp += 2;
						addrEnd = (*cp<<8) + *(cp+1);
						//______________ interrupt vector, don't do immediate flash write
						if (addrStart >= 0xff00) {
							buffUp = inBuf;
						}//if
						else {
							buffUp = 0;
							//____ erase and get ready to write flash immediate
							FCTL2 = FWKEY+FSSEL0+(30);
							FCTL1 = FWKEY+ERASE;
							FCTL3 = FWKEY;
							*((uint8_t *) addrCurrent) = 0;
							FCTL1 = FWKEY+WRT; 
						}//else
					}//if
				}//if
				if (*DataPacketBuffer == 0x0b) {			// our stuff is fixed size
					uint8_t c=0;
					if (buffUp) {
						for (c=0;c<8;c++) *((uint8_t *) buffUp++) = *cp++;
						addrCurrent += 8;
					}//if
					else {
						for (c=0;c<8;c++) inBuf[c] = *cp++;
						idx = 8;
					}//else
				}//if
			}//elseif
			DataPacketBuffer[0] = 0;	// Done with received packet, don't process again, allow USB SIE
										//  to receive the next packet
		}//if
										// Check if an outgoing packet needs chunking and the USB SIE
										//   is ready for it
		if (DataToTransmit && !ReadyForTransmit) {
			PacketGenerator();			// Make a chunk with CRC
		}//if
										//
		if (!ReadyForTransmitIrqIn) {	// Check if the USB SIE is ready for an endpoint 1 packet
			//IrqIn(IrqInSendPacketBuffer); // Build the packet
			//CRC(IrqInSendPacketBuffer);	// Append CRC
										// Send it
			//ReadyForTransmitIrqIn = IrqInSendPacketBuffer;
		}//if

		if (idx || buffUp) {
			if (idx) {
				idx--;
				*((uint8_t *) addrCurrent++) = inBuf[7-idx];		// write to flash 
			}//if
			if (!(addrCurrent&0x01ff) || addrCurrent >= addrEnd) {
				//____ lockup flash, we are crossing 512byte/block boundary
				//     or, we are done at last address
				FCTL1 = FWKEY;
				FCTL3 = FWKEY+LOCK; 
				uint8_t base = addrCurrent>>8;

				if (!(addrCurrent&0x01ff)) {		
					//___ erase and get ready for next block
					FCTL1 = FWKEY+ERASE;
					FCTL3 = FWKEY;
					*((uint8_t *) addrCurrent) = 0;
					FCTL1 = FWKEY+WRT; 
					base--;
				}//if
				else {
					//if (base == 0xff) {
					if (buffUp) {
						//
						//_____ we doing interrupt vector, so don't be interrupted
						uint16_t savIntr  = *((uint16_t *) 0xffe4);
						uint16_t savReset = *((uint16_t *) 0xfffe);
						_BIC_SR(GIE);		
						FCTL1 = FWKEY+ERASE;
						FCTL3 = FWKEY;
						*((uint8_t *) 0xff00) = 0;
						FCTL1 = FWKEY+WRT; 

						uint8_t *dp = (uint8_t *) 0xffa0;
						uint8_t *vp = (uint8_t *) 0xffe0;
						uint8_t *cp = inBuf;
						uint8_t i=0x20;
						while (i--) *dp++ = *cp++; 	// write to flash backup copy

						*((uint16_t *) (inBuf+4))  = savIntr;	// use bootloader's interrupt
						*((uint16_t *) (inBuf+30)) = savReset;// reset goes to bootloader

						i=0x20; cp = inBuf;
						while (i--) *vp++ = *cp++; 	// write to flash real vector
						// save application's end address
						*((uint16_t *) (0xff80)) = addrSave;
						*((uint16_t *) (0xffde)) = 0xaa55;	// disable factory BSL
						buffUp = 0;
						FCTL1 = FWKEY;
						FCTL3 = FWKEY+LOCK; 
					}//if
					else {		// code, let's save end address
						addrSave = addrEnd;
					}//else
					_BIS_SR(GIE);
				}//else

				if (addrCurrent >= addrEnd) {
					addrCurrent = addrStart = addrEnd = 0;
					idx = 0;
				}//if
			}//if
		}//if
	}//for-ever
}

