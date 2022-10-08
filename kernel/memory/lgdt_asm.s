global lgdt

; lgdt
; stack: [esp + 4] the gdt descriptor address
;        [esp    ] return address
lgdt:
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
