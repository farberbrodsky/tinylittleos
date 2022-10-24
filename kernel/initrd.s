section .data
global  _initrd
align 512
_initrd:
    incbin "../initrd/initrd.tar"
