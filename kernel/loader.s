global loader                   ; the entry symbol for ELF

MAGIC_NUMBER equ 0x1BADB002     ; define the magic number constant
FLAGS        equ 0x0            ; multiboot flags
CHECKSUM     equ -MAGIC_NUMBER  ; calculate the checksum
                                ; (magic number + checksum + flags should equal 0)
KERNEL_STACK_SIZE equ 4096

section .multiboot.data
align 4
    dd MAGIC_NUMBER             ; write the magic number to the machine code,
    dd FLAGS                    ; the flags,
    dd CHECKSUM                 ; and the checksum

section .bss
align 4
kernel_stack:
    resb KERNEL_STACK_SIZE      ; reserve stack for kernel in BSS

section .text
loader:
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    cld
    extern kmain
    call kmain
.loop:
    jmp .loop
