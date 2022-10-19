; asm_outb - send a byte to an I/O port
; stack: [esp + 8] the data byte
;        [esp + 4] the I/O port
;        [esp    ] return address
global asm_outb
asm_outb:
    mov al, [esp + 8]    ; move the data to be sent into the al register
    mov dx, [esp + 4]    ; move the address of the I/O port into the dx register
    out dx, al           ; send the data to the I/O port
    ret                  ; return to the calling function

; asm_inb - returns a byte from the given I/O port
; stack: [esp + 4] The address of the I/O port
;        [esp    ] The return address
global asm_inb
asm_inb:
    mov dx, [esp + 4]    ; move the address of the I/O port to the dx register
    in  al, dx           ; read a byte from the I/O port and store it in the al register
    ret                  ; return the read byte

; asm_lgdt - sets GDT and sets all segment selectors
; stack: [esp + 4] the gdt descriptor address
;        [esp    ] return address
global asm_lgdt
asm_lgdt:
    mov eax, [esp + 4]
    lgdt [eax]
    jmp 0x08:flush_cs
flush_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    ret

; asm_lldt - sets LDT (usually to 0)
; stack: [esp + 4] the gdt descriptor address
;        [esp    ] return address
global asm_lldt
asm_lldt:
    mov eax, [esp + 4]
    lldt [eax]
    ret

; asm_lidt - Loads the interrupt descriptor table (IDT), and set interrupts
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
global asm_lidt
asm_lidt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; asm_set_cr3 - sets cr3 (page directory register) to a new value
; stack: [esp + 4] the address of the page directory
;        [esp    ] the return address
global asm_set_cr3
asm_set_cr3:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

; asm_invlpg - invalidate page at address
; stack: [esp + 4] the address of the page
;        [esp    ] the return address
global asm_invlpg
asm_invlpg:
    mov eax, [esp + 4]
    invlpg [eax]
    ret

; asm_flush_tss - flush TSS, no arguments
global asm_flush_tss
asm_flush_tss:
    mov ax, 0x28
    ltr ax
    ret

; asm_enter_usermode - enter usermode with iret
; stack: [esp + 8] stack address to use
;        [esp + 4] function address to call
;        [esp    ] the return address
;
; an inter-privilege level interrupt needs to have a stack of:
; - ss, esp, eflags, cs, eip
global asm_enter_usermode
asm_enter_usermode:
    mov ebx, [esp + 4]
    mov ecx, [esp + 8]
    mov ax, 0x20 | 3  ; ring 3 - set by bottom bits of segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push 0x20 | 3  ; ss
    push ecx;      ; esp
    pushf          ; using the current EFLAGS
    push 0x18 | 3  ; cs
    push ebx       ; eip

    ; reset all registers to 0
    mov eax, 0
    mov ebx, 0
    mov ecx, 0
    mov edx, 0
    mov esi, 0
    mov edi, 0
    mov ebp, 0

    iret
