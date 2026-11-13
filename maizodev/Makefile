ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC not set)
endif
include $(DEVKITPPC)/wii_rules
TARGET := boot
BUILD := build
SOURCES := source
INCLUDES :=
CFLAGS := -O2 -Wall -Wextra $(MACHDEP) -I$(LIBOGC_INC)
LDFLAGS := $(MACHDEP) -L$(LIBOGC_LIB)
LIBS := -lwiiuse -lbte -logc -lm
all: boot.dol
boot.elf: source/main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) $(LIBS) -o $@
boot.dol: boot.elf
	elf2dol $< $@
clean:
	rm -f boot.elf boot.dol
