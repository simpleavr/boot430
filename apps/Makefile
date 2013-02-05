 
MMCU=msp430g2553
MMCU=msp430g2432

CC=msp430-gcc
CFLAGS=-Os -Wall -mmcu=$(MMCU) -ffunction-sections -fdata-sections -fno-inline-small-functions --cref -Wl,--relax

TARGETS = rgb.hex segled.hex

all: $(TARGETS)

%.hex: %.elf
	msp430-objcopy -O ihex $< $@

%.elf: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -fr *.elf *.lst *.hex *.o
