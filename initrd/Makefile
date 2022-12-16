OBJECTS = hello.txt world.txt foo/bar.txt splash.txt
CPPFLAGS = -m32 -std=c++20 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti -O2 -fno-plt -fno-pic -I.. -I/usr/include
CC = gcc

AS = nasm
ASFLAGS = -f elf

.PHONY: clean

all: initrd.tar

initrd.tar: $(OBJECTS)
	tar -H ustar -cvf initrd.tar $(OBJECTS)

clean:
	rm -rf initrd.tar
