ENTRY(reset)
SEARCH_DIR(.)
MEMORY
{
        ram : ORIGIN = 0, LENGTH = 16*1024 - 256
}
SECTIONS
{
    .reset :
    {
        *(.reset)
    } > ram
    .text :
    {
        _dst_beg = .;
        *(.text .stub .text.* .gnu.linkonce.t.*)
    } > ram
    .data ALIGN(0x4) :
    {
        *(.data .rodata .rodata.* .ctors .dtors .gnu.linkonce.r.* .gcc_except_table)
        *(COMMON)
        . = ALIGN(0x4);
        _dst_end = .;
    } > ram
    .bss ALIGN(0x4) :
    {
        _bss_start = .;
        *(.bss .sbss)
        *(COMMON)
        _bss_end = .;
    } > ram
    .stack ALIGN(0x10) (NOLOAD) :
    {
        . += 1K;
        _stack = .;
        __end = .;
    } > ram
}
