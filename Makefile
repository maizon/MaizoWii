# Portable single-DOL frontend (modules remain external)
ifndef DEVKITPPC
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules
SOURCES := $(wildcard source/*.c)
OBJECTS := $(patsubst source/%.c,build/%.o,$(SOURCES))
CFLAGS := -O2 -g -Wall -Wextra -Wno-unused-parameter $(MACHDEP) -Iinclude -I$(LIBOGC_INC)
# WiiMC forwarder (cred. crediar) relocates its loader to high MEM1.
# Do not let the next DOL overwrite the running MAIZO trampoline.
LDFLAGS := -g $(MACHDEP) -Wl,-Map,boot.map -Wl,--section-start,.init=0x81230000 -L$(LIBOGC_LIB)
LIBS := -lfat -lwiiuse -lbte -logc -lm
.PHONY: all clean
all: boot.dol
build:
	mkdir -p build
build/%.o: source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
boot.elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
boot.dol: boot.elf
	elf2dol $< $@
clean:
	rm -rf build boot.elf boot.dol boot.map
