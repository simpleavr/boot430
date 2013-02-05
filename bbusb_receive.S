#include <msp430.h>
#include "bbusb.inc"

;
;	 Mecrimus-B	- A	firmware-only USB implementation for MSP430	microcontrollers
;	 Copyright (C) 2012	 Matthias Koch
;
;	 This program is free software:	you	can	redistribute it	and/or modify
;	 it	under the terms	of the GNU General Public License as published by
;	 the Free Software Foundation, either version 3	of the License,	or
;	 (at your option) any later	version.
;
;	 This program is distributed in	the	hope that it will be useful,
;	 but WITHOUT ANY WARRANTY; without even	the	implied	warranty of
;	 MERCHANTABILITY or	FITNESS	FOR	A PARTICULAR PURPOSE.  See the
;	 GNU General Public	License	for	more details.
;
;	 You should	have received a	copy of	the	GNU	General	Public License
;	 along with	this program.  If not, see <http://www.gnu.org/licenses/>.
;  
; Ported to CCS v4 by Kevin Timmerman 2012.12
; Ported to mspgcc by Chris Chung 2012.12
; . added check upon interrupt entry to decide if bootloader or application get interrupt
;
										; Receiver for 15 MHz, 10 cycles/bit
										;
										;
	.section	.text								;
										;
	.word	CurrentAddress				;
	.word	NewAddress					;
	.word	ReadyForTransmit			;
	.word	ReadyForTransmitIrqIn		;
	.word	DataPacketBuffer			;
	.word	ReceiveBufferPointer		;
	.word	TokenPacketBuffer			;
										;
	.word	USB_Transmit				;
	;.word	USB_Transmit_CRC			;
										;
										;
	.if CRC_STYLE						;										
usb_packet_ack:							;
	.byte	1, USB_PID_ACK				;
usb_packet_nak:							;
	.byte	1, USB_PID_NAK				;
usb_packet_stall:						;
	.byte	1, USB_PID_STALL			;
	.endif								;
										;
										;
.macro nrzi_r10							;
	mov.b	@r9, r10					; [1]  NRZI
										; [2]  <---	Sampling
	xor		r10, r11					; [3]
	and		#usbminus, r11				; [4]
	rrc.b	r12							; [5]
	.endm								;
										;
.macro nrzi_r11							;
	mov.b	@r9, r11					; [1]  NRZI							
										; [2]  <---	Sampling
	xor		r11, r10					; [3]
	and		#usbminus, r10				; [4]
	rrc.b	r12							; [5]
	.endm								;
										;
										;
.macro xnstuff register, bitmask		; (this macro was unstuff, now retired)
	bit		r8,	r12						; [8]	Detect stuffing, 6 bits	without	change...
1:	jnz		1f							; [9]	Data bits inverted here	!
										; [10]
	mov.b	@r9, \register				; [e 1]
										; [e 2]	 <--- Read state again for proper NRZI decoding 
	bis		#0x80, r12					; [e 3]	Set MSB in data
										; [e 4]
	xor		#\bitmask, r13				; [e 5]	Set changed place in bitstuff mask
										;
										;		Insert one nop more	if constant	generator can be used
	.if (bitmask == 1) | ( bitmask == 2 ) | ( bitmask == 4 ) | (bitmask == 8)
		nop								; [	e 6	]
	.endif								;
	jmp		1b							; [e 7 - e 8]
1:										; Finished stuffing
	.endm								;
										;
.macro unstuff register, bitmask		;
	bit		r8,	r12						; [8]	Detect stuffing, 6 bits	without	change...
1:	jnz		1f							; [9]	Data bits inverted here	!
										; [10]
	mov.b	@r9, \register				; [e 1]
										; [e 2]	 <--- Read state again for proper NRZI decoding 
	and		#usbplus+usbminus, \register; [e 3] SE0 check
										; [e 4]
	jz		usb_se0						; [e 5] 
										; [e 6]
	bis		#0x80, r12					; [e 7]	Set MSB in data
										; [e 8]
	xor		#\bitmask, r13				; [e 9]	Set changed place in bitstuff mask
										;		Insert one nop more	if constant	generator can be used
	.if (bitmask == 1) | ( bitmask == 2 ) | ( bitmask == 4 ) | (bitmask == 8)
		nop								; [e10]
	.endif								;
1:										; Finished stuffing
	.endm								;
										;
										;
.macro unstuff_hurry_up bitmask			; Only for 1, 2, 4, 8 because of Constant Generator use
	jnz		1f							; [3 not previous bit]  ; Refers to the last bit unstuffing !
										; [4 not previous bit]
	mov		r10, r11					; [es 5]  R10 in the destuff bit spin state. Put it as a basis for ready the last measurement.
	nop 								; [es 6]
	bis		#0x80, r12					; [es 7]  The primary bit in data
										; [es 8]
	xor		#\bitmask, r13				; [es 9]  Appropriate place for the previous bit
	nop									; [es 10] Remember in Stopfmaske
	mov.b	@r9, r10					; [es 1]  R10 in the destuff bit had cashed. It must be replaced.
										; [es 2] <--- Sampling !
	nop2								; [es 3]
										; [es 4]  Because of this detects for unstuffing in	the	LAST bit, we have to juggle	around registers here.
1:										; Finished stuffing
	.endm								;
										;
										; Signal for Sync pattern
										; Signal: 
										; D-:	normally high, Sync: 01	01 01 00 
										; D+:			 low		 10	10 10 11
										; Idea:	Search 1-->0 transtition, sample it	precisely and look if double zero follows, else	repeat.
										; As initialisation	after IRQ (after bit.b for zero) needs some	time, we will normally not see the first zero here.
										;
										; ---	.include "Sync-15MHz.asm"
										;
										; Synchronize to the pattern 01010100 on D- with 15 MHz
										; Entry point for Interrupt
										;
	.if 0								; Prototype without timeout for counting cycles
										;
										; bit.b - affects flags, but does not change the register contents.
										; As and.b -> If D is high, is the right jnz jump.
										;  
; -----------------------				; Wait until the low state is here:
- bit.b @r9, r10						;        
; <-- Sampling							; (  9L 10L 11L 12L )
jnz -									; ( 10L 11L 12L  1H )
;										; ( 11L 12L  1H  2H )
; -----------------------				; Low detected. Wait till high state because:
- bit.b @r9, r11						; ( 12L  1H  2H  3H )
; <-- Sampling							; (  1H  2H  3H  4H )
jz -									; (  2H  3H  4H  5H )
;										; (  3H  4H  5H  6H )
; -----------------------				; Now there is high.
										;
	.endif								;
										;
										; Enter with interrupt triggered by low-to-high-transtion on D+
										; This is required to not trigger syncing on Keep-Alive-SE0s.
										;
										; Idea: Each bit is 10 bars long in the line
										; Sync to the pattern 01010100 on D- 
										; ---------------------------------------------------------------------------
										;
Usb_Sync_Error_9_10_11_12:				;
	pop		r12							;
	pop		r11							;
Usb_Sync_Error_9_10:					; Error happened
	pop		r10							;
	pop		r9							;
	bic.b	#usbplus, &usbifg			; Clear interrupt pending bit to prevent an endless loop in case of false sync or Reset.
	reti								;
										;
										;
										; ---------------------------------------------------------------------------
.global	__isr_2
__isr_2:
	cmp.b	#15, &BCSCTL1				; Real Entry point of interrupt
	jz		USB_Sync					; if I am running at 15Mhz, I am bootloader
	br		&0xffa4						; no, jump to application interrupt handler
USB_Sync:								; Entry point of interrupt
	push	r9							;
	push	r10							;
										;
USB_Quick_Resync:						;
	mov		#usbin, r9					;
	mov		#usbminus, r10				;
										;
	;bit.b	@r9, r10					;  Waiting for a low in D
	; <-- Sampling						;  1
	;jz		j00							;  2
	;									;  3
	bit.b	@r9, r10					;  4
	; <-- Sampling						;  5         Waiting to NULL in D
	jz		j00							;  6         Wait for Low in D-
	;									;  7
	bit.b	@r9, r10					;  8
	; <-- Sampling						;  9
	jz		j00							; 10
	;									; 11
	bit.b	@r9, r10					; 12
	; <-- Sampling						; 13
	jnz		Usb_Sync_Error_9_10			;
	;									;
j00:; -----------------------			;  Low detected
	bit.b	@r9, r10					; 
	; <-- Sampling						;  1
	jnz		j01							;  2
	;									;  3
	bit.b	@r9, r10					;  4
	; <-- Sampling						;  5         Waiting for ONE in D
	jnz		j01							;  6         Wait for High in D-
	;									;  7
	bit.b	@r9, r10					;  8
	; <-- Sampling						;  9
	jnz		j01							; 10
	;									; 11
	bit.b	@r9, r10					; 12
	; <-- Sampling						; 13
	jz		Usb_Sync_Error_9_10			;
	;									; High detected, synced within 4 cycles
j01:;   ------------>					; (  3H  4H  5H  6H )	Leave the preliminary sync with these bars
	; -----------------------			; 						First try to sync precisely
	push	r11							; (  4H  5H  6H  7H )
	;									; (  5H  6H  7H  8H )
	;									; (  6H  7H  8H  9H )
	mov.b	@r9, r10					; (  7H  8H  9H 10H )    
	; <-- Sampling						; (  8H  9H 10H  1L ) 
	mov.b	@r9, r11					; (  9H 10H  1L  2L )
	; <-- Sampling						; ( 10H  1L  2L  3L )
	; -----------------					; (   |   |   |   | )
	bit		#usbminus, r10				; (  1L  2L  3L  4L )
	jnc		j02							; (  2L  3L  4L  5L )
	;									; (  3L  4L  5L  6L )
	bit		#usbminus, r11				; (  4L  5L  6L   | )
	jnc		j03							; (  5L  6L  7L   | )
	;									; (  6L  7L  8L   | )
j02: nop2								; (  7L   |   |  7L )
	;									; (  8L   |   |  8L )
j03:; -----------------					; (                 )  Edge sampling finished
	push	r12							; (  9L  8L  9L  9L )
	;									; (      9L 10L     )
	;									; (     10L  1N     )
	nop									; (      1N  2N     )  
	mov.b	@r9, r10					; (      2N  3N     )  Check, if this is really low for two bit times
	; <-- Sampling						; (      3N  4N     )
	bit		#usbminus, r10				; (      4N  5N     )
	jnc		Usb_Rec_Einlauf				; (      5N  6N     )
	;									; (      6N  7N     )
										;
	; -----------------------			; Second try to sync precisely
	nop2								; (  5H  6H  7H  8H )  Another syncinc run with entry on 7/8 leads to sync with one cycle accuracy.
	;									; (  6H  7H  8H  9H ) 
	mov.b	@r9, r10					; (  7H  8H  9H 10H )    
	; <-- Sampling						; (  8H  9H 10H  1L ) 
	mov.b	@r9, r11					; (  9H 10H  1L  2L )
	; <-- Sampling						; ( 10H  1L  2L  3L )
	; -----------------					; (   |   |   |   | )
	bit		#usbminus, r10				; (  1L  2L  3L  4L )
	jnc		j04							; (  2L  3L  4L  5L )
	;									; (  3L  4L  5L  6L )
	bit		#usbminus, r11				; (  4L  5L  6L   | )
	jnc		j05							; (  5L  6L  7L   | )
	;									; (  6L  7L  8L   | )
j04: nop2								; (  7L   |   |  7L )
	;									; (  8L   |   |  8L )
j05:; -----------------					; (                 )  Edge sampling finished
	nop2								; (  9L  8L  9L  9L )
	;									; (      9L 10L     )
	nop2								; (     10L  1N     )
	;									; (      1N  2N     )
	mov.b	@r9, r10					; (      2N  3N     )  Check, if this is really low for two bit times
	; <-- Sampling						; (      3N  4N     )
	bit		#usbminus, r10				; (      4N  5N     )
	jnc		Usb_Rec_Einlauf				; (      5N  6N     )
	;									; (      6N  7N     )
										;
	; -----------------------			; Third and last try to sync precisely
	nop2								; (  5H  6H  7H  8H )  Another syncinc run with entry on 7/8 leads to sync with one cycle accuracy.
	;									; (  6H  7H  8H  9H )
	mov.b	@r9, r10					; (  7H  8H  9H 10H )    
	; <-- Sampling						; (  8H  9H 10H  1L ) 
	mov.b	@r9, r11					; (  9H 10H  1L  2L )
	; <-- Sampling						; ( 10H  1L  2L  3L )
	; -----------------					; (   |   |   |   | )
	bit		#usbminus, r10				; (  1L  2L  3L  4L )
	jnc		j06							; (  2L  3L  4L  5L )
	;									; (  3L  4L  5L  6L )
	bit		#usbminus, r11				; (  4L  5L  6L   | )
	jnc		j07							; (  5L  6L  7L   | )
	;									; (  6L  7L  8L   | )
j06: nop2								; (  7L   |   |  7L )
	;									; (  8L   |   |  8L )
j07:; -----------------					; (                 )  Edge sampling finished
	nop2								; (  9L  8L  9L  9L )
	;									; (      9L 10L     )
	nop2								; (     10L  1N     )
	;									; (      1N  2N     )
	mov.b	@r9, r10					; (      2N  3N     )  Check, if this is really low for two bit times
	; <-- Sampling						; (      3N  4N     )
	bit		#usbminus, r10				; (      4N  5N     )
										; (      5N  6N     )
	jc		Usb_Sync_Error_9_10_11_12	;
	;									; (      6N  7N     )
										;
Usb_Rec_Einlauf:						;
	nop2								; (      7N  8N     )  Sync is finished. Prepare to run into receiver !
	;									; (      8N  9N     )
	toggle_syncled						; (      9N 10N     )
	;									; (     10N  1B     )
	;									; (      1B  2B     )
	; <-- Wechsel						; (      2B  3B     )  Syncled is electrically toggled now.
										; ---------------------------------------  Entry in receiver now.
	; mov.b	@r9, ...					; (      3B  4B     )
	; <-- Sampling						; (      4B  5B     )
										;
										; ---   end .include "Sync-15MHz.asm"
										;
										; ---------------------------------------------------------------------------
										; Sync is ok, lets start !
										;
										; Can do preparations while	running	in,	because	there cannot occour	stuffing on	the	first few bits.
										;
										; Register usage:
										; r8   ; Mask for detect stuffing
										; r9   ; Address of	P2IN
										;
										; r10  ; Buffer	for	NRZI pin state
										; r11  ; --
										;
										; r12  ; Data input	shift register,	has	to be initialized to sync-pattern not(9f0)
										; r13  ; Stuffing mask,	has	to be cleared
										;
										; r14  ; Data backup register
										; r15  ; Pointer to	write into receive buffer
										;
										; Note:	Data is	inverted because of	clever use of flags	and	side-effects to	save cycles
										;		Stuffing mask holds	those bits that	have been inverted,	so that	stuffing detection 
										;		will not detect	stuffing on	the	same bits more than	one	time.
										;
										; We have some time	in the first byte of data, as stuffing cannot occour the first 5 bits.
										; D- is	used for sync and input, D+	is only	used to	detect packet end SE0.
										;
										; ---------------------------------------------------------------------------
										; First Bit: (0)	Run-in with	preparations
										; ---------------------------------------------------------------------------
										;
	mov.b	@r9, r10					; [1]  NRZI				( 3	 4)	 Chance	counting here so that mov.b	@r9	always counts on one.
										; [2]  <---	Sampling	( 4	 5)	 Center-Sampling on	5 or 6 !
	mov		#0x7F, r12					; [3]  not(9f0)			( 5	 6)	 Initialize	data, as last bit of Syncpattern may take part in stuffing.
										; [4]							 
	and		#usbminus, r10				; [5] instead bit Wechsele initiate here the method of payment! One arrogated deliver directly the value of the first bit. Depends on sync byte final. There is D-low. Exchange - set> Carry. Get there :-)
	rrc.b	r12							; [6]  and rotate the MOV instruction comes as ever on record [1] :-) First Data bit! Usbminus contains the first data bit inverted. fits :-)
										;	   Catch first data	bit, use only AND here,	as last	state out of sync pattern is always	the	same.
	push	r13							; [7]  Save more registers
										; [8] 
										; [9] 
	clr		r13							; [10] Clear	stuffing mask
										;
										; ---------------------------------------------------------------------------
										; Second Bit:	(1)	  Run-in with preparations
										; ---------------------------------------------------------------------------
										;
	nrzi_r11							; [1-5]
	push	r14							; [6]  The data	backup register	doesn't	need initialisation.
										; [7]  
										; [8]
										; [9]  Fetch the Pointer in	two	steps, could be	in one step	@ 18 MHz/12	cycles.
	mov		#ReceiveBufferPointer, r14	;
										; [10]
										;
										; ---------------------------------------------------------------------------
										; Third Bit:	(2)	 Run-in	with preparations
										; ---------------------------------------------------------------------------
	nrzi_r10							; [1-5]
	push	r15							; [6]  Pointer to write	reveived data, has to be initialized.
										; [7]  
										; [8] 
	mov		@r14, r15					; [9]  Second step of fetching buffer pointer
										; [10] 
										;
										; ---------------------------------------------------------------------------
										; Fourth Bit:	(3)	 Run-in	with preparations
										; ---------------------------------------------------------------------------
	nrzi_r11							; [1-5]
	push	r8							; [6]  Stopfmaskenregister secure
										; [7]
										; [8]
	mov		#0xfc,	r8				; [9]  Prepare mask to quickly detect last 6 bits for stuffing
										; [10]
										;
										; ---------------------------------------------------------------------------
										; Fifth	Bit: 	(4)	 Skip into regulare scheme
										; ---------------------------------------------------------------------------
	nrzi_r10							; [1-5]
										; [6-7]	Jump into regular cycle
	jmp		Mittendrin_im_fuenften_Bit	;
										;
										;
USB_ReceiverLoop:						;
										; ---------------------------------------------------------------------------
										; First Bit: (0)  Special task: Clear stuffmask, invert data
										; ---------------------------------------------------------------------------
	nrzi_r10							; [1-5]
	clr		r13							; [8] Clear	stuffing mask. Relevant	information	is still in	r12	and	gets shifted out by	the	way.
	inv		r14							; [9] Invert saved data	byte.
	unstuff	r10, 1						; [8-10]
										;
										; ---------------------------------------------------------------------------
										; Second Bit: (1)	Special task: Detect SE0
										; ---------------------------------------------------------------------------
	mov.b	@r9, r11					; [1]  NRZI	einleiten mit Sampling
										; [2]	<--- Sampling
										; [s3]  Check for SE0 
	and		#usbplus+usbminus, r11		;
										; [s4]
	jz		usb_se0						; [s5]  Which occours as low low in both USB lines
										; [s6]	Unfortunately this	takes whole	4 cycles...
	xor		r11, r10					; [7] 
	and		#usbminus, r10				; [8]   NRZI go through
	rrc.b	r12							; [9]
	bit		r8,	r12						; [10]	Stopferkennung, 6 bits in succession, no change!
										;
										; ---------------------------------------------------------------------------
										; Third Bit: (2)	Special task: Hurry up !
										; ---------------------------------------------------------------------------
	mov.b	@r9, r10					; [1]  NRZI to initiate sampling
										; [2]  <---	Sampling flags are retained!
	unstuff_hurry_up 2					; [3-4]
	xor		r10, r11					; [5]
	and		#usbminus, r11				; [6]  NRZI	2 Clocks go through later. New sampling in unclogging!
	rrc.b	r12							; [7]
	unstuff	r10, 4						; [8-10]
										;
										; ---------------------------------------------------------------------------
										; Fourth Bit: (3)	Special task: Write data into buffer
										; ---------------------------------------------------------------------------
	nrzi_r11							; [1-5]
	mov.b	r14, 0(r15)					; [p6]	and	put	freshly	prepared data into.
										; [p7]
										; [p8]
										; [p9]
	bit		r8,	r12						; [10]	Stopferkennung, 6 bits in succession, no change!
										;
										; ---------------------------------------------------------------------------
										; Fifth Bit:	(4)	 Special task: Hurry up	!
										; ---------------------------------------------------------------------------
	mov.b	@r9, r10					; [1]  NRZI	Initiate with sampling
										; [2]  <---	Sampling flags are retained!
	unstuff_hurry_up 8					; [3-4]
	xor		r10, r11					; [5]
	and		#usbminus, r11				; [6]  NRZI	2 Bars later perform. New sampling in unclogging!
	rrc.b	r12							; [7]
Mittendrin_im_fuenften_Bit:				;
	unstuff	r10, 16						; [8-10]
										;
										; ---------------------------------------------------------------------------
										; Sixth Bit:	(5)	 Pointer further Racken
										; ---------------------------------------------------------------------------
	nrzi_r11							; [1-5]
	inc		r15							; [6]
	nop									; [7]
	unstuff	r11, 32						; [8-10]
										;
										; ---------------------------------------------------------------------------
										; Seventh Bit: 	(6)	 Free of special tasks
										; ---------------------------------------------------------------------------
	nrzi_r10							; [1-5]
	nop2								; [6-7]
	unstuff	r10, 64						; [8-10]
										;
										; ---------------------------------------------------------------------------
										; Eigth Bit: (7)   Special task: Save data for later preparation and apply stuffmask.
										;					Jump back with stuffing-jump to save cycles !	 
										; ---------------------------------------------------------------------------
	nrzi_r11							; [1-5]
	mov		r12, r14					; [6] Data byte is ready! Fetch it.  Backup of Data
	bic		r13, r14					; [7] Apply	stuffmask
	bit		r8,	r12						; [8]  Stopferkennung, 6 bits in succession, no change!						  
	jnz		USB_ReceiverLoop			; [9]  Return with stuff-detect-jump
										; [10]
										; Unstuffing for last bit.
	mov.b	@r9, r11					; [e 1]	 For the NRZI decoding change state.
										; [e 2]	 <--- Sampling	State simply read again
	and		#usbminus, r11				; [e 3]
	bis		#0x80, r12					; [e 4]	 The primary bit in data, data register is 16 bits wide and runs through!
										; [e 5]	 Set MSB in	data, this is rotated further later.  
	nop									; [e 6]	   Do not log in Stopfmaske,
	nop2								; [e 7]	   since this bit is pushed completely
										; [e 8]	   and Stopfmaske is anyway deleted.
	jmp		USB_ReceiverLoop			; [e 9]
										; [e 10]  In the last bit there	is no need to update stuffmask,	as data	byte is	already	saved,
										;		  but it is	necessary to set the bit in	data register, as stuffing continues over byte boundaries.
										;		  It will be shifted out later anyway... 
										;
										; ---------------------------------------------------------------------------
										; SE0:	End	of packet detected.
										; ---------------------------------------------------------------------------
										; ---	.include "Receiver-Common.asm"
										; Frequency	independent	parts of receiver
										; ---------------------------------------------------------------------------
										; SE0:  End of packet	detected.
										; ---------------------------------------------------------------------------
usb_se0:								; Come with [7] Here!
										;
	bic.b	#usbplus, &usbifg			; Interrupt-Pending-Flag listening immediately after SE0 and after sending.
										; The re-rise of SE0 to J triggers an interrupt.
										;
	mov.b	r14, 0(r15)					; [8] :-)  Store last Databyte in Buffer
										; [9]
										;[10]
										;
										;  [1]	  Fetch	bufferpointer
	mov		&ReceiveBufferPointer, r14	;
										;  [2]
										;  [3]
	sub		r14, r15					;  [4]	  Calculate	length of received packet
	mov.b	r15, 0(r14)					;  [5]	  Store	length like	in a counted string
										;  [6]
										;  [7]
										;
	toggle_syncled						; Finished.	Toggle syncled again, to see on	oscilloscope if	SE0	detection missed its job due to	wrong sync.
										;
										;------------------------------------------------------------------------------		 
	mov.b	1(r14),	r10					; Fetch PID of packet for further investigation.
	mov.b	2(r14),	r11					; Fetch field that may contain address
	mov.b	3(r14),	r12					; Fetch field that may contain endpoint
	bit		#0x080, r11					; Fetch	LSB	bit	of endpoint
	rlc		r12							; Rotate that bit in
	and		#0x00F, r12					; Remove CRC5 from endpoint
	bic		#0x080, r11					; Clear	Endpoint bit in	address	 
										;
	cmp		#USB_PID_IN, r10			;
	jeq		usb_in						;
	cmp		#USB_PID_OUT, r10			;
	jeq		usb_out_setup				;
	cmp		#USB_PID_SETUP,	r10			;
	jeq		usb_out_setup				;
										; Do this only if the packet was for me	and	the	receive	pointer	has
										;  been set accordingly.
	cmp		#DataPacketBuffer, &ReceiveBufferPointer
	jne		usb_pop_and_finish			;
										; This	is for me !	Accept it.
										; Switch back receive buffer
	mov		#TokenPacketBuffer,	&ReceiveBufferPointer
										;
	cmp		#USB_PID_DATA0,	r10			;
	jeq		usb_data					;
	cmp		#USB_PID_DATA1,	r10			;
	jeq		usb_data					;
										;
usb_pop_and_finish:						;
	pop		r8							;
	pop		r15							;
	pop		r14							;
	pop		r13							;
	pop		r12							;
	pop		r11							;
										; Is there already the next	packet in the line ?
	bit.b	#usbplus, &usbifg			; (Or vice versa, in order to save strokes?)
	jc		USB_Quick_Resync			; Catch	it immediately ! This entry	point is only possible if r9 is	not	changed.
										; 
	pop		r10							;
	pop		r9							;
										; Next packet now in line ?
	bit.b	#usbplus, &usbifg			; Or vice versa, in order to save strokes?)
	jc		USB_Sync					; If yes, catch it. Else handle it with another interrupt. 
	reti								; 
										;
										; Next paket may follow	in 2 bittimes =	20 cycles.
										; If I have	to reply, I	have 6.5 to	7.5	bittimes maximum until I have to transmit my answer.
										; So I should check	first if I have	to receive some	more...
										; Note that	we can take	up some	time of	the	sync pattern, as we	need only the second half "0100" of	it...
										;
										;
usb_in:									;
	cmp.b	r11, &CurrentAddress		; For me ?
	jne		usb_pop_and_finish			;
										;
	cmp		#1,	r12						; Check endpoint !
	jne		j10							;
	mov		&ReadyForTransmitIrqIn, r15	; Fetch	pointer	for	outgoing packets for Irq-In	endpoint
	clr		&ReadyForTransmitIrqIn		; Buffer ready for refill
	jmp		j11							;
										;
j10: mov	&ReadyForTransmit, r15		; Fetch pointer for outgoing packets
	clr		&ReadyForTransmit			; Buffer ready for refill
										;
j11: cmp	#0,	r15						; Something inside	?
	jne		j12							;
	.if CRC_STYLE						;		
	mov		#usb_packet_nak, r15		; If not, send NAK
	br		#USB_Transmit				;
	.else								;
	mov		#(USB_PID_NAK << 8) ^ 0xFF7F, R14
	jmp		ack_nak						;
	.endif								;
										;
j12:									;
										; Address changes are committed	after Status Stage.	Do this	here !
	tst		0(R15)						; Stall?
	jz		usb_stall					;										
	mov		&NewAddress, &CurrentAddress ;
	br		#USB_Transmit				;
										;
										;
usb_stall:								;
	.if CRC_STYLE						;		
	mov		#usb_packet_stall, r15		; Send STALL
	br		#USB_Transmit				;
	.else								;
	mov		#(USB_PID_STALL << 8) ^ 0xFF7F, R14
	jmp		ack_nak						;
	.endif								;
										
usb_out_setup:							;
	cmp.b	r11, &CurrentAddress		; For me ?
	jne		usb_pop_and_finish			;
										;
										; Save PID
	mov.b	r10, &DataPacketBuffer + 15	;
										; Get ready	to receive data	with destination "me"
	mov		#DataPacketBuffer, &ReceiveBufferPointer
	jmp		usb_pop_and_finish			;
										;
usb_data:								;
	.if CRC_STYLE						;
	mov		#usb_packet_ack, r15		;
	br		#USB_Transmit				;
	.else								;	
	mov		#(USB_PID_ACK << 8) ^ 0xFF7F, R14
ack_nak:								;
	clr		R15							;
	br		#USB_Transmit				;
	.endif								;

.global	__isr_0
.global	__isr_1
.global	__isr_3
.global	__isr_4
.global	__isr_5
.global	__isr_6
.global	__isr_7
.global	__isr_8
.global	__isr_9
.global	__isr_10
.global	__isr_11
.global	__isr_12
.global	__isr_13
.global	__isr_14
__isr_0:
__isr_1:
__isr_3:
__isr_4:
__isr_5:
__isr_6:
__isr_7:
__isr_8:
__isr_9:
__isr_10:
__isr_11:
__isr_12:
__isr_13:
__isr_14:

common_return:
	reti
										;
	.sect ".int02"						;
	.short	USB_Sync					; 03: 0FFE4	 Port 1
										;
	.end								;
