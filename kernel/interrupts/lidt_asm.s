global load_idt

; lidt - Loads the interrupt descriptor table (IDT), and set interrupts
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    sti
    ret
