#pragma once

extern "C" {
    void asm_outb(unsigned short port, unsigned char data);
    unsigned char asm_inb(unsigned short port);
    void asm_lgdt(void *addr);
    void asm_lldt(void *addr);
    void asm_lidt(void *addr);
}
