OBJECTS = loader.o crti.o kmain.o util/str_util.o util/cstr.o util/kassert.o tty.o crtn.o io.o serial.o
CC = gcc
CPPFLAGS = -m32 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti \
-I.. -I/usr/include

LDFLAGS = -T link.ld -melf_i386
AS = nasm
ASFLAGS = -f elf

.PHONY: clean

all: kernel.elf

kernel.elf: $(OBJECTS) link.ld
	ld $(LDFLAGS) $(OBJECTS) -o kernel.elf

crtbegin.o crtend.o:
	OBJ=`$(CC) $(CPPFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

%.o: %.cpp
	$(CC) -c $(CPPFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS)  $< -o $@

clean:
	rm -rf *.o **/*.o kernel.elf
