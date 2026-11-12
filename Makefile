# MAIZO Wii portable GX frontend with isolated MEM2 DOL bootstrap
ifndef DEVKITPPC
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules

SOURCES := $(wildcard source/*.c)
OBJECTS := $(patsubst source/%.c,build/%.o,$(SOURCES)) build/bootstub_bytes.o build/ca_bundle.o build/maizonet_smb1.o build/maizonet_udp.o
CFLAGS := -O2 -g -Wall -Wextra -Wno-unused-parameter $(MACHDEP) -Iinclude -I$(LIBOGC_INC) -I$(DEVKITPRO)/portlibs/wii/include -I$(DEVKITPRO)/portlibs/ppc/include -I$(DEVKITPRO)/portlibs/ppc/include/libpng16
# Old wii-curl libwiisocket duplicates inet_ntop/inet_pton already provided by
# libogc 3.2. Keep legacy first definition to satisfy its dependent archive.
LDFLAGS := -g $(MACHDEP) -Wl,-Map,boot.map -Wl,--allow-multiple-definition -L$(DEVKITPRO)/portlibs/wii/lib -L$(DEVKITPRO)/portlibs/ppc/lib -L$(LIBOGC_LIB)
LIBS := -lcurl -lpng -lz -lmbedtls -lmbedcrypto -lmbedx509 -lwiisocket -lfat -lwiiuse -lbte -logc -lm
STUBFLAGS := $(MACHDEP) -O2 -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -mno-eabi -mno-sdata -mcpu=750 -I$(LIBOGC_INC)

.PHONY: all clean
all: boot.dol
build:
	mkdir -p build

build/%.o: source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/maizonet_smb1.o: integrations/maizonet_smb1.c integrations/maizonet_smb1.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/maizonet_udp.o: integrations/maizonet_udp.c integrations/maizonet_udp.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/ca_bundle.o: source/ca_bundle.S data/cacert.pem | build
	$(CC) $(MACHDEP) -c $< -o $@
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
