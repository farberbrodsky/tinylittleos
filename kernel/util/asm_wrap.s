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

; asm_lidt_and_sti - Loads the interrupt descriptor table (IDT), and set interrupts
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
global asm_lidt_and_sti
asm_lidt_and_sti:
    mov eax, [esp + 4]
    lidt [eax]
    sti
    ret
