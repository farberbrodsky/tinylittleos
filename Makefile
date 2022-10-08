.PHONY: iso kernel qemu clean

iso: kernel
	cp kernel/kernel.elf iso/boot/kernel.elf
	genisoimage -R                              \
				-b boot/grub/stage2_eltorito    \
				-no-emul-boot                   \
				-boot-load-size 4               \
				-A os                           \
				-input-charset utf8             \
				-quiet                          \
				-boot-info-table                \
				-o os.iso                       \
				iso

kernel:
	$(MAKE) -C kernel

qemu: iso
	qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -cdrom os.iso

qemu-gdb: iso
	qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -cdrom os.iso -gdb tcp::1337

clean:
	$(MAKE) clean -C kernel
	rm -f iso/boot/kernel.elf
	rm -f os.iso
