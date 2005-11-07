ENTRY(start)
addr = 0x100000;

SECTIONS
{
	.text addr : AT(addr)
	{
		_text = . ;
		*(.text)
		*(.rodata)
		. = ALIGN(4096);
	}
	.data : AT(addr + (_data - _text))
	{
		_data = . ;
		*(.data)
		. = ALIGN(4096);
		_bss = . ;
		*(.bss)
		*(COMMON)
		. = ALIGN(4096);
		_end = . ;
	}
	/DISCARD/ :
	{
		*(.ctors)
	}
}
