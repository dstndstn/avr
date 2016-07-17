#  avrdude -p m168 -c bsd -U flash:w:charger.hex

# To hit RESET:
#  avrdude -p m168 -c bsd -E noreset

# 4-line LCD:
#    00...19
#    40...59
#    20...39
#    60...79

PRG            := charger
OBJ            := charger.o lcd.o
MCU_TARGET     := atmega168
OPTIMIZE       := -O2

#DEFS           := -DF_CPU=1000000UL
# 12 MHz crystal
DEFS           := -DF_CPU=12000000UL
LIBS           :=

# You should not have to change anything below here.
# (other than dependencies)

CC             := avr-gcc

# Override is only needed by avr-lib build system.

CFLAGS        := -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
LDFLAGS       := -Wl,-Map,$(PRG).map

# Use minimal vfprintf implementation.
#LDFLAGS += -Wl,-u,vfprintf -lprintf_min
# Use full floating-point vfprintf implementation.
LDFLAGS += -Wl,-u,vfprintf -lprintf_flt -lm
# Use minimal vscanf implementation.
LDFLAGS += -Wl,-u,vfscanf -lscanf_min -lm

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

#all: $(PRG).elf lst text eeprom

all: main.hex charger.hex

main.elf: main.o lcd.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(PRG).elf: $(OBJ)
	echo $(LDFLAGS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# dependency:
main.o: main.c iocompat.h

charger.o: charger.c iocompat.h

clean:
	rm -rf *.o $(PRG).elf *.eps *.png *.pdf *.bak 
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@ \
	|| { echo empty $@ not generated; exit 0; }

# Every thing below here is used by avr-libc's build system and can be ignored
# by the casual user.

FIG2DEV                 = fig2dev
EXTRA_CLEAN_FILES       = *.hex *.bin *.srec

dox: eps png pdf

eps: $(PRG).eps
png: $(PRG).png
pdf: $(PRG).pdf

%.eps: %.fig
	$(FIG2DEV) -L eps $< $@

%.pdf: %.fig
	$(FIG2DEV) -L pdf $< $@

%.png: %.fig
	$(FIG2DEV) -L png $< $@

