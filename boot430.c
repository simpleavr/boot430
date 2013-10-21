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
//	  v0.91 release notes
//
//    2013.11 changes for v0.91 mainly contributed by "theprophet" (username at 43Oh.com)
//    . theprophet tried v0.90 and had numerous issues
//    . had provided various fixes to enable reliability on various platforms
//    . implement correct report_id according to usb standards
//    . implement a much more reliable timing scheme for data flashing and transfer
//    . bootloader is a bit slower but much more reliable
//	  * if you use a led "heartbeat" blinker between P2.6 and P2.7, be sure to use a resistor
//	  * if you have a choice, use a 3.6V LDO for better reliability
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

#include  <msp430.h>
#include  "bbusb.h"

//static uint8_t IrqInSendPacketBuffer[12];
static const uint8_t usb_packet_stall[] =
{
  1, USB_PID_STALL
};

uint8_t TokenPacketBuffer[16];
uint8_t DataPacketBuffer[16] = { 0 };
uint8_t *ReadyForTransmitIrqIn = 0;
unsigned CurrentAddress = 0;
unsigned NewAddress = 0;
unsigned Data_PID_ToggleIrqIn = USB_PID_DATA1;
uint8_t *ReceiveBufferPointer = TokenPacketBuffer;

#ifdef USE_32768HZ_XTAL
static unsigned FrequencyCounter = 0;
#endif

extern unsigned Data_PID_Toggle;
extern unsigned DataToTransmitOffset;
extern const uint8_t *DataToTransmit;
extern const uint8_t *ReadyForTransmit;



//________________________________________________________________________________

int main(void) {

  //if (!(P1IN&usbminus)) usbwait = 0;  // no usb D- pullup, no bootloader
  // no pull-up, app present, run app
  if (!(P1IN&usbminus) && (*((char*) *((uint16_t*) 0xffbe)) != -1))
    asm("br &0xffbe");

  WDTCTL = WDTPW | WDTHOLD;     // Stop watchdog
  P1OUT = 0;              // Prepare Port 1
  usbdir = ~(usbplus | usbminus);   // USB lines are inputs
  //P1SEL |= BIT4;          // SMCLK for measurements on P1.4

  P1DIR |= BIT5;

#ifdef USE_32768HZ_XTAL
  BCSCTL1 = DIVA_3 | 15;        // ACLK / 8, Range 15
  DCOCTL = 127;           // ~15 MHz
  TACTL = TASSEL_2 | MC_2 | TACLR;  // Continous up mode with clocking from SMCLK
  TACCTL0 = CM_1 | CCIS_1 | CAP | CCIE; // Setup CCR0 for capturing on rising edges of ACLK with enabled interrupts
  __eint();
#else
  P2SEL = 0;
  P2DIR = BIT6|BIT7;          // indicator led
  P2OUT = 0;

  BCSCTL1 = 15;           // Range 15
  DCOCTL = 127;           // ~15 MHz
#endif

  __delay_cycles(2000000);      // Allow some time to stabilize power
  uint8_t usbwait=12;


  usbies = 0;       // Interrupt on rising edge of D+ because there is keep-alive signaling on
                    //   D- that would cause false syncs
  usbifg = 0;       // No interrupt pending

  uint16_t addrStart=0, addrEnd=0, addrCurrent=0, addrSave=0;
  uint8_t  initialized=0;
  uint16_t heartbeat = 0;

  uint8_t idx=0, inBuf[48];
  uint8_t *buffUp=0;
  uint8_t next_time_flash_erase = 0;

  static uint16_t const USB_IDLE_DETECT_THRESHOLD = 16384;
  uint16_t usb_idle_detect_counter = 0;
  uint8_t usb_idle = 0;

  /*    What is this usb idle thing ?
     We are in a bootloader, its aim is to receive via usb a new
     program to be flashed. But erasing and writing to flash takes
     quite a big time, and usb timing is tight. The usb protocol
     needs that response packets be sent within a certain time
     frame, and if the mcu misses that time frame, the host can
     consider the device as malfunctioning.
     The idea behind the usb idle thing is to delay operations that
     take time to a moment where we hope the mcu will not have any
     usb packet to answer.
     Specifically :
     1. when the host side sends an address where flash must be
        erased, the host sleeps for a certain amount T of milliseconds
        before sending another command. Within T milliseconds, the
        mcu has time to answer remaining usb packets and erase the
        requested flash segment.
     2. when the host side sends payload bytes (which are bytes of
        the program to be flashed), the mcu has no time to write them
        into flash and answer usb packets. It stores them in a small
        RAM buffer, and writes them to flash once the host sleeps a
        bit. Here, the host sends 32 bytes before sleeping and letting
        the mcu write those bytes to flash.
     To detect usb idle, we simply count how many "for-ever" loops we
     have done since last received usb packet, and beyond a threshold,
     we consider being in usb idle mode, that is to say : we consider
     the host will not send any other usb packet before we have
     finished with flashing and being able to handle a new usb packet.
  */

  for (;;) // for-ever
  {
    // Detect USB idle
    if(initialized && !usb_idle)
    {
      usb_idle_detect_counter++;
      if(usb_idle_detect_counter > USB_IDLE_DETECT_THRESHOLD)
        usb_idle = 1;
    }

    if (!heartbeat++)
    {
      if (initialized) // usb reset from host occured
      {
        P2OUT ^= BIT6;
      }//if
      else
      { // no reset, run app if present
        //if (!usbwait && (*((char*) 0xc000) != -1)) {
        if (!usbwait && (*((char*) *((uint16_t*) 0xffbe)) != -1))
        {
          _BIC_SR(GIE);
          TACTL = 0;
          P2DIR = P1DIR = 0;
          P2OUT = P1OUT = 0;
          asm("br &0xffbe");
        }
        else
        {
          usbwait--;
        }
      }
    }

    unsigned n = 80;        // Reset if both D+ and D- stay low for a while
                    // 10 ms in SE0 is reset
                    // Should this be done in the ISR somehow???
    while (!initialized && !(usbin & (usbplus | usbminus)))
    {
      if (!--n)
      {
        CurrentAddress = 0;   // Device has initial address of 0
        NewAddress = 0;     //
        addrCurrent = addrStart = addrEnd = 0;

#ifndef USE_32768HZ_XTAL
        if (!initialized)
        {
          TACTL = TASSEL_2 | MC_2 | TACLR;  // Continous up mode with clocking from SMCLK
          initialized = 1;
          //P2OUT |= BIT6;  // debug use, led indicate we are trying sync
          n = 1000; // time to stabilize clock
          while (--n)
          {
            while (!(usbin&usbminus));
            TACTL |= TACLR;
            while ((usbin&usbminus));
            if (TAR < 15000) ++DCOCTL;
            else --DCOCTL;
          }
          usbie = usbplus;
          _BIS_SR(GIE);
        }
#else
        usbie = usbplus;
        _BIS_SR(GIE);
        initialized = 1;
#endif

        break;
      }
    }


    if(DataPacketBuffer[0]) // Check if the USB SIE has received a packet
    {
	  uint8_t packet_size = *DataPacketBuffer;	// we save packet_size here to avoid it being
      DataPacketBuffer[0] = 0;					// overwritten by "fast" status packet that follows
      usb_idle_detect_counter = 0;
      usb_idle = 0;

      // At the end of the buffer is the PID of the preceding token packet
      // Check if it is a SETUP packet
      if(DataPacketBuffer[15] == USB_PID_SETUP)
      {
        // Handle the packet, get a response
        if((DataToTransmit = ProtocolSetupPacket(DataPacketBuffer)))
        {
          // Setup to send the response
          Data_PID_Toggle = USB_PID_DATA0;
          DataToTransmitOffset = 1;
        }
        else
        {
          // Send STALL PID if there is no response
          ReadyForTransmit = usb_packet_stall;
        }
      }
      else if (DataPacketBuffer[15] == USB_PID_OUT) // Check if it is an OUT packet
      {
        // will be getting stuffs here
        // incoming LL-PP-RI-(d0-d1-d2-d3-d4-d5)-C1-C2
        // ex.      0a-4b-01-(00-4b-0e-8d-0d-4e)-56-fa
        // LL - length, ours are always 0a or 08
        // PP - PID, should only be DATA0 or DATA1 (0xc3 or 0x4b) - 4b for us
        // RI - application level control byte - the HID report id. Here :
        //      == 1, request flash write, start address / length follows
        //      == 2, no special instruction, just carry data
        // d? - firmware data, each packet carries up to LL-4 bytes of data
        //      for report id 1 : report count = 6, LL = 0a, data bytes = 6
        //      for report id 2 : report count = 4, LL = 08, data bytes = 4
        // C? - packet checksum, application does not use these
        //

        uint8_t *cp = DataPacketBuffer+2;
        //if (*DataPacketBuffer == 0x0A && *cp == 0x01)
        if (packet_size == 0x0A)
        { // flash write request (report id 1)
          cp++;
          addrCurrent = addrStart = (*cp<<8) + *(cp+1); // get start address high bytes
          cp += 2;
          addrEnd = (*cp<<8) + *(cp+1);
          //______________ interrupt vector, don't do immediate flash write
          if (addrStart >= 0xff00)
          {
            buffUp = inBuf;
          }
          else
          {
            addrSave = addrEnd;
            buffUp = 0;
            idx = 0;
            next_time_flash_erase = 1;
          }
        }
        //else if (*DataPacketBuffer == 0x08 && *cp == 0x02)
        else if (packet_size == 0x08)
        {
          uint8_t c=0; // receive bytes (report id 2)
          cp++; // skip report id
          if (buffUp) // we are receiving interrupt vector bytes
          {
            for (c=0;c<4;c++)
              *((uint8_t *) buffUp++) = *cp++;
            addrCurrent += 4;
          }
          else
          { // we are receiving normal data
            for (c=0;c<4;c++)
              inBuf[c+idx] = *cp++;
            idx += 4;
          }
        }

      } // else if USB_PID_OUT
      //DataPacketBuffer[0] = 0; // Done with received packet, don't process again, allow USB SIE to receive the next packet
    } // if USB SIE has received a packet

    // Check if an outgoing packet needs chunking and the USB SIE
    //   is ready for it
    if (DataToTransmit && !ReadyForTransmit)
    {
      PacketGenerator(); // Make a chunk with CRC
    }

    if (!ReadyForTransmitIrqIn)
    { // Check if the USB SIE is ready for an endpoint 1 packet
      /*
      IrqIn(IrqInSendPacketBuffer); // Build the packet
      CRC(IrqInSendPacketBuffer); // Append CRC
      ReadyForTransmitIrqIn = IrqInSendPacketBuffer; // Send it
      */
    }


    // We have received interrupt vector address, and data bytes.
    // USB is idle which means we can write to flash
    if(usb_idle && buffUp && addrCurrent > addrStart && addrStart >= 0xff00)
    {
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
      while (i--) *dp++ = *cp++;  // write to flash backup copy

      *((uint16_t *) (inBuf+4))  = savIntr; // use bootloader's interrupt
      *((uint16_t *) (inBuf+30)) = savReset;// reset goes to bootloader

      i=0x20; cp = inBuf;
      while (i--) *vp++ = *cp++;  // write to flash real vector
      *((uint16_t *) (0xff80)) = addrSave; // save application's end address
      *((uint16_t *) (0xffde)) = 0xaa55;  // disable factory BSL
      buffUp = 0;
      FCTL1 = FWKEY;
      FCTL3 = FWKEY+LOCK;

      _BIS_SR(GIE);
    }


    // We have received addresses. USB is idle : erase flash.
    if(usb_idle && next_time_flash_erase)
    {
      FCTL2 = FWKEY+FSSEL0+(30);
      FCTL1 = FWKEY+ERASE;
      FCTL3 = FWKEY;
      *((uint8_t *) addrCurrent) = 0;
      FCTL1 = FWKEY+WRT;
      next_time_flash_erase = 0;
    }


    // We have received data bytes, and USB is idle : flash them.
    if(usb_idle && !buffUp && addrStart < 0xff00 && idx)
    {
      uint8_t c=0;
      for(c = 0; c < idx; c++)
        *((uint8_t *) addrCurrent++) = inBuf[c];
      idx = 0;

      if (!(addrCurrent&0x01ff))
      {
        //____ we are crossing 512byte/block boundary
        //____ erase and get ready for next block
        FCTL1 = FWKEY+ERASE;
        FCTL3 = FWKEY;
        *((uint8_t *) addrCurrent) = 0;
        FCTL1 = FWKEY+WRT;
      }
    }


    if(addrCurrent >= addrEnd && !buffUp) // !buffUp means that we have handled the interrupt vector in an usb idle time
    {
      //____ lockup flash, we are over.
      FCTL1 = FWKEY;
      FCTL3 = FWKEY+LOCK;

      addrCurrent = addrStart = addrEnd = 0;
      idx = 0;
    }

  }//for-ever
}


#ifdef USE_32768HZ_XTAL
#pragma vector = TIMER0_A0_VECTOR   // Timer A CCR0 interrupt
__interrupt void dco_adjust(void)   //
{                   //
  TA0CCTL0 &= ~CCIFG;         // Clear interrupt flag
  __eint();       // Re-enable interrupts to allow fast response to USB
                    // Adjust DCO
                    // 15,000,000 / (32,768 / 8) = 3662.11
                    // 3662.5 * (32,768 / 8) = 15,001,600
  if((TA0CCR0 - FrequencyCounter) < 3662) ++DCOCTL; else --DCOCTL;
  FrequencyCounter = TA0CCR0;     //
}                   //
#endif

