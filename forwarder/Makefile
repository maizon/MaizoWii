# Standalone MAIZO HBC forwarder; requires the root bootstub object built first.
ifndef DEVKITPPC
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules

CFLAGS := -O2 -Wall -Wextra -Werror $(MACHDEP) -I../include -I$(LIBOGC_INC)
LDFLAGS := $(MACHDEP) -L$(LIBOGC_LIB)
LIBS := -lfat -lwiiuse -lbte -logc -lm
.PHONY: all clean
all: boot.dol
build:
	mkdir -p build
build/main.o: main.c | build
	$(CC) $(CFLAGS) -c $< -o $@
../build/bootstub_bytes.o:
	$(MAKE) -C .. build/bootstub_bytes.o
boot.elf: build/main.o ../build/bootstub_bytes.o
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@
boot.dol: boot.elf
	elf2dol $< $@
clean:
	rm -rf build boot.elf boot.dol
