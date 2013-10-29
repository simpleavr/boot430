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
	0x34, 0x12,							// idVendor
	0x01, 0x00,							// idProduct
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
	   1,								// bInterfaceSubClass = Boot
	   2,								// bInterfaceProtocol = Mouse
	   0,								// iInterface (String descriptor for this interface)
	   									// - HID Descriptor
	   9,								// bLength
	0x21,								// bDescriptorType = HID
	0x10, 0x01,							// bcdHID (Class Spec Version 1.10)
	   0,								// bCountryCode
	   1,								// bNumDescriptors
	  34,								// bDescriptorType = Report
	  52, 0,							// wDescriptorLength
	  									// - Endpoint Descriptor
	   7,								// bLength
	   5,								// bDescriptorType = Endpoint
	0x81,								// bEndpointAddress = 1 IN
	   3,								// bmAttributes = Interrupt
	0x05, 0x00,							// wMaxPacketSize
	0x0A								// bInterval (ms)
};

static const uint8_t HID_Descriptor[] = {	// Mouse HID Descriptor - 3 buttons + wheel
	52,									// Length
    0x05, 0x01,                    		// USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                   		// USAGE (Mouse)
    0xA1, 0x01,                    		// COLLECTION (Application)
    0x09, 0x01,                    		//   USAGE (Pointer)
    0xA1, 0x00,                    		//   COLLECTION (Physical)
    0x05, 0x09,                    		//     USAGE_PAGE (Button)
    0x19, 0x01,                    		//     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    		//     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    		//     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    		//     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    		//     REPORT_COUNT (3)  		L/M/R Buttons
    0x75, 0x01,                    		//     REPORT_SIZE (1)
    0x81, 0x02,                    		//     INPUT (Data,Var,Abs)
    0x95, 0x01,                    		//     REPORT_COUNT (1)
    0x75, 0x05,                    		//     REPORT_SIZE (5)			5 bits padding after buttons
    0x81, 0x03,                    		//     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    		//     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    		//     USAGE (X)					X rel postion
    0x09, 0x31,                    		//     USAGE (Y)					Y rel position
    0x09, 0x38,                    		//     USAGE (Wheel)				Wheel rel position
    0x15, 0x81,                    		//     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    		//     LOGICAL_MAXIMUM (127)
    0x95, 0x03,                    		//     REPORT_COUNT (3)
    0x75, 0x08,                    		//     REPORT_SIZE (8)
    0x81, 0x06,                    		//     INPUT (Data,Var,Rel)
    0xC0,                          		//   END_COLLECTION
    0xC0                           		// END_COLLECTION
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
	16, 16, 3, 'o', 0, 'P', 0, 'o', 0, 's', 0, 's', 0, 'u', 0, 'm',	0
};

#ifdef USE_32768HZ_XTAL
static const uint8_t String_Descriptor_2[] = {
    30, 30, 3, 'M', 0, 'o', 0, 'u', 0, 's', 0, 'e', 0, ' ', 0, '4', 0, '3', 0, '0', 0
    ,  ' ', 0, 'X', 0, 'T', 0, 'A', 0, 'L', 0
};
#else
static const uint8_t String_Descriptor_2[] = {
	20, 20, 3, 'M', 0, 'o', 0, 'u', 0, 's', 0, 'e', 0, ' ', 0, '4',	0, '3', 0, '0', 0
};
#endif

const uint8_t* ProtocolSetupPacket(uint8_t *pkt)
{										//
	if(pkt[2] & 0x20) {					// Class Requests.
		switch(pkt[3]) {				//
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
										//

void IrqIn(uint8_t *d)					//
{										// -- Mouse HID response
	*d++ = 5;							// Length
										// Data PID toggle
	*d++ = (Data_PID_ToggleIrqIn ^= (USB_PID_DATA0 ^ USB_PID_DATA1));  
	*d++ = 0;							// Buttons 
	*d++ = 1;							// X
	*d++ = 0;							// Y
	*d++ = 0;							// Wheel
}										//
