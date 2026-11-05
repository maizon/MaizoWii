# MAIZO forwarder is a port of OSC's proven FIX94 handoff.
# The original 872-byte app_booter.bin is embedded without byte modifications.
ifndef DEVKITPPC
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules

CFLAGS := -Os -Wall -Wextra -Werror $(MACHDEP) -I$(LIBOGC_INC)
LDFLAGS := $(MACHDEP) -L$(LIBOGC_LIB)
LIBS := -lfat -lwiiuse -lbte -logc -lm

.PHONY: all clean
all: boot.dol
build:
	mkdir -p build

build/main.o: main.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/osc_app_booter.c: data/app_booter.bin ../tools/embed_osc_booter.py | build
	python3 ../tools/embed_osc_booter.py $< $@
build/osc_app_booter.o: build/osc_app_booter.c | build
	$(CC) $(CFLAGS) -c $< -o $@

boot.elf: build/main.o build/osc_app_booter.o
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@
boot.dol: boot.elf
	elf2dol $< $@
clean:
	rm -rf build boot.elf boot.dol
