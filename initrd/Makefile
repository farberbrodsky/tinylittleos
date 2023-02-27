OBJECTS = hello.txt world.txt foo/bar.txt splash.txt shell.elf
CPPFLAGS = -m32 -std=gnu99 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti -O2 -fno-plt -fno-pic -I.. -I/usr/include -static -nostdlib
CC = i686-linux-gnu-gcc-10

AS = nasm
ASFLAGS = -f elf

.PHONY: clean

%.elf: %.c
	$(CC) $(CPPFLAGS) $< -o $@

all: initrd.tar

initrd.tar: $(OBJECTS)
	tar -H ustar -cvf initrd.tar $(OBJECTS)

clean:
	rm -rf initrd.tar *.o shell.elf
