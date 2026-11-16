ENTRY(hubgrid)
SECTIONS { .text 0x0012BED8 : SUBALIGN(4) { *(.text*) } /DISCARD/ : { *(.MIPS.*) *(.reginfo) *(.pdr) *(.gnu*) *(.got) *(.data) *(.bss) } }
