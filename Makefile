.PHONY: iso kernel initrd qemu qemu-gdb qemu-kernel qemu-test qemu-test-all clean

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

initrd:
	$(MAKE) -C initrd

kernel: initrd
	$(MAKE) -C kernel testname=$(testname)

qemu: iso
	qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -cdrom os.iso

qemu-gdb: iso
	qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -cdrom os.iso -gdb tcp::1337

qemu-kernel: iso
	qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -kernel kernel/kernel.elf

qemu-test: iso
	# qemu-system-i386 -m 64M -chardev file,id=logfile,path=log.txt -serial chardev:logfile -cdrom os.iso -display none
	python3 scripts/test_runner.py

qemu-test-all:
	$(MAKE) qemu-test testname=test_filesystem
	$(MAKE) qemu-test testname=test_data_structures
	$(MAKE) qemu-test testname=test_page_allocator
	echo "All tests passed!"

clean:
	$(MAKE) clean -C kernel
	$(MAKE) clean -C initrd
	rm -f iso/boot/kernel.elf
	rm -f os.iso
