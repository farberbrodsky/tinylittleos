ENTRY(loader)                /* the name of the entry label */

SECTIONS {
    . = 0x00100000;          /* everything should be as if it is at 3GB + 1 MB, except for multiboot */

    .multiboot.data : {
        *(.multiboot.data)
    }

    .loader : {
        *(.loader)
    }

    . += 0xC0000000;
    _kernel_start = .;       /* for use in later paging */

    .text ALIGN (0x1000)   : AT (ADDR (.text) - 0xC0000000) {
        *(.text)             /* all text sections from all files */
    }

    .rodata ALIGN (0x1000) : AT (ADDR (.rodata) - 0xC0000000) {
        *(.rodata*)          /* all read-only data sections from all files */
    }

    .data ALIGN (0x1000)   : AT (ADDR (.data) - 0xC0000000) {
        *(.data)             /* all data sections from all files */
    }

    .bss ALIGN (0x1000)    : AT (ADDR (.bss) - 0xC0000000) {
        *(COMMON)            /* all COMMON sections from all files */
        *(.bss)              /* all bss sections from all files */
    }

    _kernel_end = .;         /* for use in later paging */
}
