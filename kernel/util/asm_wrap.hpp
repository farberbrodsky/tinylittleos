#pragma once

extern "C" {
    void asm_outb(unsigned short port, unsigned char data);
    unsigned char asm_inb(unsigned short port);
    void asm_lgdt(void *addr);
    void asm_lldt(void *addr);
    void asm_lidt(void *addr);
    void asm_set_cr3(void *addr);
    void asm_flush_tss();
    void asm_enter_usermode(void *func, void *esp);
}
