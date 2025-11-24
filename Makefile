CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

SRC = main.c startup_n32g031_gcc.s \
      system_n32g031.c \
      n32g031_rcc.c \
      n32g031_usart.c \
      n32g031_gpio.c \
	  n32g031_adc.c \
      n32g031_flash.c

TARGET = main
OUTDIR = build
ELF = $(OUTDIR)/$(TARGET).elf
HEX = $(OUTDIR)/$(TARGET).hex

CFLAGS = -mcpu=cortex-m0 -mthumb -O0 -g -Wall -I. -IN32_SDK -ICMSIS/Core/Include
#LDFLAGS = -Tn32g031_flash.ld -nostartfiles
LDFLAGS = -Tn32g031_flash.ld -Wl,--gc-sections
LIBS = -lc -lm -lnosys


all: $(HEX)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(ELF): $(SRC) | $(OUTDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@
	$(SIZE) $<

#OPENOCD = /C/openocd-v0.12/bin/openocd.exe -f interface/cmsis-dap.cfg -c "transport select swd" -c "init"

OPENOCD = /C/openocd-v0.12/bin/openocd.exe \
  -f interface/cmsis-dap.cfg \
  -f n32g03x.cfg \
   -c "adapter speed 400" \
  -c "program build/main.elf verify reset exit"

flash:
	$(OPENOCD) 


clean:
	rm -rf $(OUTDIR)
