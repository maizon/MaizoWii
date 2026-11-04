# MAIZO Wii portable GX frontend with isolated MEM2 DOL bootstrap
ifndef DEVKITPPC
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules

SOURCES := $(wildcard source/*.c)
OBJECTS := $(patsubst source/%.c,build/%.o,$(SOURCES)) build/bootstub_bytes.o
CFLAGS := -O2 -g -Wall -Wextra -Wno-unused-parameter $(MACHDEP) -Iinclude -I$(LIBOGC_INC)
LDFLAGS := -g $(MACHDEP) -Wl,-Map,boot.map -L$(LIBOGC_LIB)
LIBS := -lfat -lwiiuse -lbte -logc -lm
STUBFLAGS := $(MACHDEP) -O2 -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -mno-eabi -mno-sdata -mcpu=750 -I$(LIBOGC_INC)

.PHONY: all clean
all: boot.dol
build:
	mkdir -p build

build/%.o: source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/stub_entry.o: bootstub/start.S | build
	$(CC) $(STUBFLAGS) -c $< -o $@
build/stub_main.o: bootstub/stub.c | build
	$(CC) $(STUBFLAGS) -c $< -o $@
build/bootstub.elf: build/stub_entry.o build/stub_main.o bootstub/link.ld
	$(CC) -nostdlib -nostartfiles -mno-eabi -mno-sdata -mcpu=750 -Wl,-T,bootstub/link.ld -Wl,--build-id=none build/stub_entry.o build/stub_main.o -o $@
build/bootstub.bin: build/bootstub.elf
	powerpc-eabi-objcopy -O binary $< $@
build/bootstub_bytes.c: build/bootstub.bin tools/embed_bootstub.py
	python3 tools/embed_bootstub.py $< $@
build/bootstub_bytes.o: build/bootstub_bytes.c
	$(CC) $(CFLAGS) -c $< -o $@

boot.elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
boot.dol: boot.elf
	elf2dol $< $@
clean:
	rm -rf build boot.elf boot.dol boot.map
