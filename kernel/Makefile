OBJECTS = loader.o crti.o util/str_util.o util/cstr.o util/kassert.o util/asm_wrap.o util/ds/hashtable.o util/ds/refcount.o tty.o serial.o memory/gdt.o memory/multiboot.o memory/page_allocator.o interrupts/init.o interrupts/interrupt_handlers.o interrupts/pic.o devices/keyboard.o scheduler/init.o scheduler/elf.o fs/vfs.o fs/tar.o memory/virtual_memory.o initrd.o
CPPFLAGS = -m32 -std=c++20 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti -O1 -fno-plt -fno-pic -fno-omit-frame-pointer -I.. -I/usr/include
CC = gcc
ifndef testname
OBJECTS := $(OBJECTS) kmain.o crtn.o
else
OBJECTS := $(OBJECTS) tests/$(testname).o crtn.o
CPPFLAGS := $(CPPFLAGS) -DTINY_TEST
endif

AS = nasm
ASFLAGS = -f elf

.PHONY: clean

all: kernel.elf

kernel.elf: $(OBJECTS) link.ld
	ld -T link.ld -melf_i386 $(OBJECTS) -o kernel.elf

crtbegin.o crtend.o:
	OBJ=`$(CC) $(CPPFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

%.o: %.cpp
	$(CC) -c $(CPPFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS)  $< -o $@

initrd.o: initrd.s ../initrd/initrd.tar
	$(AS) $(ASFLAGS)  $< -o $@

clean:
	rm -rf *.o **/*.o **/**/*.o kernel.elf
