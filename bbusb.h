
#include <stdint.h>

// bbusb_protocol.c
const uint8_t *ProtocolSetupPacket(uint8_t *);
void IrqIn(uint8_t *);

// bbusb_packet.c
void CRC(uint8_t *);
void PacketGenerator(void);

// --- USB PID values
// Token
#define USB_PID_OUT		0xE1
#define USB_PID_IN		0x69
//USB_PID_SOF			0xA5
#define USB_PID_SETUP 	0x2D
// Data
#define USB_PID_DATA0	0xC3
#define USB_PID_DATA1	0x4B
//USB_PID_DATA2			0x87 // HS only
//USB_PID_MDATA			0x0F // HS only
// Handshake
#define	USB_PID_ACK		0xD2
#define	USB_PID_NAK		0x5A
#define	USB_PID_STALL	0x1E
//USB_PID_NYET			0x96
// Special
//USB_PID_PRE			0x3C
//USB_PID_ERR			0x3C
//USB_PID_SPLIT			0x78
//USB_PID_PING			0xB4


#define usbout		P1OUT
#define usbdir		P1DIR
#define usbin		P1IN
#define usbifg		P1IFG
#define usbie		P1IE
#define usbies		P1IES

#define usbplus 	BIT0	// P1.0 D+ over 50 Ohms
#define	usbminus	BIT1	// P1.1 D- over 50 Ohms with 1.5 kOhm pullup hardwired to Vcc.

#define nop2 		jmp		$ + 2

#define	toggle_syncled	xor.b	#BIT2, &P1OUT	// Toggles Sync	LED, good for triggering an	oscilloscope and visual	feedback
												// Preserve clock cycles if you want to remove this !
