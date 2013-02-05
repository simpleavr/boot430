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
//
	#include	<msp430.h>
	#include	"bbusb.h"


static uint8_t SendPacketBuffer[12];	// LEN PID DATA[0...8] CRCL CRCM <- 12 octets max

unsigned DataToTransmitOffset;
 
uint8_t *ReadyForTransmit = 0;
uint8_t *DataToTransmit = 0;
unsigned Data_PID_Toggle = USB_PID_DATA0;

										
void CRC(uint8_t *d)					// Append CRC to an outbound USB SIE packet 
{										// Before: [Length] [PID] [Data 0...8]
										// After:  [Length] [PID] [Data 0...8] [CRC LSB] [CRC MSB]
	uint16_t x;							//
	uint16_t crc = 0xFFFF;				// Init CRC with FFFF
	uint8_t *end = d + *d;				// Make pointer to last octect of buffer
	*d++ += 2;							// Add 2 to length - the CRC takes 2 octets
	++d;								// Skip length and PID octets, the CRC is of data only
	while(d <= end) {					// Do all data octets
		x = *d++;						// Get an octect of data
		x ^= crc; crc >>= 8;			// Xor with CRC, shift CRC
		if(x & 0x01) crc ^= 0xC0C1;		// Perform CRC on each bit of (octect ^ CRC)
		if(x & 0x02) crc ^= 0xC181;		//
		if(x & 0x04) crc ^= 0xC301;		//
		if(x & 0x08) crc ^= 0xC601;		//
		if(x & 0x10) crc ^= 0xCC01;		//
		if(x & 0x20) crc ^= 0xD801;		//
		if(x & 0x40) crc ^= 0xF001;		//
		if(x & 0x80) crc ^= 0xA001;		//
	}									//
	crc ^= 0xFFFF;						// Invert CRC
	*d++ = crc & 0x00FF;				// Append LSB of CRC
	*d = crc >> 8;						// Append MSB of CRC
}										//

void PacketGenerator(void)				// Split outbound USB packets in to chunks of 8 or fewer data octets
{										//
										// The last packet must have 7 or fewer data octets, all preceding must
										//   have 8 data octets <<<--- read that again!
										// If the last chunk fills the packet (8 data octets) then a packet with
										//   no data must be sent as the last packet  <<<--- understand now?
										//
										// This should never happen!
	//if(ReadyForTransmit || !DataToTransmit) return;
										//
										// Make pointer to next chunk of data
	uint8_t *s = DataToTransmit + DataToTransmitOffset;
										// Make pointer to last octet of data
	uint8_t *end = DataToTransmit + DataToTransmit[0];
										//
	uint8_t *d = SendPacketBuffer + 1;	// Make pointer to second octet (PID) of USB SIE packet
										// Toggle the DATA PID for each USB SIE packet 
	*d++ = (Data_PID_Toggle ^= (USB_PID_DATA0 ^ USB_PID_DATA1));
										//
	unsigned n = 0;						// Init data octet counter
	do {								//
		if(s > end) {					// Check if at end of buffer
			DataToTransmit = 0;			// Nothing more to send >>> and fewer than 8 data octets
										//   in this packet <<<--- very important!
			break;						// Stop data copy
		}								//
		*d++ = *s++;					// Copy a data octet
	} while(++n < 8);					// Until up to 8 octets have been copied
										//
	DataToTransmitOffset += n;			// Move to next chunk of data
	SendPacketBuffer[0] = n + 1;		// Set USB SIE packet length
										//
	CRC(SendPacketBuffer);				// Append CRC to USB SIE packet
										//
										// Send the packet
	ReadyForTransmit = SendPacketBuffer; // The SIE will see the non-NULL pointer and tx the buffer
}										//

