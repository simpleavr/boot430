#include <msp430.h>
#include "bbusb.inc"

;
;    Copyright (C) 2012  Kevin Timmerman
;
;    This program is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program.  If not, see <http://www.gnu.org/licenses/>.
;  
;
; 15 MHz USB low speed transmitter with CRC
;
; USB uses NRZI endoding with bit stuffing
; Each 0 bit is sent as a toggle of the data lines
; Each 1 bit is sent as no change of the data lines
; After six 1 bits (no change), a zero bit (toggle) is inserted
; This is to provide enough clock edges for the receiver to stay in sync
;   with the transmitter
;
; Ported to mspgcc by Chris Chung 2012.12
;
										;
	.global		USB_Transmit			;
										;
	.section	.text								;

	.if !CRC_STYLE
crc_table:
	.word	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241
	.word	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440
	.word	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40
	.word	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841
	.word	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40
	.word	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41
	.word	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641
	.word	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040
	.word	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240
	.word	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441
	.word	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41
	.word	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840
	.word	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41
	.word	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40
	.word	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640
	.word	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041
	.word	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240
	.word	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441
	.word	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41
	.word	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840
	.word	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41
	.word	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40
	.word	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640
	.word	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041
	.word	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241
	.word	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440
	.word	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40
	.word	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841
	.word	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40
	.word	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41
	.word	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641
	.word	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
	.endif

USB_Transmit:							; Transmit packet
										;
										; R15: Pointer to tx data
										; Registers pushed on stack (pop before reti):
										;  R8, R15, R14, R13, R12, R11, R10, R9
										;
										; Register usage:
										; R8:  CRC table pointer
										; R9:  Data line toggle mask
										; R10: Packet octet count
										; R11: Tx data octet
										; R12: Bit stuff counter
										; R13: CRC data octet
										; R14: CRC
										; R15: Tx data pointer
										;
	bic.b	#usbplus,  &usbout			; Default to J state
	bis.b	#usbminus, &usbout			; D+ low, D- high
										;
	mov		#usbplus | usbminus, R9		; Setup data line toggle mask
	bis.b	R9, &usbdir					; Set USB data lines to output
										;	
	.if CRC_STYLE						;     --- Data packet has CRC appended ---
	mov.b	@R15+, R10					; 8,9 Get packet length, inc pointer
	inc		R10							; 0   Inc length for sync octect
	mov		#0x80, R11					; 1,2 Begin with sync octect
	clr		R12							; 3   Clear bit stuff counter
										;
tx_data_loop:							;     --- Data transmit loop ---
										;     - Bit 0
	rra		R11							; 4   Get a bit
tx02:									;	
	jnc		tx00						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx01						; 0,1 No stuffing needed...
	nop									; 2
	jmp		tx02						; 3,4 Send stuffing bit										
tx00:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx01:									;
	clr		R8							; 2   Clear bit loop toggle
										;
tx13:									;     - Bit 1/3
	rra		R11							; 3   Get a bit
tx12:									;	
	nop									; 4
	jnc		tx10						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx11						; 0,1 No stuffing needed...
	jmp		tx12						; 2,3 Send stuffing bit									
tx10:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx11:									;
										;     - Bit 2/4
	rra		R11							; 2   Get a bit
tx22:									;	
	nop									; 3
	jnc		tx20						; 4,5 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 6   Increment bit stuffing count
	cmp		#-5, R12					; 7,8 Check if stuffing needed
	jhs		tx21						; 9,0 No stuffing needed...
	jmp		tx22						; 1,2 Send stuffing bit										
tx20:									;     Send zero bit
	clr		R12							; 6   Clear stuffing count
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
tx21:									;
	xor		#1, R8						; 1   Toggle loop count
	jnz		tx13						; 2,3 Do bits 2 & 3...
										;
										;
	.else								;	  --- !CRC_STYLE - Do on-the-fly CRC calculation
	clr		R10							; 1   Clear bit count
	tst		R15							; 2   Is there a pointer to a data packet?
	jeq		txc0						; 3,4 No, send sync + PID...
										;
										;     --- Send sync + data + CRC ---		
tx_sync:								;     - Send sync (0x80)
	nop									; 5   - 7 zero bits in this loop
	inc		R10							; 6   Increment bit count
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	cmp		#7, R10						; 1,2 Done with 7 bits?
	jne		tx_sync						; 3,4 No...
										;
										;     - 1 bit period without toggle
	mov.b	@R15+, R10					; 4,5 Get packet length, inc pointer
	mov.b	@R15+, R11					; 6,7 Get PID from tx packet, inc pointer
	clr		R12							; 0   Clear bit stuff counter
	mov		#crc_table, R8				; 8,9 Setup CRC table pointer										
	clr		R13							; 1   Init first CRC data byte (PID will be ignored)
	mov		#0xFE54, R14				; 2,3 Init CRC - this will result in the CRC
										;       being 0xFFFF after the first pass - the PID is not included
										;       in the CRC calculation
										;
tx_data_loop:							;     --- Data transmit loop ---
										;     - Bit 0
	rra		R11							; 4   Get a bit
tx02:									;	
	jnc		tx00						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx01						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		tx02						; 3,4 Send stuffing bit
tx00:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx01:									;
	xor		R14, R13					; 2   Xor data octet with crc
	mov.b	R13, R13					; 3   Mask off MSB
										;
										;     - Bit 1
	rra		R11							; 4   Get a bit
tx12:									;	
	jnc		tx10						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx11						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		tx12						; 3,4 Send stuffing bit
tx10:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx11:									;
	swpb	R14							; 2   Shift CRC right by 8 bits
										;      by doing byte swap and MSB clear
										;
										;     - Bit 2
	rra		R11							; 3   Get a bit
tx22:									;	
	mov.b	R14, R14					; 4
	jnc		tx20						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx21						; 0,1 No stuffing needed...
	jmp		tx22						; 2,3 Send stuffing bit										
tx20:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx21:									;
	rla		R13							; 2   Make pointer to CRC table entry
	add		R8, R13						; 3
										;
										;     - Bit 3
	rra		R11							; 4   Get a bit
tx32:									;	
	jnc		tx30						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx31						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		tx32						; 3,4 Send stuffing bit
tx30:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx31:									;
	mov		@R13, R13					; 2,3 Get CRC table entry
										;
										;     - Bit 4
	rra		R11							; 4   Get a bit
tx42:									;	
	jnc		tx40						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx41						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		tx42						; 3,4 Send stuffing bit
tx40:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx41:									;
	xor		R13, R14					; 2   Update CRC
										;
	.endif								;
										;     - Bit 5
	rra		R11							; 3   Get a bit
tx52:									;	
	nop									; 4
	jnc		tx50						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx51						; 0,1 No stuffing needed...										
	jmp		tx52						; 2,3 Send stuffing bit
tx50:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx51:									;
	mov.b	@R15+, R13					; 2,3 Get next CRC data octet
										;
										;     - Bit 6
	rra		R11							; 4   Get a bit
tx62:									;	
	jnc		tx60						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-5, R12					; 8,9 Check if stuffing needed
	jhs		tx61						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		tx62						; 3,4 Send stuffing bit
tx60:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
tx61:									;
										;
										;     - Bit 7
	rra		R11							; 2   Get a bit
tx72:									;	
	mov		R13, R11					; 3   Get next tx data octet
	jnc		tx70						; 4,5 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 6   Increment bit stuffing count
	cmp		#-5, R12					; 7,8 Check if stuffing needed
	jhs		tx71						; 9,0 No stuffing needed...
	jmp		tx72						; 1,2 Send stuffing bit										
tx70:									;     Send zero bit
	clr		R12							; 6   Clear stuffing count										
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
tx71:									;
	dec		R10							; 1   Decrement octet count
	jne		tx_data_loop				; 2,3 Next octet...
										;
										;										
	.if !CRC_STYLE						; --- Send CRC -or- handshake (Sync + PID)
										; Note: This sends inverted data because there is no
										;   time to invert the CRC
txc0:									;     - Bit 0/2/4/6/8/10/12/14
	rra		R14							; 4   Get a bit
txc5:									;	
	jc		txc1						; 5,6 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-6, R12					; 8,9 Check if stuffing needed
	jne		txc2						; 0,1 No stuffing needed...										
	nop									; 2
	jmp		txc5						; 3,4 Send stuffing bit
txc1:									;     Send zero bit
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
	clr		R12							; 1   Clear stuffing count
txc2:									;
	inc		R10							; 2   Increment bit pair count
										;     - Bit 1/3/5/7/9/11/13/15
	rra		R14							; 3   Get a bit
txc6:									;	
	jc		txc3						; 4,5 If zero bit...
										;     One bit - no data lines toggle
	dec		R12							; 7   Increment bit stuffing count
	cmp		#-6, R12					; 8,9 Check if stuffing needed
	jne		txc4						; 0,1 No stuffing needed...
	jmp		txc6						; 2,3 Send stuffing bit										
txc3:									;     Send zero bit
	clr		R12							; 6   Clear stuffing count										
	xor.b	R9, &usbout					; 7,8,9,0 Toggle data lines
txc4:									;
	cmp		#8, R10						; 1   Check if done with 8 bit pairs (16 bits)
	jne		txc0						; 2,3 Next bit pair...
	.endif								;																																																																
										;
										; --- Set SE0 state - EOP ---                                                          
	jmp		$ + 2						; 4,5
										;
	bic.b	#usbplus | usbminus, &usbout ; 6,7,8,9,0 Set SE0 and hold for two bit times (20 clock cycles)
										;
	pop		R8							; 1,2 Restore saved registers
	pop		R15							; 3,4  
	pop		R14							; 5,6
	pop		R13							; 7,8
	pop		R12							; 9,0
	pop		R11							; 1,2
	pop		R10							; 3,4
	pop		R9							; 5,6
										;
	bis.b	#usbminus, &usbout			; 7,8,9,0 Idle - J into line, required for EOP
										;          (Will be 1 cycle late if usbminus > 8)
  	jmp		$ + 2						; 1,2
  	jmp		$ + 2						; 3,4
	nop									; 5
										;
	bic.b	#usbplus | usbminus, &usbdir ; 6,7,8,9,0 Set data lines to input
										;
	bic.b	#usbplus, &usbifg			; Clear interrupt flag.
										;
	reti								; Return from interrupt
										;
										;
	.end								;
