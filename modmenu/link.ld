ENTRY(hook)
SECTIONS {
  . = 0x000C0000;
  .text : { *(.text.hook) *(.text*) }
  .rodata : { *(.rodata*) *(.data*) *(.sdata*) }
  . = 0x000E0000;
  .bss (NOLOAD) : { *(.bss*) *(.sbss*) *(COMMON) }
  /DISCARD/ : { *(.MIPS.*) *(.reginfo) *(.comment) *(.pdr) *(.gnu*) }
}
