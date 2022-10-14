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

align 4096
bootstrap_page_directory:
    resb 4096

section .text
loader:
    ; bootstrap paging, the easy way: a single page directory with one 4MB huge page at 0xC0000000 and 4MB huge page at 0
    mov ecx, 0x83 ; Page Size (4MB) (1), Not Dirty (0), Not Accessed (0), Cached (0),
                  ; Write-Back (0), Supervisor (0), Write (1), Present (1)
    mov [bootstrap_page_directory - 0xC0000000 +    0], ecx
    mov [bootstrap_page_directory - 0xC0000000 + 3072], ecx

    ; enable paging with this page directory
    mov ecx, bootstrap_page_directory - 0xC0000000
    mov cr3, ecx
    mov ecx, cr4        ; read current cr4
    or  ecx, 0x00000010 ; set PSE
    mov cr4, ecx        ; update cr4
    mov ecx, cr0        ; read current cr0
    or  ecx, 0x80000000 ; set PG
    mov cr0, ecx        ; update cr0

    ; call kmain with multiboot data and with a stack
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    cld
    extern kmain
    push eax  ; multiboot magic
    push ebx  ; multiboot data
    call kmain
.loop:
    hlt
    jmp .loop
