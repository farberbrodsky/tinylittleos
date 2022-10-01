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
	qemu-system-i386 -m 64M -cdrom os.iso

clean:
	$(MAKE) clean -C kernel
	rm -f iso/boot/kernel.elf
	rm -f os.iso
