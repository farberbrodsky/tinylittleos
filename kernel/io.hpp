#pragma once

extern "C" {
    void outb(unsigned short port, unsigned char data);
    unsigned char inb(unsigned short port);
}
