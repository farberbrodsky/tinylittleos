int main() {
    unsigned syscall = 0x1234;
    asm volatile("int $0x80" : "+a"(syscall));
    return 0;
}
