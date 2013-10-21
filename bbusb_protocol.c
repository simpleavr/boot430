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
//
	#include	<msp430.h>
	#include	"bbusb.h"


extern const uint8_t *ReadyForTransmit;
extern unsigned NewAddress;
extern uint8_t *ReadyForTransmitIrqIn;
extern unsigned Data_PID_ToggleIrqIn;


static const uint8_t StatusEmpty[] = {
	0
};

static const uint8_t StatusZeroZero[] = {
	2, 0, 0
};

static const uint8_t Device_Descriptor[] = {	// Mouse Device Descriptor
	18,									// Length
	0x12,								// bLength
	0x01,								// bDescriptorType = Device
	0x10, 0x01,							// bcdUSB (USB Spec Rev 1.10)
	0x00,								// bDeviceClass
	0x00,								// bDeviceSubClass
	0x00,								// bDeviceProtocol
	0x08,								// bMaxPacketSize0
	0xc0, 0x16,							// idVendor
	0xdf, 0x05,							// idProduct
	0x01, 0x00,							// bcdDevice
	0x01,								// iManufactuer (String descriptor index)
	0x02,								// iProduct (String descriptor index)
	0x00,								// iSerialNumber (String descriptor index)
	0x01								// idNumConfigurations
};

static const uint8_t Configuration_Descriptor_first9[] = {
	 9, 9, 2, 34, 0, 1, 1, 0, 0x80, 50
};

static const uint8_t Configuration_Descriptor_complete[] = {
	  34,								// Length
	  									// - Config Descriptor
	   9,								// bLength
	   2,								// bDescriptorType = Configuration
	  34, 0,							// wTotalLength
	   1,								// bNumInterfaces
	   1,								// bConfigurationValue (Used by Set Configuration request)
	   0,								// iConfiguration (String descriptor for this configuration)
	0x80,								// bmAttributes
	  50,								// bMaxPower (in units of 2 mA)
	  									// - Interface Descriptor
	   9,								// bLength
	   4,								// bDescriptorType = Interface
	   0,								// bInterfaceNumber
	   0,								// bAlternateSetting
	   1,								// bNumEndpoints
	   3,								// bInterfaceClass = HID
	   0,								// bInterfaceSubClass = Boot
	   0,								// bInterfaceProtocol = Mouse
	   0,								// iInterface (String descriptor for this interface)
	   									// - HID Descriptor
	   9,								// bLength
	0x21,								// bDescriptorType = HID
	0x01, 0x01,							// bcdHID (Class Spec Version 1.10)
	   0,								// bCountryCode
	   1,								// bNumDescriptors
	  34,								// bDescriptorType = Report
	  33, 0,							// wDescriptorLength
	  									// - Endpoint Descriptor
	   7,								// bLength
	   5,								// bDescriptorType = Endpoint
	0x81,								// bEndpointAddress = 1 IN
	   3,								// bmAttributes = Interrupt
	0x08, 0x00,							// wMaxPacketSize
	0xC8								// bInterval (ms)
};

static const uint8_t HID_Descriptor[] = {	// i am me
	33,
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)

    0x85, 0x01,                    //   REPORT_ID (1)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0x85, 0x02,                    //   REPORT_ID (2)
    0x95, 0x04,                    //   REPORT_COUNT (4)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

										// The first string descriptor (index 0) specifies the language(s)
static const uint8_t String_Descriptor_0[] = {
	4,									// Length
	4,									// bLength
	3,									// bDescriptorType = String
	0x09, 0x04							// wLangID = 0x0409 US English
};

										// Strings are Unicode UTF-16
static const uint8_t String_Descriptor_1[] = {
	20, 20, 3, 's', 0, 'i', 0, 'm', 0, 'p', 0, 'l', 0, 'e', 0, 'a',	0, 'v', 0, 'r', 0
};

static const uint8_t String_Descriptor_2[] = {
	16, 16, 3, 'b', 0, 'o', 0, 'o', 0, 't', 0, '4',	0, '3', 0, '0', 0,
};

#define BOOT430_VER		0
#define BOOT430_REL		91

static uint8_t ClassReport[] = {
		7,
        1,                              /* report ID */
        BOOT430_VER,
        BOOT430_REL,
        0xff,
        0xff,
        0xff,
        0xff, };


const uint8_t* ProtocolSetupPacket(uint8_t *pkt)
{										//

	if(pkt[2] & 0x20) {					// Class Requests.
		switch(pkt[3]) {				//
			case 0x01:					// Get Report
				ClassReport[4] = *((uint8_t *) 0xff80);
				ClassReport[5] = *((uint8_t *) 0xff81);
				ClassReport[6] = *((uint8_t *) 0x0ff1);
				ClassReport[7] = *((uint8_t *) 0x0ff0);
				return ClassReport;
			case 0x09:					// Set Report
			case 0x0A:					// Set Idle
				return StatusEmpty;		//
		}								//
	} else {							//
		switch(pkt[3]) {				//
			case 0x00:					// Get Status
				return StatusZeroZero;	//
			case 0x05:					// Set Address
				NewAddress = pkt[4];	//
				return StatusEmpty;		/// This should work, but isn't reliable!
			case 0x06:					// Get Descriptor.
				switch(pkt[5]) {		//  Which one ?
					case 0x01:			//	Device Descriptor.
						return Device_Descriptor;
					case 0x02:			// Configuration Descriptor.
						if(pkt[8] == 9) { // 0B C3 80 06 00 02 00 00 --> 09 <-- 00 AE 04
										// Short Configuration Descriptor, only the first nine bytes
							return Configuration_Descriptor_first9;
						} else {		// Complete Configuration Descriptor
							return Configuration_Descriptor_complete;
						}				//
					case 0x03:			// Strings
						switch(pkt[4]) {
							case 0:		//
								return String_Descriptor_0;
							case 1:		//
								return String_Descriptor_1;
							case 2:		//
								return String_Descriptor_2;
						}				//
					case 0x22:			// HID Descriptor
						return HID_Descriptor;
				}						//
			case 0x09:					// Set Configuration
				return StatusEmpty;		//
			case 0x0B:					// Set Interface
				ReadyForTransmitIrqIn = 0;
				Data_PID_ToggleIrqIn = USB_PID_DATA1;
				return StatusEmpty;		//
		}								//
	}									//
	return 0;							//
}										//


volatile uint8_t keyCnt=4;

void IrqIn(uint8_t *d) {
	*d++ = 3;
	*d++ = (Data_PID_ToggleIrqIn ^= (USB_PID_DATA0 ^ USB_PID_DATA1));

	*d++ = 0;
	*d = 0;
}
