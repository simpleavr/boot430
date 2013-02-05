 
MMCU=msp430g2553
MMCU=msp430g2432
PRG=boot430

CC=msp430-gcc
CFLAGS=-Os -Wall -mmcu=$(MMCU) -ffunction-sections -fdata-sections -fno-inline-small-functions --cref -Wl,--relax -Wl,-Ttext=0xf000
AFLAGS=-Wa,--gstabs -Wall -mmcu=$(MMCU) -x assembler-with-cpp

OBJS=$(PRG).o bbusb_packet.o bbusb_protocol.o bbusb_receive.o bbusb_transmit.o

all: $(OBJS)
	$(CC) $(CFLAGS) -o $(PRG).elf $(OBJS)

size:
	msp430-size --totals $(PRG).elf

hex: $(PRG).elf
	msp430-objcopy -O ihex $(PRG).elf $(PRG).hex

lst: $(PRG).elf
	msp430-objdump -DS $(PRG).elf > $(PRG).lst

flash:
	mspdebug rf2500 "prog $(PRG).elf"

%.o: %.S
	$(CC) $(AFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr $(PRG).elf $(PRG).lst $(PRG).hex $(OBJS)
